// DEADNOISE - STFT spectral noise suppressor.
//
// Signal model: Y = S + N in every STFT bin. Per frame:
//   1. Noise power estimate
//        - learned profile (robust, captured during "Learn Background"),
//          scaled by the current input gain and slowly level-corrected per band
//        - otherwise an SPP-driven MMSE noise tracker (Gerkmann & Hendriks 2012),
//          which keeps adapting during speech without learning the speech itself
//   2. Decision-directed a-priori SNR (Ephraim & Malah)
//   3. MMSE log-spectral-amplitude gain (Ephraim & Malah 1985)
//   4. Speech presence probability with IMCRA-style local/global/frame
//      a-priori absence estimation (Cohen 2003) -> OM-LSA gain
//        G = G_H1^p * G_min^(1-p)
//      This keeps suppressing noise in speech-free bins *during* speech and
//      prevents isolated noise peaks (musical noise) from opening the gain.
//   5. Per-bin release smoothing (protects word endings / decays)
// Analysis and synthesis: periodic Hann, 75 % overlap, linked across channels.
#pragma once

#include "RealFFT.h"
#include "NoiseProfile.h"

#include <atomic>
#include <mutex>
#include <vector>
#include <complex>
#include <limits>

namespace deadnoise
{

// Suppressor tuning. Each mode has defaults; voice preservation shifts them.
struct SuppressorTuning
{
    float gMinDb;          // OM-LSA floor (speech-absence gain)
    float overSub;         // noise over-estimation factor
    float xiMin;           // a-priori SNR floor (linear)
    float alphaDD;         // decision-directed smoothing
    float zetaLoDb, zetaHiDb;    // local/global speech presence thresholds
    float frameLoDb, frameHiDb;  // frame-level speech presence thresholds
    float qMax;            // max a-priori speech absence probability
    float holdSeconds;     // frame speech-presence hold
    float releaseSeconds;  // per-bin gain release
    float deepFloorDb;     // extra attenuation for confidently speech-free bins
};

// Development overrides (NaN = use the built-in value). Used by the offline
// tools for parameter sweeps; the plugin never sets these.
struct TuningOverride
{
    float v[12];
    TuningOverride() { for (auto& x : v) x = std::numeric_limits<float>::quiet_NaN(); }
};

struct DenoiserSettings
{
    float amount = 0.8f;              // 0..1  (NOISE REDUCTION)
    bool maximumMode = false;         // PROCESSING MODE
    float voicePreservation = 0.5f;   // 0..1
    float inputGain = 1.0f;           // linear gain the signal has already received
};

enum class LearnState : int
{
    idle = 0,
    requested,
    capturing,
    captureDone,
    analysing,
    succeeded,
    failedActivity,
    failedNoSignal
};

class SpectralDenoiser
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr double kLearnSeconds = 4.0;
    static constexpr int kDisplayPoints = 256;

    SpectralDenoiser();

    // ---- configuration (non-realtime thread, audio stopped)
    void prepare (double sampleRate, int numChannels);
    void reset();   // clear audio history, keep the profile

    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return numBins; }
    int getHopSize() const noexcept { return hop; }
    double getSampleRate() const noexcept { return sampleRate; }

    // ---- audio thread
    void setSettings (const DenoiserSettings& s) noexcept { settings = s; }
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // ---- learning (UI / any non-audio thread)
    void requestLearn() noexcept;
    void cancelLearn() noexcept;
    LearnState getLearnState() const noexcept { return (LearnState) learnState.load(); }
    float getLearnProgress() const noexcept { return learnProgress.load(); }
    void acknowledgeLearnResult() noexcept;   // succeeded/failed -> idle

    // Must be called periodically from a non-audio thread (e.g. every 20 ms).
    // Runs the profile analysis once a capture is complete. Returns true when
    // the stored profile changed.
    bool serviceBackgroundTasks();

    // ---- profile (non-audio threads)
    bool hasProfile() const noexcept { return profilePresent.load(); }
    NoiseProfile getProfile() const;
    void setProfile (const NoiseProfile& p);  // e.g. restored from plugin state
    void clearProfile();
    bool profileMatchesCurrentRate() const;

    // ---- metering / display (relaxed atomics; any thread)
    float getAverageReductionDb() const noexcept { return reductionDb.load (std::memory_order_relaxed); }
    float getSpeechActivity() const noexcept { return speechActivity.load (std::memory_order_relaxed); }
    // Mean deviation (dB) of the current background from the learned profile.
    // Several dB for a sustained period means the profile should be relearned.
    float getProfileDriftDb() const noexcept { return profileDrift.load (std::memory_order_relaxed); }
    // Log-spaced spectra (dB), kDisplayPoints values each, 20 Hz .. 20 kHz
    void getDisplaySpectra (float* inputDb, float* outputDb, float* noiseDb) const noexcept;

    void setTuningOverride (const TuningOverride& o) noexcept { tuningOverride = o; }
    SuppressorTuning getCurrentTuning() const noexcept { return currentModeParams(); }

private:
    using ModeParams = SuppressorTuning;
    TuningOverride tuningOverride;

    void processFrame() noexcept;
    void updateNoiseEstimate (float inputGain2) noexcept;
    void computeGains (const ModeParams& mp) noexcept;
    ModeParams currentModeParams() const noexcept;
    void installPendingProfile() noexcept;
    void captureLearnFrame (float inputGain2) noexcept;
    void updateDisplay() noexcept;
    void rebuildMapped (const NoiseProfile& p, std::vector<float>& dst) const;
    void setupBands();

    // configuration
    double sampleRate = 48000.0;
    int numChannels = 1, fftSize = 1024, hop = 256, numBins = 513;
    double frameRate = 187.5;
    float sumW2 = 1.0f;
    RealFFT fft;
    std::vector<float> window;

    // streaming buffers
    std::vector<float> inFrame[kMaxChannels];   // fftSize, newest sample last
    std::vector<float> outAccum[kMaxChannels];  // fftSize overlap-add accumulator
    std::vector<float> outReady[kMaxChannels];  // hop samples ready for output
    int hopCounter = 0;

    // per-frame scratch
    std::vector<float> timeBuf;
    std::vector<std::complex<float>> spec[kMaxChannels];
    std::vector<float> power;           // linked |Y|^2

    // noise estimation
    std::vector<float> tracker;         // SPP-MMSE noise power
    std::vector<float> trackerSmooth;   // time-smoothed tracker (adaptive mode)
    std::vector<float> trackerPBar;
    int trackerFrames = 0;
    std::vector<float> profileBins;     // learned profile, bin power at unity input gain
    std::vector<float> noise;           // final noise power used for the gain
    bool profileActive = false;
    // slow level-correction of the profile, per band
    std::vector<int> bandOfBin;
    std::vector<float> bandWeight;      // interpolation weight towards band+1
    std::vector<float> bandCorr;
    std::vector<double> bandNum, bandDen;
    int numCorrBands = 0;

    // gain computation state
    std::vector<float> a2Prev;          // previous |S_hat|^2 under H1
    std::vector<float> zeta;            // recursively smoothed a-priori SNR
    std::vector<float> xi, pArr, vArr, gH1, pLocal, pGlobal, gainPrev, gain;
    std::vector<float> globalKernel;
    int globalHalf = 9;
    int frameLoBin0 = 1, frameLoBin1 = 1, frameHiBin0 = 1, frameHiBin1 = 1;
    float pFrame = 0.0f;
    int holdCounter = 0;
    int warmupFrames = 0;

    DenoiserSettings settings;

    // learning
    std::atomic<int> learnState { (int) LearnState::idle };
    std::atomic<float> learnProgress { 0.0f };
    std::vector<float> captureBuf;      // learnFrames x numBins
    int learnFrames = 0, capturedFrames = 0;

    // profile handover (non-RT -> RT), single-slot mailbox
    enum MailboxState : int { mbEmpty = 0, mbWriting, mbFull, mbReading };
    std::atomic<int> mailbox { mbEmpty };
    std::vector<float> mailboxBins;
    bool mailboxClear = false;
    int mailboxFft = 0;

    mutable std::mutex profileMutex;    // never taken on the audio thread
    NoiseProfile storedProfile;
    std::atomic<bool> profilePresent { false };

    // display
    std::atomic<float> reductionDb { 0.0f };
    std::atomic<float> speechActivity { 0.0f };
    std::atomic<float> profileDrift { 0.0f };
    std::vector<int> dispBin0, dispBin1;
    std::vector<float> dispFrac;
    std::vector<float> dispIn, dispOut, dispNoise;   // smoothed, dB
    std::atomic<float> dispInA[kDisplayPoints], dispOutA[kDisplayPoints], dispNoiseA[kDisplayPoints];
    int displayCounter = 0;
};

} // namespace deadnoise
