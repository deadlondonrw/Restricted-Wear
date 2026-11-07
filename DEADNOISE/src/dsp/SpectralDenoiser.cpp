// DEADNOISE - STFT spectral noise suppressor (see header for the algorithm).
#include "SpectralDenoiser.h"
#include "DspMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

namespace deadnoise
{

namespace
{
    constexpr float kNoiseFloorPerSample = 1.0e-13f;   // ~ -130 dBFS noise variance
    constexpr int kTrackerInitFrames = 8;
    constexpr float kDeepP = 0.2f;   // speech presence below which the deep floor applies

    int chooseFftSize (double sr)
    {
        // ~21 ms analysis window at every sample rate
        if (sr <= 26000.0)  return 512;
        if (sr <= 52000.0)  return 1024;
        if (sr <= 104000.0) return 2048;
        return 4096;
    }
}

SpectralDenoiser::SpectralDenoiser()
{
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        dispInA[i].store (-150.0f);
        dispOutA[i].store (-150.0f);
        dispNoiseA[i].store (-150.0f);
    }
}

//==============================================================================
void SpectralDenoiser::prepare (double newSampleRate, int newNumChannels)
{
    std::lock_guard<std::mutex> lock (profileMutex);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = clampv (newNumChannels, 1, kMaxChannels);
    fftSize = chooseFftSize (sampleRate);
    hop = fftSize / 4;
    numBins = fftSize / 2 + 1;
    frameRate = sampleRate / hop;

    fft.init (fftSize);

    window.resize ((size_t) fftSize);
    sumW2 = 0.0f;
    for (int n = 0; n < fftSize; ++n)
    {
        window[(size_t) n] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * n / fftSize)); // periodic Hann
        sumW2 += window[(size_t) n] * window[(size_t) n];
    }

    for (int c = 0; c < kMaxChannels; ++c)
    {
        inFrame[c].assign ((size_t) fftSize, 0.0f);
        outAccum[c].assign ((size_t) fftSize, 0.0f);
        outReady[c].assign ((size_t) hop, 0.0f);
        spec[c].assign ((size_t) numBins, {});
    }
    timeBuf.assign ((size_t) fftSize, 0.0f);

    auto alloc = [this] (std::vector<float>& v, float init) { v.assign ((size_t) numBins, init); };
    alloc (power, 0.0f);   alloc (tracker, 0.0f); alloc (trackerPBar, 0.0f);
    alloc (profileBins, 0.0f); alloc (noise, 0.0f);
    alloc (a2Prev, 0.0f);  alloc (zeta, 0.0f);    alloc (xi, 0.0f);
    alloc (vArr, 0.0f);    alloc (gH1, 1.0f);     alloc (pLocal, 0.0f);
    alloc (pGlobal, 0.0f); alloc (gainPrev, 1.0f); alloc (gain, 1.0f);
    mailboxBins.assign ((size_t) numBins, 0.0f);

    setupBands();

    // global (wide) SNR smoothing kernel: Hann, ~ +-400 Hz
    const double binHz = sampleRate / fftSize;
    globalHalf = clampv ((int) std::round (400.0 / binHz), 3, 24);
    globalKernel.resize ((size_t) (2 * globalHalf + 1));
    {
        float s = 0.0f;
        for (int j = -globalHalf; j <= globalHalf; ++j)
        {
            const float w = 0.5f + 0.5f * (float) std::cos (kPi * j / (globalHalf + 1));
            globalKernel[(size_t) (j + globalHalf)] = w;
            s += w;
        }
        for (auto& w : globalKernel) w /= s;
    }

    // frame-level speech detection bands
    auto toBin = [&] (double hz) { return clampv ((int) std::round (hz / binHz), 1, numBins - 1); };
    frameLoBin0 = toBin (150.0);
    frameLoBin1 = std::max (frameLoBin0 + 1, toBin (4000.0));
    frameHiBin0 = frameLoBin1;
    frameHiBin1 = std::max (frameHiBin0 + 1, toBin (std::min (12000.0, sampleRate * 0.45)));

    // display mapping, log spaced 20 Hz .. 20 kHz
    dispBin0.resize (kDisplayPoints);
    dispBin1.resize (kDisplayPoints);
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        const double fLo = 20.0 * std::pow (1000.0, (i - 0.5) / (kDisplayPoints - 1));
        const double fHi = 20.0 * std::pow (1000.0, (i + 0.5) / (kDisplayPoints - 1));
        int b0 = clampv ((int) std::floor (fLo / binHz), 0, numBins - 1);
        int b1 = clampv ((int) std::ceil (fHi / binHz), b0 + 1, numBins);
        dispBin0[(size_t) i] = b0;
        dispBin1[(size_t) i] = b1;
    }
    dispIn.assign (kDisplayPoints, -150.0f);
    dispOut.assign (kDisplayPoints, -150.0f);
    dispNoise.assign (kDisplayPoints, -150.0f);

    // learning capture buffer
    learnFrames = (int) std::round (kLearnSeconds * frameRate);
    captureBuf.assign ((size_t) learnFrames * (size_t) numBins, 0.0f);
    capturedFrames = 0;
    const int ls = learnState.load();
    if (ls == (int) LearnState::requested || ls == (int) LearnState::capturing
        || ls == (int) LearnState::captureDone || ls == (int) LearnState::analysing)
        learnState.store ((int) LearnState::idle);
    learnProgress.store (0.0f);

    // profile: map the stored profile to the new grid directly (audio is stopped)
    mailbox.store (mbEmpty);
    if (storedProfile.isValid())
    {
        rebuildMapped (storedProfile, profileBins);
        profileActive = true;
    }
    else
    {
        std::fill (profileBins.begin(), profileBins.end(), 0.0f);
        profileActive = false;
    }

    reset();
}

void SpectralDenoiser::setupBands()
{
    // ~18 log-spaced correction bands, at least 4 bins wide
    const double binHz = sampleRate / fftSize;
    std::vector<int> edges { 0 };
    int first = std::max (4, (int) std::round (150.0 / binHz));
    edges.push_back (std::min (first, numBins));
    const int bandsWanted = 18;
    const double ratio = std::pow ((double) numBins / std::max (1, first), 1.0 / bandsWanted);
    double e = first;
    while (edges.back() < numBins)
    {
        e *= ratio;
        int next = std::max (edges.back() + 4, (int) std::round (e));
        if (numBins - next < 4) next = numBins;
        edges.push_back (std::min (next, numBins));
    }
    numCorrBands = (int) edges.size() - 1;

    std::vector<float> centres ((size_t) numCorrBands);
    for (int b = 0; b < numCorrBands; ++b)
        centres[(size_t) b] = 0.5f * (float) (edges[(size_t) b] + edges[(size_t) b + 1] - 1);

    bandOfBin.assign ((size_t) numBins * 2, 0);   // [member, interpLo] pairs
    bandWeight.assign ((size_t) numBins, 0.0f);
    for (int k = 0; k < numBins; ++k)
    {
        int member = 0;
        while (member < numCorrBands - 1 && k >= edges[(size_t) member + 1]) ++member;
        int lo = 0;
        while (lo < numCorrBands - 1 && centres[(size_t) lo + 1] <= (float) k) ++lo;
        float w = 0.0f;
        if (lo < numCorrBands - 1 && (float) k > centres[(size_t) lo])
            w = ((float) k - centres[(size_t) lo]) / (centres[(size_t) lo + 1] - centres[(size_t) lo]);
        bandOfBin[(size_t) k * 2] = member;
        bandOfBin[(size_t) k * 2 + 1] = lo;
        bandWeight[(size_t) k] = clampv (w, 0.0f, 1.0f);
    }
    bandCorr.assign ((size_t) numCorrBands, 1.0f);
    bandNum.assign ((size_t) numCorrBands, 0.0);
    bandDen.assign ((size_t) numCorrBands, 0.0);
}

void SpectralDenoiser::reset()
{
    for (int c = 0; c < kMaxChannels; ++c)
    {
        std::fill (inFrame[c].begin(), inFrame[c].end(), 0.0f);
        std::fill (outAccum[c].begin(), outAccum[c].end(), 0.0f);
        std::fill (outReady[c].begin(), outReady[c].end(), 0.0f);
    }
    hopCounter = 0;
    std::fill (a2Prev.begin(), a2Prev.end(), 0.0f);
    std::fill (zeta.begin(), zeta.end(), 0.0f);
    std::fill (gainPrev.begin(), gainPrev.end(), 1.0f);
    std::fill (gain.begin(), gain.end(), 1.0f);
    std::fill (trackerPBar.begin(), trackerPBar.end(), 0.0f);
    std::fill (bandCorr.begin(), bandCorr.end(), 1.0f);
    pFrame = 0.0f;
    holdCounter = 0;
    warmupFrames = 0;

    if (profileActive)
    {
        const float g2 = settings.inputGain * settings.inputGain;
        for (int k = 0; k < numBins; ++k) tracker[(size_t) k] = profileBins[(size_t) k] * g2;
        trackerFrames = kTrackerInitFrames;
    }
    else
    {
        std::fill (tracker.begin(), tracker.end(), 0.0f);
        trackerFrames = 0;
    }
}

void SpectralDenoiser::rebuildMapped (const NoiseProfile& p, std::vector<float>& dst) const
{
    dst.resize ((size_t) numBins);
    mapProfileToGrid (p, sampleRate, fftSize, dst.data());
    for (auto& v : dst) v *= sumW2;   // normalised PSD -> bin power for this window
}

//==============================================================================
void SpectralDenoiser::process (float* const* channels, int numCh, int numSamples) noexcept
{
    const int nch = std::min (numCh, numChannels);
    int i = 0;
    while (i < numSamples)
    {
        const int todo = std::min (numSamples - i, hop - hopCounter);
        for (int c = 0; c < nch; ++c)
        {
            float* io = channels[c] + i;
            float* dstIn = inFrame[c].data() + (fftSize - hop) + hopCounter;
            const float* srcOut = outReady[c].data() + hopCounter;
            for (int n = 0; n < todo; ++n)
            {
                dstIn[n] = io[n];
                io[n] = srcOut[n];
            }
        }
        hopCounter += todo;
        i += todo;
        if (hopCounter == hop)
        {
            processFrame();
            hopCounter = 0;
        }
    }
}

SpectralDenoiser::ModeParams SpectralDenoiser::currentModeParams() const noexcept
{
    ModeParams m;
    if (! settings.maximumMode)
    {
        // Natural: transparent speech, residual hiss attenuated to a constant floor
        m.gMinDb = -32.0f;  m.overSub = 1.0f;  m.xiMin = dbToPower (-22.0f); m.alphaDD = 0.96f;
        m.zetaLoDb = -10.0f; m.zetaHiDb = -5.0f; m.frameLoDb = -6.0f; m.frameHiDb = 0.0f;
        m.qMax = 0.999f; m.holdSeconds = 0.12f; m.releaseSeconds = 0.035f; m.deepFloorDb = 0.0f;
    }
    else
    {
        // Maximum: same speech handling, much deeper floor for speech-free bins,
        // slight noise over-estimation to reduce residual hiss inside speech
        m.gMinDb = -36.0f;  m.overSub = 1.1f;  m.xiMin = dbToPower (-25.0f); m.alphaDD = 0.97f;
        m.zetaLoDb = -10.0f; m.zetaHiDb = -5.0f; m.frameLoDb = -6.0f; m.frameHiDb = 0.0f;
        m.qMax = 0.999f; m.holdSeconds = 0.10f; m.releaseSeconds = 0.030f; m.deepFloorDb = -24.0f;
    }

    // Voice preservation (0.5 = neutral): more speech-presence sensitivity,
    // longer hold/release and less over-subtraction when raised.
    const float d = clampv ((settings.voicePreservation - 0.5f) * 2.0f, -1.0f, 1.0f);
    m.zetaLoDb  -= 3.0f * d;
    m.zetaHiDb  -= 3.0f * d;
    m.frameLoDb -= 1.0f * d;
    m.frameHiDb -= 1.0f * d;
    m.holdSeconds *= (1.0f + 0.8f * d);
    m.releaseSeconds *= (1.0f + 0.7f * d);
    m.overSub = std::max (1.0f, m.overSub * (1.0f - 0.15f * d));

    float* fields[12] = { &m.gMinDb, &m.overSub, &m.xiMin, &m.alphaDD, &m.zetaLoDb, &m.zetaHiDb,
                          &m.frameLoDb, &m.frameHiDb, &m.qMax, &m.holdSeconds, &m.releaseSeconds, &m.deepFloorDb };
    for (int i = 0; i < 12; ++i)
        if (! std::isnan (tuningOverride.v[i])) *fields[i] = tuningOverride.v[i];
    return m;
}

void SpectralDenoiser::processFrame() noexcept
{
    const float g2 = std::max (settings.inputGain * settings.inputGain, 1.0e-8f);

    // ---- analysis
    for (int c = 0; c < numChannels; ++c)
    {
        const float* src = inFrame[c].data();
        for (int n = 0; n < fftSize; ++n) timeBuf[(size_t) n] = src[n] * window[(size_t) n];
        fft.forward (timeBuf.data(), spec[c].data());
    }
    const float chScale = 1.0f / (float) numChannels;
    for (int k = 0; k < numBins; ++k)
    {
        float p = 0.0f;
        for (int c = 0; c < numChannels; ++c) p += std::norm (spec[c][(size_t) k]);
        p *= chScale;
        power[(size_t) k] = std::isfinite (p) ? p : 0.0f;
    }

    installPendingProfile();
    captureLearnFrame (g2);
    updateNoiseEstimate (g2);
    computeGains (currentModeParams());

    // ---- synthesis
    const float olaScale = 1.0f / 1.5f;   // sum of Hann^2 at 75 % overlap
    for (int c = 0; c < numChannels; ++c)
    {
        auto& s = spec[c];
        for (int k = 0; k < numBins; ++k) s[(size_t) k] *= gain[(size_t) k];
        fft.inverse (s.data(), timeBuf.data());

        float* acc = outAccum[c].data();
        for (int n = 0; n < fftSize; ++n) acc[n] += timeBuf[(size_t) n] * window[(size_t) n] * olaScale;

        std::memcpy (outReady[c].data(), acc, sizeof (float) * (size_t) hop);
        std::memmove (acc, acc + hop, sizeof (float) * (size_t) (fftSize - hop));
        std::fill (acc + fftSize - hop, acc + fftSize, 0.0f);

        float* in = inFrame[c].data();
        std::memmove (in, in + hop, sizeof (float) * (size_t) (fftSize - hop));
    }

    updateDisplay();
}

//==============================================================================
void SpectralDenoiser::updateNoiseEstimate (float g2) noexcept
{
    const float floorPow = kNoiseFloorPerSample * sumW2;
    const double hopSec = hop / sampleRate;

    // ---- SPP-based MMSE noise tracker (Gerkmann & Hendriks 2012)
    if (trackerFrames < kTrackerInitFrames)
    {
        const float w = 1.0f / (float) (trackerFrames + 1);
        for (int k = 0; k < numBins; ++k)
            tracker[(size_t) k] += (power[(size_t) k] - tracker[(size_t) k]) * w;
        ++trackerFrames;
    }
    else
    {
        const float xiH1 = 31.622776f;                  // 15 dB fixed a-priori SNR under H1
        const float c1 = 1.0f + xiH1;
        const float c2 = xiH1 / (1.0f + xiH1);
        const float alphaN = (float) std::pow (0.8, hopSec / 0.016);
        const float alphaP = (float) std::pow (0.9, hopSec / 0.016);
        for (int k = 0; k < numBins; ++k)
        {
            const float s2 = std::max (tracker[(size_t) k], floorPow);
            const float post = power[(size_t) k] / s2;
            float pH1 = 1.0f / (1.0f + c1 * std::exp (-std::min (post * c2, 80.0f)));
            float& pb = trackerPBar[(size_t) k];
            pb = alphaP * pb + (1.0f - alphaP) * pH1;
            if (pb > 0.99f) pH1 = std::min (pH1, 0.99f);
            const float est = (1.0f - pH1) * power[(size_t) k] + pH1 * s2;
            tracker[(size_t) k] = alphaN * s2 + (1.0f - alphaN) * est;
        }
    }

    if (profileActive)
    {
        // slow per-band level correction of the learned profile (-6 / +4.5 dB),
        // follows gain drift of the microphone/interface without re-learning
        std::fill (bandNum.begin(), bandNum.end(), 0.0);
        std::fill (bandDen.begin(), bandDen.end(), 0.0);
        for (int k = 1; k < numBins; ++k)
        {
            const int b = bandOfBin[(size_t) k * 2];
            bandNum[(size_t) b] += tracker[(size_t) k];
            bandDen[(size_t) b] += profileBins[(size_t) k] * g2;
        }
        const float a = timeConstantCoef (2.0, frameRate);
        for (int b = 0; b < numCorrBands; ++b)
        {
            float inst = bandDen[(size_t) b] > 0.0 ? (float) (bandNum[(size_t) b] / bandDen[(size_t) b]) : 1.0f;
            inst = clampv (inst, 0.25f, 4.0f);
            float& c = bandCorr[(size_t) b];
            c = std::exp (a * std::log (c) + (1.0f - a) * std::log (inst));
            c = clampv (c, 0.25f, 2.8f);
        }
        for (int k = 0; k < numBins; ++k)
        {
            const int lo = bandOfBin[(size_t) k * 2 + 1];
            const int hi = std::min (lo + 1, numCorrBands - 1);
            const float w = bandWeight[(size_t) k];
            const float corr = bandCorr[(size_t) lo] * (1.0f - w) + bandCorr[(size_t) hi] * w;
            noise[(size_t) k] = std::max (profileBins[(size_t) k] * g2 * corr, floorPow);
        }
    }
    else
    {
        for (int k = 0; k < numBins; ++k)
            noise[(size_t) k] = std::max (tracker[(size_t) k], floorPow);
    }
}

void SpectralDenoiser::computeGains (const ModeParams& mp) noexcept
{
    const float amount = clampv (settings.amount, 0.0f, 1.0f);
    if (amount <= 0.0f)
    {
        // exact bypass of the suppressor (states keep running for smooth re-entry)
        std::fill (gain.begin(), gain.end(), 1.0f);
        std::fill (gainPrev.begin(), gainPrev.end(), 1.0f);
        for (int k = 0; k < numBins; ++k) a2Prev[(size_t) k] = power[(size_t) k];
        reductionDb.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const float alphaDD = mp.alphaDD;
    const float bz = (float) std::pow (0.7, (hop / sampleRate) / 0.008);  // Cohen: 0.7 per 8 ms

    // ---- a-priori SNR (decision directed) and LSA gain under speech presence
    for (int k = 0; k < numBins; ++k)
    {
        const float neff = mp.overSub * noise[(size_t) k];
        const float inv = 1.0f / neff;
        const float gamma = std::min (power[(size_t) k] * inv, 1.0e4f);
        float x = alphaDD * a2Prev[(size_t) k] * inv + (1.0f - alphaDD) * std::max (gamma - 1.0f, 0.0f);
        x = std::max (x, mp.xiMin);
        const float v = gamma * x / (1.0f + x);
        float g = x / (1.0f + x) * std::exp (0.5f * expint1 (v));
        g = std::min (g, 1.0f);
        a2Prev[(size_t) k] = g * g * power[(size_t) k];
        zeta[(size_t) k] = bz * zeta[(size_t) k] + (1.0f - bz) * x;
        xi[(size_t) k] = x;
        vArr[(size_t) k] = v;
        gH1[(size_t) k] = std::max (g, 1.0e-6f);
    }

    // ---- IMCRA-style a-priori speech presence: local, global and frame evidence
    const int K = numBins;
    for (int k = 0; k < K; ++k)
    {
        const float zl = 0.25f * zeta[(size_t) std::max (k - 1, 0)] + 0.5f * zeta[(size_t) k]
                       + 0.25f * zeta[(size_t) std::min (k + 1, K - 1)];
        pLocal[(size_t) k] = rampDb (zl, mp.zetaLoDb, mp.zetaHiDb);

        float zg = 0.0f;
        for (int j = -globalHalf; j <= globalHalf; ++j)
            zg += globalKernel[(size_t) (j + globalHalf)] * zeta[(size_t) clampv (k + j, 0, K - 1)];
        pGlobal[(size_t) k] = rampDb (zg, mp.zetaLoDb, mp.zetaHiDb);
    }

    // Frame-level evidence uses the *current* a-priori SNR (no recursive
    // smoothing) so speech onsets are recognised within one hop; averaging
    // over ~100+ bins keeps its variance low in noise-only frames.
    auto meanXi = [this] (int b0, int b1)
    {
        double s = 0.0;
        for (int k = b0; k < b1; ++k) s += xi[(size_t) k];
        return (float) (s / std::max (1, b1 - b0));
    };
    const float zFrame = std::max (meanXi (frameLoBin0, frameLoBin1), meanXi (frameHiBin0, frameHiBin1));
    const float pfRaw = rampDb (zFrame, mp.frameLoDb, mp.frameHiDb);
    if (pfRaw >= pFrame)
    {
        pFrame = pfRaw;
        holdCounter = (int) std::round (mp.holdSeconds * frameRate);
    }
    else if (holdCounter > 0)
        --holdCounter;
    else
    {
        const float rel = timeConstantCoef (0.05, frameRate);
        pFrame = rel * pFrame + (1.0f - rel) * pfRaw;
    }
    speechActivity.store (pFrame, std::memory_order_relaxed);

    // ---- OM-LSA gain with release smoothing
    const float lnGMin = std::log (dbToGain (mp.gMinDb));
    const float lnDeep = std::log (dbToGain (mp.deepFloorDb));
    const float rel = timeConstantCoef (mp.releaseSeconds, frameRate);
    double eIn = 0.0, eOut = 0.0;
    for (int k = 0; k < K; ++k)
    {
        float q = 1.0f - pLocal[(size_t) k] * pGlobal[(size_t) k] * pFrame;
        q = clampv (q, 0.0f, mp.qMax);
        const float ratio = q / (1.0f - q);
        const float x = xi[(size_t) k];
        const float p = 1.0f / (1.0f + ratio * (1.0f + x) * std::exp (-std::min (vArr[(size_t) k], 80.0f)));
        float lnG = p * std::log (gH1[(size_t) k]) + (1.0f - p) * lnGMin;
        // Deep floor: applied uniformly in speech-free frames (constant gain, no
        // musical noise) and, inside speech frames, only to bins where speech is
        // confidently absent.
        const float deepW = std::max (clampv (1.0f - 2.0f * pFrame, 0.0f, 1.0f), clampv (1.0f - p * (1.0f / kDeepP), 0.0f, 1.0f));
        lnG += deepW * lnDeep;
        float g = std::exp (amount * lnG);
        g = std::max (g, gainPrev[(size_t) k] * rel);
        g = std::min (g, 1.0f);
        gainPrev[(size_t) k] = g;
        gain[(size_t) k] = g;
        eIn += power[(size_t) k];
        eOut += power[(size_t) k] * g * g;
    }
    const float red = eIn > 1.0e-20 ? (float) (10.0 * std::log10 (std::max (eOut, 1.0e-30) / eIn)) : 0.0f;
    reductionDb.store (red, std::memory_order_relaxed);
}

//==============================================================================
void SpectralDenoiser::installPendingProfile() noexcept
{
    int expected = mbFull;
    if (! mailbox.compare_exchange_strong (expected, mbReading, std::memory_order_acquire))
        return;

    if (mailboxClear)
    {
        profileActive = false;
        trackerFrames = std::min (trackerFrames, kTrackerInitFrames);
    }
    else if (mailboxFft == fftSize)
    {
        std::copy (mailboxBins.begin(), mailboxBins.end(), profileBins.begin());
        profileActive = true;
        const float g2 = settings.inputGain * settings.inputGain;
        for (int k = 0; k < numBins; ++k) tracker[(size_t) k] = profileBins[(size_t) k] * g2;
        std::fill (trackerPBar.begin(), trackerPBar.end(), 0.0f);
        std::fill (bandCorr.begin(), bandCorr.end(), 1.0f);
        trackerFrames = kTrackerInitFrames;
    }
    mailbox.store (mbEmpty, std::memory_order_release);
}

void SpectralDenoiser::captureLearnFrame (float g2) noexcept
{
    int st = learnState.load (std::memory_order_acquire);
    if (st == (int) LearnState::requested)
    {
        if (learnState.compare_exchange_strong (st, (int) LearnState::capturing))
        {
            capturedFrames = 0;
            learnProgress.store (0.0f);
            st = (int) LearnState::capturing;
        }
    }
    if (st != (int) LearnState::capturing || learnFrames <= 0)
        return;

    float* row = captureBuf.data() + (size_t) capturedFrames * (size_t) numBins;
    const float inv = 1.0f / (g2 * sumW2);
    for (int k = 0; k < numBins; ++k) row[k] = power[(size_t) k] * inv;
    ++capturedFrames;
    learnProgress.store ((float) capturedFrames / (float) learnFrames, std::memory_order_relaxed);
    if (capturedFrames >= learnFrames)
    {
        int expected = (int) LearnState::capturing;
        learnState.compare_exchange_strong (expected, (int) LearnState::captureDone, std::memory_order_release);
    }
}

void SpectralDenoiser::requestLearn() noexcept
{
    const int st = learnState.load();
    if (st == (int) LearnState::requested || st == (int) LearnState::capturing
        || st == (int) LearnState::captureDone || st == (int) LearnState::analysing)
        return;
    learnProgress.store (0.0f);
    learnState.store ((int) LearnState::requested, std::memory_order_release);
}

void SpectralDenoiser::cancelLearn() noexcept
{
    int st = (int) LearnState::requested;
    if (learnState.compare_exchange_strong (st, (int) LearnState::idle)) return;
    st = (int) LearnState::capturing;
    learnState.compare_exchange_strong (st, (int) LearnState::idle);
}

void SpectralDenoiser::acknowledgeLearnResult() noexcept
{
    int st = learnState.load();
    if (st == (int) LearnState::succeeded || st == (int) LearnState::failedActivity
        || st == (int) LearnState::failedNoSignal)
        learnState.compare_exchange_strong (st, (int) LearnState::idle);
}

bool SpectralDenoiser::serviceBackgroundTasks()
{
    if (learnState.load (std::memory_order_acquire) != (int) LearnState::captureDone)
        return false;

    std::unique_lock<std::mutex> lock (profileMutex);
    int expected = (int) LearnState::captureDone;
    if (! learnState.compare_exchange_strong (expected, (int) LearnState::analysing, std::memory_order_acq_rel))
        return false;

    NoiseProfile p;
    p.sampleRate = sampleRate;
    p.fftSize = fftSize;
    const LearnAnalysis a = analyseNoiseCapture (captureBuf.data(), capturedFrames, numBins,
                                                 sampleRate, fftSize, p.psd);
    p.acceptedFraction = a.acceptedFraction;

    if (a.outcome != LearnOutcome::ok)
    {
        learnState.store ((int) (a.outcome == LearnOutcome::noSignal ? LearnState::failedNoSignal
                                                                     : LearnState::failedActivity));
        return false;
    }

    double var = 0.0;
    for (size_t k = 1; k + 1 < p.psd.size(); ++k) var += p.psd[k];
    var *= 2.0 / fftSize;
    p.levelDb = (float) (10.0 * std::log10 (std::max (var, 1.0e-30)));

    storedProfile = p;
    profilePresent.store (true);

    // hand the mapped profile to the audio thread
    std::vector<float> mapped;
    rebuildMapped (p, mapped);
    lock.unlock();

    for (;;)
    {
        int e = mbEmpty;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        e = mbFull;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        std::this_thread::yield();
    }
    std::copy (mapped.begin(), mapped.end(), mailboxBins.begin());
    mailboxClear = false;
    mailboxFft = fftSize;
    mailbox.store (mbFull, std::memory_order_release);

    learnState.store ((int) LearnState::succeeded);
    return true;
}

NoiseProfile SpectralDenoiser::getProfile() const
{
    std::lock_guard<std::mutex> lock (profileMutex);
    return storedProfile;
}

bool SpectralDenoiser::profileMatchesCurrentRate() const
{
    std::lock_guard<std::mutex> lock (profileMutex);
    return storedProfile.isValid() && std::abs (storedProfile.sampleRate - sampleRate) < 1.0;
}

void SpectralDenoiser::setProfile (const NoiseProfile& p)
{
    if (! p.isValid()) { clearProfile(); return; }

    std::vector<float> mapped;
    int fftNow = 0;
    {
        std::lock_guard<std::mutex> lock (profileMutex);
        storedProfile = p;
        profilePresent.store (true);
        rebuildMapped (p, mapped);
        fftNow = fftSize;
    }
    for (;;)
    {
        int e = mbEmpty;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        e = mbFull;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        std::this_thread::yield();
    }
    if (mailboxBins.size() == mapped.size())
        std::copy (mapped.begin(), mapped.end(), mailboxBins.begin());
    mailboxClear = false;
    mailboxFft = fftNow;
    mailbox.store (mbFull, std::memory_order_release);
}

void SpectralDenoiser::clearProfile()
{
    {
        std::lock_guard<std::mutex> lock (profileMutex);
        storedProfile = NoiseProfile();
        profilePresent.store (false);
    }
    for (;;)
    {
        int e = mbEmpty;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        e = mbFull;
        if (mailbox.compare_exchange_strong (e, mbWriting)) break;
        std::this_thread::yield();
    }
    mailboxClear = true;
    mailbox.store (mbFull, std::memory_order_release);
}

//==============================================================================
void SpectralDenoiser::updateDisplay() noexcept
{
    if (++displayCounter < 2) return;
    displayCounter = 0;

    // 0 dB = full-scale sine in one bin (Hann: |X| = N/4)
    const float q = (float) fftSize * 0.25f;
    const float ref = 1.0f / (q * q);
    const float a = 0.6f;
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        const int b0 = dispBin0[(size_t) i], b1 = dispBin1[(size_t) i];
        float pin = 0.0f, pout = 0.0f, pn = 0.0f;
        for (int k = b0; k < b1; ++k)
        {
            pin += power[(size_t) k];
            pout += power[(size_t) k] * gain[(size_t) k] * gain[(size_t) k];
            pn += noise[(size_t) k];
        }
        const float inv = ref / (float) (b1 - b0);
        const float din = powerToDb (pin * inv), dout = powerToDb (pout * inv), dn = powerToDb (pn * inv);
        dispIn[(size_t) i]    = din  > dispIn[(size_t) i]  ? din  : a * dispIn[(size_t) i]  + (1 - a) * din;
        dispOut[(size_t) i]   = dout > dispOut[(size_t) i] ? dout : a * dispOut[(size_t) i] + (1 - a) * dout;
        dispNoise[(size_t) i] = dn;
        dispInA[i].store (dispIn[(size_t) i], std::memory_order_relaxed);
        dispOutA[i].store (dispOut[(size_t) i], std::memory_order_relaxed);
        dispNoiseA[i].store (dispNoise[(size_t) i], std::memory_order_relaxed);
    }
}

void SpectralDenoiser::getDisplaySpectra (float* inputDb, float* outputDb, float* noiseDb) const noexcept
{
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        if (inputDb)  inputDb[i]  = dispInA[i].load (std::memory_order_relaxed);
        if (outputDb) outputDb[i] = dispOutA[i].load (std::memory_order_relaxed);
        if (noiseDb)  noiseDb[i]  = dispNoiseA[i].load (std::memory_order_relaxed);
    }
}

} // namespace deadnoise
