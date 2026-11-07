// DEADNOISE - automated DSP tests (no framework dependency).
// Build: part of the CMake project (target deadnoise_tests). Exit code 0 = all passed.
#include "DeadnoiseEngine.h"
#include "RealFFT.h"
#include "DspMath.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <random>
#include <string>
#include <vector>

using namespace deadnoise;

//==============================================================================
// Allocation tracking: every operator new while g_trackAllocs is set counts.
static std::atomic<bool> g_trackAllocs { false };
static std::atomic<long> g_allocCount { 0 };

void* operator new (size_t n)
{
    if (g_trackAllocs.load (std::memory_order_relaxed)) g_allocCount.fetch_add (1);
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, size_t) noexcept { std::free (p); }
void operator delete[] (void* p, size_t) noexcept { std::free (p); }

//==============================================================================
static int g_failures = 0, g_checks = 0;

#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (! (cond)) {                                                         \
            ++g_failures;                                                       \
            std::printf ("    FAIL  %s:%d  %s  -- ", __FILE__, __LINE__, #cond); \
            std::printf (__VA_ARGS__); std::printf ("\n");                      \
        }                                                                       \
    } while (0)

static void section (const char* name) { std::printf ("\n[%s]\n", name); }
static void metric (const char* name, double v, const char* unit = "") { std::printf ("    %-52s %10.3f %s\n", name, v, unit); }

//==============================================================================
// Signal helpers
using Buf = std::vector<float>;

static Buf whiteNoise (size_t n, float rms, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, rms);
    Buf b (n);
    for (auto& x : b) x = d (rng);
    return b;
}

// High-frequency weighted hiss: white noise through a first-order high-shelf-ish
// differentiator mix (rising ~6 dB/oct above ~2 kHz), normalised to rms.
static Buf hfHiss (size_t n, float rms, unsigned seed, double sr = 48000.0)
{
    Buf w = whiteNoise (n + 1, 1.0f, seed);
    Buf b (n);
    const float a = (float) (sr / (2.0 * kPi * 2000.0)) * 0.25f;
    for (size_t i = 0; i < n; ++i) b[i] = 0.35f * w[i + 1] + a * (w[i + 1] - w[i]);
    double s = 0; for (float x : b) s += (double) x * x;
    const float g = rms / (float) std::sqrt (s / (double) n);
    for (auto& x : b) x *= g;
    return b;
}

// Synthetic vowel: harmonics of f0 with a formant-like envelope, amplitude-modulated
// into syllables (on/off), mimicking voiced speech.
static Buf vowel (size_t n, double sr, double f0, float amp, double syllableHz = 0.0)
{
    Buf b (n, 0.0f);
    const double formants[3] = { 700.0, 1200.0, 2600.0 };
    for (int h = 1; h * f0 < std::min (8000.0, sr * 0.45); ++h)
    {
        const double f = h * f0;
        double env = 0.0;
        for (double F : formants) env += std::exp (-std::pow ((f - F) / 300.0, 2.0));
        env = 0.08 + env;
        const double ph = 0.37 * h * h;
        for (size_t i = 0; i < n; ++i) b[i] += (float) (env * std::sin (2.0 * kPi * f * (double) i / sr + ph));
    }
    float mx = 0; for (float x : b) mx = std::max (mx, std::abs (x));
    for (size_t i = 0; i < n; ++i)
    {
        float g = amp / mx;
        if (syllableHz > 0.0)
        {
            const double s = std::sin (2.0 * kPi * syllableHz * (double) i / sr);
            g *= (float) std::max (0.0, s);   // half-wave: 50 % duty cycle syllables
        }
        b[i] *= g;
    }
    return b;
}

static double rmsDb (const Buf& b, size_t from, size_t to)
{
    double s = 0.0;
    to = std::min (to, b.size());
    for (size_t i = from; i < to; ++i) s += (double) b[i] * b[i];
    return 10.0 * std::log10 (s / (double) std::max<size_t> (1, to - from) + 1e-30);
}

struct RunOptions
{
    double sr = 48000.0;
    int block = 512;
    bool randomBlocks = false;
    double learnAt = -1.0;
    EngineParams params;
    std::function<void (DeadnoiseEngine&, size_t)> perBlock;
};

// Runs the engine over mono or stereo input; returns output channels.
static std::vector<Buf> run (const std::vector<Buf>& in, const RunOptions& o, DeadnoiseEngine* external = nullptr,
                             long* allocs = nullptr)
{
    DeadnoiseEngine local;
    DeadnoiseEngine& e = external ? *external : local;
    const int nch = (int) in.size();
    e.setParams (o.params);
    if (! external) e.prepare (o.sr, o.randomBlocks ? 4096 : o.block, nch);
    std::vector<Buf> out = in;
    std::mt19937 rng (99);
    std::uniform_int_distribution<int> d (1, 4096);
    size_t pos = 0;
    bool learnReq = false;
    const size_t n = in[0].size();
    long allocTotal = 0;
    while (pos < n)
    {
        const int bs = (int) std::min<size_t> (n - pos, (size_t) (o.randomBlocks ? d (rng) : o.block));
        if (o.learnAt >= 0.0 && ! learnReq && (double) pos / o.sr >= o.learnAt) { e.getDenoiser().requestLearn(); learnReq = true; }
        if (o.perBlock) o.perBlock (e, pos);
        float* ptrs[2] = {};
        for (int c = 0; c < nch; ++c) ptrs[c] = out[(size_t) c].data() + pos;
        g_allocCount = 0;
        g_trackAllocs = true;
        e.process (ptrs, nch, bs);
        g_trackAllocs = false;
        allocTotal += g_allocCount.load();
        e.getDenoiser().serviceBackgroundTasks();
        pos += (size_t) bs;
    }
    if (allocs) *allocs = allocTotal;
    return out;
}

//==============================================================================
static void testFFT()
{
    section ("FFT correctness");
    for (int n : { 64, 512, 1024, 2048 })
    {
        RealFFT f; f.init (n);
        Buf x = whiteNoise ((size_t) n, 1.0f, (unsigned) n);
        std::vector<std::complex<float>> X ((size_t) n / 2 + 1);
        f.forward (x.data(), X.data());
        double maxErr = 0.0;
        for (int k = 0; k <= n / 2; ++k)
        {
            std::complex<double> ref = 0;
            for (int t = 0; t < n; ++t) ref += (double) x[(size_t) t] * std::polar (1.0, -2.0 * kPi * k * t / n);
            maxErr = std::max (maxErr, std::abs (ref - std::complex<double> (X[(size_t) k])) / std::sqrt ((double) n));
        }
        Buf y ((size_t) n);
        f.inverse (X.data(), y.data());
        double rt = 0.0;
        for (int t = 0; t < n; ++t) rt = std::max (rt, (double) std::abs (y[(size_t) t] - x[(size_t) t]));
        CHECK (maxErr < 1e-4, "n=%d dft error %g", n, maxErr);
        CHECK (rt < 1e-5, "n=%d round trip error %g", n, rt);
        std::printf ("    n=%-5d  max |DFT err|/sqrt(n) = %.2e   round trip = %.2e\n", n, maxErr, rt);
    }
}

static void testReconstructionAndLatency()
{
    section ("Transparency at 0 % reduction, latency, sample rates, channel layouts");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int nch : { 1, 2 })
        {
            RunOptions o; o.sr = sr; o.randomBlocks = true;
            o.params.reduction = 0.0f;
            const size_t n = (size_t) (sr * 1.5);
            std::vector<Buf> in;
            for (int c = 0; c < nch; ++c) in.push_back (whiteNoise (n, 0.1f, 7u + (unsigned) c));
            DeadnoiseEngine e; e.setParams (o.params); e.prepare (sr, 4096, nch);
            const int L = e.getLatencySamples();
            auto out = run (in, o, &e);
            double maxErr = 0.0;
            for (int c = 0; c < nch; ++c)
                for (size_t i = (size_t) L; i < n; ++i)
                    maxErr = std::max (maxErr, (double) std::abs (out[(size_t) c][i] - in[(size_t) c][i - (size_t) L]));
            CHECK (maxErr < 2e-5, "sr=%g ch=%d max err %g", sr, nch, maxErr);
            std::printf ("    %6.0f Hz  %d ch  latency %4d smp (%.2f ms)  max |out - delayed in| = %.2e\n",
                         sr, nch, L, 1000.0 * L / sr, maxErr);
        }

    // impulse with suppression active: timing must stay exactly at the reported latency
    RunOptions o; o.params.reduction = 1.0f;
    const size_t n = 48000 * 6;
    Buf x = hfHiss (n, 0.003f, 3);
    const size_t imp = 48000 * 5;
    x[imp] += 0.9f;
    o.learnAt = 0.2;
    DeadnoiseEngine e; e.setParams (o.params); e.prepare (48000.0, 512, 1);
    auto out = run ({ x }, o, &e);
    size_t peak = 0; float pv = 0;
    for (size_t i = imp; i < imp + 4096; ++i) if (std::abs (out[0][i]) > pv) { pv = std::abs (out[0][i]); peak = i; }
    CHECK (peak - imp == (size_t) e.getLatencySamples(), "impulse at +%zu, latency %d", peak - imp, e.getLatencySamples());
    metric ("impulse peak offset with suppression active (samples)", (double) (peak - imp));
    metric ("impulse peak level preserved (dB)", 20.0 * std::log10 (pv / 0.9));
    CHECK (std::abs (20.0 * std::log10 (pv / 0.9)) < 1.0, "impulse level changed");
}

static void testSilenceAndInvalidInput()
{
    section ("Silence, NaN / Inf input, extreme levels");
    {
        RunOptions o; o.params.reduction = 1.0f;
        Buf z (48000 * 2, 0.0f);
        auto out = run ({ z, z }, o);
        bool allZero = true;
        for (auto& ch : out) for (float v : ch) if (v != 0.0f) allZero = false;
        CHECK (allZero, "digital silence in -> non-zero out");
        std::printf ("    digital silence -> exact digital silence: %s\n", allZero ? "yes" : "NO");
    }
    {
        RunOptions o; o.params.reduction = 1.0f; o.params.maximumMode = true;
        Buf x = hfHiss (48000 * 4, 0.01f, 5);
        for (size_t i = 48000; i < 48000 + 300; ++i) x[i] = (i % 3 == 0) ? NAN : (i % 3 == 1 ? INFINITY : -INFINITY);
        auto out = run ({ x }, o);
        bool finite = true;
        for (float v : out[0]) if (! std::isfinite (v)) finite = false;
        CHECK (finite, "non-finite output after NaN/Inf input");
        const double tail = rmsDb (out[0], 48000 * 3, 48000 * 4);
        CHECK (tail < rmsDb (x, 48000 * 3, 48000 * 4) - 10.0, "did not recover after NaN burst");
        std::printf ("    NaN/Inf burst: output finite = %s, still suppressing afterwards (%.1f dB)\n", finite ? "yes" : "NO", tail);
    }
    {
        RunOptions o; o.params.reduction = 1.0f;
        Buf x = vowel (48000 * 3, 48000.0, 140.0, 0.99f, 2.0);
        Buf h = hfHiss (x.size(), 0.002f, 8);
        for (size_t i = 0; i < x.size(); ++i) x[i] += h[i];
        auto out = run ({ x }, o);
        float inPk = 0, outPk = 0;
        for (float v : x) inPk = std::max (inPk, std::abs (v));
        for (float v : out[0]) outPk = std::max (outPk, std::abs (v));
        metric ("loud input: peak in (dBFS)", 20.0 * std::log10 (inPk));
        metric ("loud input: peak out (dBFS)", 20.0 * std::log10 (outPk));
        CHECK (outPk <= inPk * 1.03f, "suppressor increased the peak level (%f > %f)", outPk, inPk);
    }
}

static void testBlockSizeInvariance()
{
    section ("Host buffer-size independence (random block sizes 1..4096)");
    Buf x = hfHiss (48000 * 5, 0.005f, 11);
    Buf v = vowel (x.size(), 48000.0, 120.0, 0.3f, 1.5);
    for (size_t i = 0; i < x.size(); ++i) x[i] += v[i];
    RunOptions a; a.params.reduction = 0.9f; a.learnAt = 0.0; a.block = 512;
    RunOptions b = a; b.randomBlocks = true;
    // learning needs noise only: put the vowel after 4.5 s
    for (size_t i = 0; i < (size_t) (48000 * 4.5); ++i) x[i] -= v[i];
    auto oa = run ({ x }, a), ob = run ({ x }, b);
    double maxDiff = 0;
    for (size_t i = 0; i < x.size(); ++i) maxDiff = std::max (maxDiff, (double) std::abs (oa[0][i] - ob[0][i]));
    // the learn request timing differs by up to one block, so only compare after the profile is in place
    CHECK (maxDiff < 1e-3, "block size changed the result by %g", maxDiff);
    metric ("max difference fixed 512 vs random blocks", maxDiff);
}

static void testNoAllocation()
{
    section ("Real-time safety: heap allocations inside process()");
    long allocs = -1;
    RunOptions o; o.randomBlocks = true; o.learnAt = 0.1; o.params.reduction = 1.0f; o.params.gateEnabled = true;
    o.perBlock = [] (DeadnoiseEngine& e, size_t pos)
    {
        auto p = e.getParams();
        p.maximumMode = (pos / 24000) % 2 == 1;
        p.monitor = (MonitorMode) ((pos / 36000) % 3);
        p.voicePreservation = (float) ((pos / 12000) % 10) / 9.0f;
        e.setParams (p);
    };
    Buf x = hfHiss (48000 * 8, 0.004f, 13);
    run ({ x, x }, o, nullptr, &allocs);
    CHECK (allocs == 0, "%ld allocations on the audio thread", allocs);
    metric ("operator new calls during process() (incl. learning)", (double) allocs);
}

static void testLearning()
{
    section ("Learn Background: accuracy and speech rejection");
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 6);
    Buf hiss = hfHiss (n, 0.004f, 21);

    auto learn = [&] (const Buf& x, LearnState& st) -> NoiseProfile
    {
        DeadnoiseEngine e; RunOptions o; o.learnAt = 0.5; o.params.reduction = 0.0f;
        e.setParams (o.params); e.prepare (sr, 512, 1);
        run ({ x }, o, &e);
        st = e.getDenoiser().getLearnState();
        return e.getDenoiser().getProfile();
    };

    // reference PSD of the same noise process from a long independent realisation
    NoiseProfile ref;
    {
        LearnState st;
        Buf longHiss = hfHiss (n, 0.004f, 77);
        ref = learn (longHiss, st);
        CHECK (st == LearnState::succeeded, "learning failed on clean hiss (%d)", (int) st);
        metric ("clean hiss: accepted frames (%)", 100.0 * ref.acceptedFraction);
    }
    auto bandErrorDb = [&] (const NoiseProfile& p)
    {
        // compare in 1/3-octave-ish groups 150 Hz .. 16 kHz
        double worst = 0.0;
        const double binHz = p.sampleRate / p.fftSize;
        for (double f = 150.0; f < 16000.0; f *= 1.26)
        {
            const int k0 = (int) (f / binHz), k1 = std::max (k0 + 1, (int) (f * 1.26 / binHz));
            double a = 0, b = 0;
            for (int k = k0; k < k1; ++k) { a += p.psd[(size_t) k]; b += ref.psd[(size_t) k]; }
            worst = std::max (worst, std::abs (10.0 * std::log10 (a / b)));
        }
        return worst;
    };
    {
        LearnState st;
        auto p = learn (hiss, st);
        CHECK (st == LearnState::succeeded, "clean hiss");
        const double err = bandErrorDb (p);
        metric ("clean hiss: worst band error vs reference (dB)", err);
        CHECK (err < 1.0, "profile error %g dB", err);
    }
    {
        // speaking during 35 % of the learn window (vowel syllables)
        Buf x = hiss;
        Buf v = vowel (n, sr, 130.0, 0.25f, 0.0);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const bool talk = (t > 1.0 && t < 1.6) || (t > 2.4 && t < 2.9) || (t > 3.5 && t < 3.9);
            if (talk) x[i] += v[i];
        }
        LearnState st;
        auto p = learn (x, st);
        CHECK (st == LearnState::succeeded, "should still learn with partial speech (%d)", (int) st);
        if (st == LearnState::succeeded)
        {
            const double err = bandErrorDb (p);
            metric ("speech in 37 % of window: accepted frames (%)", 100.0 * p.acceptedFraction);
            metric ("speech in 37 % of window: worst band error (dB)", err);
            CHECK (err < 1.5, "speech leaked into the profile: %g dB", err);
        }
    }
    {
        // talking through the whole learn window (two overlapping syllabic voices,
        // speech present in ~90 % of frames): must refuse, or at worst produce a
        // profile that is not inflated by the voice
        Buf x = hiss;
        Buf v = vowel (n, sr, 130.0, 0.25f, 3.1);
        Buf v2 = vowel (n, sr, 175.0, 0.25f, 4.7);
        for (size_t i = 0; i < n; ++i) x[i] += v[i] + v2[(i + 4000) % n];
        LearnState st;
        auto p = learn (x, st);
        const double err = st == LearnState::succeeded ? bandErrorDb (p) : 0.0;
        CHECK (st == LearnState::failedActivity || err < 3.0, "continuous speech was learned as noise (%d, %g dB)", (int) st, err);
        std::printf ("    continuous talking during learning -> %s\n",
                     st == LearnState::failedActivity ? "rejected (correct)" : (err < 3.0 ? "accepted gaps only" : "VOICE LEARNED"));
    }
    {
        Buf z (n, 0.0f);
        LearnState st;
        learn (z, st);
        CHECK (st == LearnState::failedNoSignal, "silence (%d)", (int) st);
        std::printf ("    muted input during learning -> %s\n", st == LearnState::failedNoSignal ? "'no signal' (correct)" : "WRONG");
    }
    {
        // serialisation round trip and corruption rejection
        auto bytes = ref.serialise();
        NoiseProfile back;
        CHECK (NoiseProfile::deserialise (bytes.data(), bytes.size(), back), "deserialise");
        CHECK (back.psd == ref.psd && back.fftSize == ref.fftSize, "round trip mismatch");
        auto bad = bytes; bad[10] ^= 0xFF; bad.resize (bad.size() - 7);
        NoiseProfile tmp;
        CHECK (! NoiseProfile::deserialise (bad.data(), bad.size(), tmp), "corrupt data accepted");
        std::printf ("    profile serialise/deserialise round trip ok, truncated data rejected\n");
    }
}

static void testSuppression()
{
    section ("Noise suppression depth (stationary hiss, profile learned)");
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 10);
    for (int noiseType = 0; noiseType < 2; ++noiseType)
        for (int mode = 0; mode < 2; ++mode)
            for (float amt : { 0.5f, 0.8f, 1.0f })
            {
                Buf x = noiseType == 0 ? hfHiss (n, 0.003f, 31) : whiteNoise (n, 0.003f, 32);
                RunOptions o; o.learnAt = 0.3; o.params.reduction = amt; o.params.maximumMode = mode == 1;
                auto out = run ({ x }, o);
                const double att = rmsDb (out[0], (size_t) (sr * 6), n) - rmsDb (x, (size_t) (sr * 6), n);
                const double expected = (mode == 0 ? -32.0 : -60.0) * amt;
                std::printf ("    %-9s %-8s %3.0f %%   attenuation %6.1f dB  (target %5.1f dB)\n",
                             noiseType == 0 ? "HF hiss" : "white", mode == 0 ? "Natural" : "Maximum", amt * 100, att, expected);
                CHECK (att < expected + 3.0, "attenuation %g dB", att);
            }

    // adaptive (no profile): converges without learning
    {
        Buf x = hfHiss (n, 0.003f, 33);
        RunOptions o; o.params.reduction = 0.8f;
        auto out = run ({ x }, o);
        const double att = rmsDb (out[0], (size_t) (sr * 6), n) - rmsDb (x, (size_t) (sr * 6), n);
        metric ("adaptive tracking (no profile) attenuation (dB)", att, "dB");
        CHECK (att < -20.0, "adaptive att %g", att);
    }

    // input gain changes after learning keep the profile valid
    {
        Buf x = hfHiss (n, 0.003f, 34);
        RunOptions o; o.learnAt = 0.3; o.params.reduction = 1.0f;
        o.perBlock = [] (DeadnoiseEngine& e, size_t pos)
        {
            auto p = e.getParams();
            p.inputGainDb = pos > 48000 * 5 ? 12.0f : 0.0f;
            e.setParams (p);
        };
        auto out = run ({ x }, o);
        const double att = rmsDb (out[0], (size_t) (sr * 7), n) - (rmsDb (x, (size_t) (sr * 7), n) + 12.0);
        metric ("+12 dB input gain after learning: attenuation (dB)", att, "dB");
        CHECK (att < -29.0, "profile not scaled with input gain (%g)", att);
    }

    // noise level drift: +6 dB after learning, slow per-band correction
    {
        Buf x = hfHiss (n, 0.003f, 35);
        for (size_t i = (size_t) (sr * 5); i < n; ++i) x[i] *= 2.0f;
        RunOptions o; o.learnAt = 0.3; o.params.reduction = 0.8f;
        auto out = run ({ x }, o);
        const double att = rmsDb (out[0], (size_t) (sr * 8), n) - rmsDb (x, (size_t) (sr * 8), n);
        metric ("hiss +6 dB after learning (3 s later) attenuation (dB)", att, "dB");
        CHECK (att < -15.0, "drift handling %g", att);
    }
}

static void testSpeechPreservation()
{
    section ("Speech preservation (synthetic voiced speech + hiss, 20 dB SNR)");
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 10);
    Buf clean (n, 0.0f);
    Buf v = vowel (n, sr, 145.0, 0.3f, 1.7);
    for (size_t i = (size_t) (sr * 5); i < n; ++i) clean[i] = v[i];
    const double sDb = rmsDb (clean, (size_t) (sr * 5), n);
    Buf hiss = hfHiss (n, 1.0f, 41);
    const float nscale = (float) std::pow (10.0, (sDb - 20.0) / 20.0);
    Buf x (n);
    for (size_t i = 0; i < n; ++i) x[i] = clean[i] + hiss[i] * nscale;

    for (int mode = 0; mode < 2; ++mode)
    {
        RunOptions o; o.learnAt = 0.3; o.params.reduction = 1.0f; o.params.maximumMode = mode == 1;
        DeadnoiseEngine e; e.setParams (o.params); e.prepare (sr, 512, 1);
        auto out = run ({ x }, o, &e);
        const size_t L = (size_t) e.getLatencySamples();
        // harmonic level preservation: compare output (latency-compensated) with the clean
        // signal via projection on the clean waveform during voiced parts
        double num = 0, den = 0, err = 0;
        for (size_t i = (size_t) (sr * 5.5); i + L < n; ++i)
        {
            num += (double) out[0][i + L] * clean[i];
            den += (double) clean[i] * clean[i];
        }
        const double gainOnSpeech = num / den;
        for (size_t i = (size_t) (sr * 5.5); i + L < n; ++i)
        {
            const double d = out[0][i + L] - gainOnSpeech * clean[i];
            err += d * d;
        }
        const double sdr = 10.0 * std::log10 (den * gainOnSpeech * gainOnSpeech / err);
        double nin = 0; for (size_t i = (size_t) (sr * 5.5); i < n; ++i) { const double d = x[i] - clean[i]; nin += d * d; }
        const double snrIn = 10.0 * std::log10 (den / nin);
        std::printf ("    %-8s speech level change %+6.2f dB   SDR in %5.1f dB -> out %5.1f dB\n",
                     mode == 0 ? "Natural" : "Maximum", 20.0 * std::log10 (gainOnSpeech), snrIn, sdr);
        CHECK (std::abs (20.0 * std::log10 (gainOnSpeech)) < 1.5, "speech level changed");
        CHECK (sdr > snrIn + 6.0, "no SDR improvement");
    }
}

static void testStereoAndMonitor()
{
    section ("Stereo linking, monitor modes, gate");
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 6);
    Buf l = hfHiss (n, 0.003f, 51);
    Buf v = vowel (n, sr, 150.0, 0.2f, 1.5);
    for (size_t i = (size_t) (sr * 4.5); i < n; ++i) l[i] += v[i];
    {
        RunOptions o; o.learnAt = 0.2; o.params.reduction = 1.0f;
        auto out = run ({ l, l }, o);
        double d = 0; for (size_t i = 0; i < n; ++i) d = std::max (d, (double) std::abs (out[0][i] - out[1][i]));
        CHECK (d == 0.0, "identical L/R produced different outputs (%g)", d);
        Buf z (n, 0.0f);
        auto out2 = run ({ l, z }, o);
        double r = 0; for (float x : out2[1]) r = std::max (r, (double) std::abs (x));
        CHECK (r == 0.0, "silent right channel got signal (%g)", r);
        std::printf ("    identical L/R -> identical output: %s;  L-only input -> R stays silent: %s\n",
                     d == 0.0 ? "yes" : "NO", r == 0.0 ? "yes" : "NO");
    }
    {
        RunOptions base; base.learnAt = 0.2; base.params.reduction = 1.0f; base.params.inputGainDb = 3.0f; base.params.outputGainDb = -2.0f;
        RunOptions orig = base; orig.params.monitor = MonitorMode::original;
        RunOptions rem = base; rem.params.monitor = MonitorMode::removedNoise;
        DeadnoiseEngine e; e.setParams (base.params); e.prepare (sr, 512, 1);
        const size_t L = (size_t) e.getLatencySamples();
        auto p = run ({ l }, base), o = run ({ l }, orig), r = run ({ l }, rem);
        double eo = 0, er = 0;
        const float g = dbToGain (3.0f) * dbToGain (-2.0f);
        for (size_t i = L + 4800; i < n; ++i)
        {
            eo = std::max (eo, (double) std::abs (o[0][i] - l[i - L] * g));
            er = std::max (er, (double) std::abs (r[0][i] - (o[0][i] - p[0][i])));
        }
        CHECK (eo < 1e-5, "original monitor is not the delayed input (%g)", eo);
        CHECK (er < 1e-5, "removed-noise monitor != original - processed (%g)", er);
        std::printf ("    'Original' = latency-aligned input (err %.1e);  'Removed' = original - processed (err %.1e)\n", eo, er);
    }
    {
        RunOptions o; o.learnAt = 0.2; o.params.reduction = 0.5f; o.params.gateEnabled = true;
        o.params.gateThresholdDb = -40.0f; o.params.gateRangeDb = 30.0f;
        RunOptions off = o; off.params.gateEnabled = false;
        auto g = run ({ l }, o), ng = run ({ l }, off);
        const double closed = rmsDb (g[0], (size_t) (sr * 3), (size_t) (sr * 4.4)) - rmsDb (ng[0], (size_t) (sr * 3), (size_t) (sr * 4.4));
        const double open = rmsDb (g[0], (size_t) (sr * 5), n) - rmsDb (ng[0], (size_t) (sr * 5), n);
        double maxStep = 0;
        for (size_t i = (size_t) (sr * 3); i + 1 < n; ++i)
        {
            const double a = ng[0][i] != 0.0f ? g[0][i] / ng[0][i] : 1.0;
            const double b = ng[0][i + 1] != 0.0f ? g[0][i + 1] / ng[0][i + 1] : 1.0;
            if (std::abs (ng[0][i]) > 1e-4 && std::abs (ng[0][i + 1]) > 1e-4) maxStep = std::max (maxStep, std::abs (a - b));
        }
        metric ("gate: extra attenuation in pauses (dB)", closed, "dB");
        metric ("gate: level change during speech (dB)", open, "dB");
        metric ("gate: max per-sample gain step", maxStep);
        CHECK (closed < -25.0, "gate did not close (%g)", closed);
        CHECK (open > -1.0, "gate attenuated speech (%g)", open);
        CHECK (maxStep < 0.05, "gate gain jumps (click risk) %g", maxStep);
    }
}

static void testProfileAcrossRates()
{
    section ("Profile reuse across sample rates / FFT sizes");
    // learn at 48 kHz, restore into a 96 kHz engine (as after a project reload at another rate)
    DeadnoiseEngine a; RunOptions o; o.learnAt = 0.2; o.params.reduction = 1.0f;
    a.setParams (o.params); a.prepare (48000.0, 512, 1);
    Buf w48 = whiteNoise (48000 * 5, 0.003f, 61);
    run ({ w48 }, o, &a);
    NoiseProfile p = a.getDenoiser().getProfile();
    CHECK (p.isValid(), "no profile");

    DeadnoiseEngine b; RunOptions ob; ob.sr = 96000.0; ob.params.reduction = 1.0f;
    b.setParams (ob.params); b.prepare (96000.0, 512, 1);
    b.getDenoiser().setProfile (p);
    // white noise of the same density at twice the bandwidth has twice the variance
    Buf w96 = whiteNoise (96000 * 6, 0.003f * std::sqrt (2.0f), 62);
    auto out = run ({ w96 }, ob, &b);
    const double att = rmsDb (out[0], 96000 * 3, w96.size()) - rmsDb (w96, 96000 * 3, w96.size());
    metric ("48 kHz profile used at 96 kHz: attenuation (dB)", att, "dB");
    CHECK (att < -25.0, "cross-rate profile att %g", att);
    CHECK (b.getLatencySamples() == 2048, "96k latency %d", b.getLatencySamples());

    // re-preparing the same engine at other rates keeps it working
    for (double sr : { 44100.0, 96000.0, 48000.0 })
    {
        b.prepare (sr, 256, 2);
        Buf x = whiteNoise ((size_t) sr, 0.003f, 63);
        float* ptrs[2] = { x.data(), x.data() };
        Buf y = x; ptrs[1] = y.data();
        b.process (ptrs, 2, (int) x.size());
        bool finite = true; for (float v : x) finite &= std::isfinite (v);
        CHECK (finite, "after re-prepare at %g", sr);
    }
    std::printf ("    re-prepare 44.1 -> 96 -> 48 kHz with profile: ok\n");
}

static void testRandomAutomation()
{
    section ("Stress: random parameter automation every block (30 s, stereo)");
    const double sr = 44100.0;
    const size_t n = (size_t) (sr * 30);
    Buf x = hfHiss (n, 0.01f, 71);
    Buf v = vowel (n, sr, 110.0, 0.5f, 0.9);
    for (size_t i = 0; i < n; ++i) x[i] += v[i];
    RunOptions o; o.sr = sr; o.randomBlocks = true; o.learnAt = 1.0;
    std::mt19937 rng (5);
    o.perBlock = [&rng] (DeadnoiseEngine& e, size_t)
    {
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        EngineParams p;
        p.reduction = u (rng); p.maximumMode = u (rng) > 0.5f; p.voicePreservation = u (rng); p.mix = u (rng);
        p.gateEnabled = u (rng) > 0.5f; p.gateThresholdDb = -90.0f + 70.0f * u (rng); p.gateRangeDb = 80.0f * u (rng);
        p.inputGainDb = -24.0f + 48.0f * u (rng); p.outputGainDb = -24.0f + 48.0f * u (rng);
        p.monitor = (MonitorMode) (int) (u (rng) * 2.99f);
        e.setParams (p);
    };
    auto out = run ({ x, x }, o);
    bool finite = true; float pk = 0;
    for (auto& c : out) for (float s : c) { finite &= std::isfinite (s); pk = std::max (pk, std::abs (s)); }
    CHECK (finite, "non-finite");
    float inPk = 0; for (float s : x) inPk = std::max (inPk, std::abs (s));
    CHECK (pk < inPk * 251.2f * 1.1f, "unbounded output %g", pk);   // at most +24 dB in and +24 dB out
    std::printf ("    output finite: %s, peak %.2f (input peak x gains bounded)\n", finite ? "yes" : "NO", pk);
}

static void testCpu()
{
    section ("CPU cost");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const size_t n = (size_t) (sr * 20);
        Buf l = hfHiss (n, 0.005f, 81), r = hfHiss (n, 0.005f, 82);
        DeadnoiseEngine e; EngineParams p; p.reduction = 1.0f; p.gateEnabled = true;
        e.setParams (p); e.prepare (sr, 256, 2);
        float* ptrs[2] = { l.data(), r.data() };
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos + 256 <= n; pos += 256)
        {
            float* pp[2] = { ptrs[0] + pos, ptrs[1] + pos };
            e.process (pp, 2, 256);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double rtf = secs / (n / sr);
        std::printf ("    %6.0f Hz stereo, 256-sample blocks: %.2f %% of one CPU core (real-time factor %.4f)\n", sr, 100.0 * rtf, rtf);
        CHECK (rtf < 0.10, "too slow: %g", rtf);
    }
}

int main()
{
    std::printf ("DEADNOISE DSP test suite\n========================\n");
    testFFT();
    testReconstructionAndLatency();
    testSilenceAndInvalidInput();
    testBlockSizeInvariance();
    testNoAllocation();
    testLearning();
    testSuppression();
    testSpeechPreservation();
    testStereoAndMonitor();
    testProfileAcrossRates();
    testRandomAutomation();
    testCpu();
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    std::printf (g_failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}
