// DEADNOISE offline processor - runs the exact plugin DSP chain on a WAV file.
// Used by the evaluation scripts; also handy to audition settings offline.
//
//   deadnoise_cli --in noisy.wav --out clean.wav [options]
//     --learn-at SEC        press "Learn Background" at SEC seconds (default: off)
//     --reduction 0..1      NOISE REDUCTION (default 0.8)
//     --mode natural|max    PROCESSING MODE
//     --voice 0..1          VOICE PRESERVATION (default 0.5)
//     --mix 0..1            WET/DRY (default 1)
//     --gate THR_DB         enable gate with threshold
//     --gate-range DB       gate range (default 40)
//     --in-gain DB / --out-gain DB
//     --monitor processed|original|removed
//     --block N | --block random   host block size (default 512)
//     --compensate          remove the plugin latency from the output file
#include "../src/dsp/DeadnoiseEngine.h"
#include "WavIO.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <cmath>

using namespace deadnoise;

int main (int argc, char** argv)
{
    std::string in, out;
    double learnAt = -1.0;
    EngineParams p;
    int block = 512;
    bool randomBlocks = false, compensate = false;
    TuningOverride tune;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--in") in = next();
        else if (a == "--out") out = next();
        else if (a == "--learn-at") learnAt = std::atof (next().c_str());
        else if (a == "--reduction") p.reduction = (float) std::atof (next().c_str());
        else if (a == "--mode") p.maximumMode = next() == "max";
        else if (a == "--voice") p.voicePreservation = (float) std::atof (next().c_str());
        else if (a == "--mix") p.mix = (float) std::atof (next().c_str());
        else if (a == "--gate") { p.gateEnabled = true; p.gateThresholdDb = (float) std::atof (next().c_str()); }
        else if (a == "--gate-range") p.gateRangeDb = (float) std::atof (next().c_str());
        else if (a == "--in-gain") p.inputGainDb = (float) std::atof (next().c_str());
        else if (a == "--out-gain") p.outputGainDb = (float) std::atof (next().c_str());
        else if (a == "--monitor")
        {
            const std::string m = next();
            p.monitor = m == "original" ? MonitorMode::original : m == "removed" ? MonitorMode::removedNoise : MonitorMode::processed;
        }
        else if (a == "--block")
        {
            const std::string b = next();
            if (b == "random") randomBlocks = true; else block = std::atoi (b.c_str());
        }
        else if (a == "--compensate") compensate = true;
        else if (a == "--tune")
        {
            // --tune name=value  (gmin, oversub, ximin_db, alphadd, zlo, zhi, flo, fhi, qmax, hold, rel, deep)
            static const char* names[12] = { "gmin", "oversub", "ximin_db", "alphadd", "zlo", "zhi", "flo", "fhi", "qmax", "hold", "rel", "deep" };
            const std::string kv = next();
            const auto eq = kv.find ('=');
            bool found = false;
            for (int k = 0; k < 12 && eq != std::string::npos; ++k)
                if (kv.substr (0, eq) == names[k])
                {
                    float val = (float) std::atof (kv.c_str() + eq + 1);
                    if (k == 2) val = std::pow (10.0f, val / 10.0f);
                    tune.v[k] = val;
                    found = true;
                }
            if (! found) { std::fprintf (stderr, "bad --tune %s\n", kv.c_str()); return 2; }
        }
        else { std::fprintf (stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (in.empty() || out.empty()) { std::fprintf (stderr, "usage: deadnoise_cli --in a.wav --out b.wav [options]\n"); return 2; }

    wavio::Audio audio;
    if (! wavio::read (in, audio)) { std::fprintf (stderr, "cannot read %s\n", in.c_str()); return 1; }
    const int nch = std::min<int> ((int) audio.ch.size(), 2);
    const size_t frames = audio.frames();

    DeadnoiseEngine engine;
    engine.setParams (p);
    engine.prepare (audio.sampleRate, randomBlocks ? 4096 : block, nch);
    engine.getDenoiser().setTuningOverride (tune);
    const int latency = engine.getLatencySamples();

    std::vector<std::vector<float>> buf ((size_t) nch, std::vector<float> (frames + (size_t) latency, 0.0f));
    for (int c = 0; c < nch; ++c) std::copy (audio.ch[(size_t) c].begin(), audio.ch[(size_t) c].end(), buf[(size_t) c].begin());
    const size_t total = compensate ? frames + (size_t) latency : frames;

    std::mt19937 rng (1234);
    std::uniform_int_distribution<int> dist (1, 4096);
    bool learnRequested = false;
    double dspSeconds = 0.0;
    size_t pos = 0;
    while (pos < total)
    {
        const int n = (int) std::min<size_t> (total - pos, (size_t) (randomBlocks ? dist (rng) : block));
        if (learnAt >= 0.0 && ! learnRequested && (double) pos / audio.sampleRate >= learnAt)
        {
            engine.getDenoiser().requestLearn();
            learnRequested = true;
        }
        float* ptrs[2] = {};
        for (int c = 0; c < nch; ++c) ptrs[c] = buf[(size_t) c].data() + pos;
        const auto t0 = std::chrono::high_resolution_clock::now();
        engine.process (ptrs, nch, n);
        dspSeconds += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        engine.getDenoiser().serviceBackgroundTasks();   // background thread in the plugin
        pos += (size_t) n;
    }

    wavio::Audio o;
    o.sampleRate = audio.sampleRate;
    o.ch.assign ((size_t) nch, std::vector<float> (frames));
    for (int c = 0; c < nch; ++c)
        for (size_t i = 0; i < frames; ++i)
            o.ch[(size_t) c][i] = buf[(size_t) c][i + (compensate ? (size_t) latency : 0)];
    wavio::writeFloat (out, o);

    const auto st = engine.getDenoiser().getLearnState();
    const char* ls = st == LearnState::succeeded ? "succeeded" : st == LearnState::failedActivity ? "failed-activity"
                   : st == LearnState::failedNoSignal ? "failed-nosignal" : learnRequested ? "incomplete" : "not-requested";
    const NoiseProfile prof = engine.getDenoiser().getProfile();
    std::printf ("{\"latency\": %d, \"fft\": %d, \"learn\": \"%s\", \"accepted\": %.3f, \"profileDb\": %.2f, \"rtf\": %.5f}\n",
                 latency, engine.getDenoiser().getFftSize(), ls, prof.acceptedFraction, prof.levelDb,
                 dspSeconds / ((double) total / audio.sampleRate));
    return 0;
}
