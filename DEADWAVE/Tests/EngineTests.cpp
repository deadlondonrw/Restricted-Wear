// DEADWAVE headless engine tests. Renders audio through the real AudioProcessor and checks
// invariants that matter in a host: finite output, polyphony, envelopes, glide, pitch bend,
// band-limiting, Vertical Bias behaviour, real-time allocation freedom, state round-trips and presets.

#include "PluginProcessor.h"
#include "Presets/PresetManager.h"
#include "Core/Expression.h"
#include "DSP/WaveGen.h"
#include <chrono>
#include <cstdlib>
#include <new>

// ------------------------------------------------------------------ allocation tracking (per thread)
static thread_local bool tTrackAllocs = false;
static std::atomic<int> gAllocCount { 0 };

void* operator new (std::size_t n)
{
    if (tTrackAllocs) gAllocCount.fetch_add (1);
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    if (tTrackAllocs) gAllocCount.fetch_add (1);
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace dw;

static int failures = 0, checks = 0;
#define CHECK(cond, msg)                                                                   \
    do {                                                                                   \
        ++checks;                                                                          \
        if (! (cond)) { ++failures; std::printf ("  FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
    } while (0)

static void section (const char* name) { std::printf ("\n== %s\n", name); }

struct Rig
{
    std::unique_ptr<DeadwaveProcessor> proc;
    double sr;
    int block;

    Rig (double sampleRate = 48000.0, int blockSize = 512) : sr (sampleRate), block (blockSize)
    {
        proc = std::make_unique<DeadwaveProcessor>();
        proc->setPlayConfigDetails (0, 2, sr, block);
        proc->prepareToPlay (sr, block);
    }

    void set (const juce::String& id, float v)
    {
        const int idx = Registry::get().indexOf (id);
        if (idx < 0) { std::printf ("  unknown param %s\n", id.toRawUTF8()); ++failures; return; }
        proc->setValue (idx, v);
    }

    void pump (int ms = 60) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    /** Renders seconds of audio. events: (sampleOffset, message). Returns interleaved stereo in out. */
    void render (double seconds, std::vector<std::pair<int, juce::MidiMessage>> events, std::vector<float>& outL,
                 std::vector<float>& outR, bool trackAllocs = false, int* allocs = nullptr)
    {
        const int total = (int) (seconds * sr);
        outL.assign ((size_t) total, 0.0f);
        outR.assign ((size_t) total, 0.0f);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.ensureSize (4096);
        int pos = 0;
        int localAllocs = 0;
        while (pos < total)
        {
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            midi.clear();
            for (auto& e : events)
                if (e.first >= pos && e.first < pos + n)
                    midi.addEvent (e.second, e.first - pos);
            if (trackAllocs)
            {
                const int before = gAllocCount.load();
                tTrackAllocs = true;
                proc->processBlock (buf, midi);
                tTrackAllocs = false;
                localAllocs += gAllocCount.load() - before;
            }
            else
                proc->processBlock (buf, midi);
            for (int i = 0; i < n; ++i)
            {
                outL[(size_t) (pos + i)] = buf.getSample (0, i);
                outR[(size_t) (pos + i)] = buf.getSample (1, i);
            }
            pos += n;
        }
        if (allocs) *allocs = localAllocs;
    }
};

static bool allFinite (const std::vector<float>& v)
{
    for (auto x : v) if (! std::isfinite (x)) return false;
    return true;
}
static float peakOf (const std::vector<float>& v, size_t a = 0, size_t b = 0)
{
    if (b == 0 || b > v.size()) b = v.size();
    float p = 0.0f;
    for (size_t i = a; i < b; ++i) p = juce::jmax (p, std::abs (v[i]));
    return p;
}
static float rmsOf (const std::vector<float>& v, size_t a, size_t b)
{
    b = juce::jmin (b, v.size());
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return b > a ? (float) std::sqrt (s / (double) (b - a)) : 0.0f;
}
/** Frequency estimate by counting rising zero crossings with linear interpolation. */
static double pitchOf (const std::vector<float>& v, size_t a, size_t b, double sr)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = a + 1; i < b && i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + v[i - 1] / (double) (v[i - 1] - v[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count * sr / (last - first) : 0.0;
}
static std::vector<double> spectrumDb (const std::vector<float>& v, size_t start, int n)
{
    std::vector<std::complex<double>> buf ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
        buf[(size_t) i] = { (start + (size_t) i < v.size() ? v[start + (size_t) i] : 0.0) * w, 0.0 };
    }
    fft (buf, false);
    std::vector<double> db ((size_t) n / 2);
    for (int i = 0; i < n / 2; ++i) db[(size_t) i] = 20.0 * std::log10 (std::abs (buf[(size_t) i]) + 1e-12);
    return db;
}
static juce::MidiMessage on (int note, float vel = 0.8f) { return juce::MidiMessage::noteOn (1, note, vel); }
static juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }

// ============================================================================================= tests
static void testRegistry()
{
    section ("Parameter registry");
    const auto& reg = Registry::get();
    CHECK ((int) reg.specs().size() == P::Count, "spec count matches layout");
    std::set<juce::String> ids;
    for (const auto& s : reg.specs()) ids.insert (s.id);
    CHECK ((int) ids.size() == P::Count, "parameter IDs are unique");
    CHECK (reg.spec (P::osc (2, OscP::Bias)).id == "osc3_bias", "index layout osc3_bias");
    CHECK (reg.spec (P::fx (0, FxP::RevMix)).id == "fx_rev_mix", "index layout fx_rev_mix");
    CHECK (reg.spec (P::seqStep (15, SeqStepP::Gate)).id == "seq16_gate", "index layout seq16_gate");
    std::printf ("  %d parameters, %d modulation destinations\n", P::Count, (int) reg.modDestinations().size());
}

static void testExpression()
{
    section ("Expression engine");
    Expression e;
    CHECK (e.compile ("sin(2*pi*x)").isEmpty(), "compiles sin");
    Expression::Vars v; v.x = 0.25;
    CHECK (std::abs (e.evaluate (v) - 1.0) < 1e-9, "sin(2 pi 0.25) == 1");
    CHECK (e.compile ("lerp(saw(x), sqr(x), f) ^ 2 * -1").isEmpty(), "compiles nested functions");
    CHECK (e.compile ("sin(").isNotEmpty(), "reports syntax errors");
    CHECK (e.compile ("foo(x)").isNotEmpty(), "reports unknown functions");
}

static void testWavetable()
{
    section ("Wavetable mipmaps");
    auto saw = wavegen::saw();
    auto wt = Wavetable::fromFrames ({ saw }, "saw");
    CHECK (wt->getNumFrames() == 1, "one frame");
    // Level 19 keeps only the fundamental: should be a sine of the saw's first-harmonic amplitude.
    const float* d = wt->frameData (19, 0);
    const int n = Wavetable::levelSize (19);
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (d[i]));
    CHECK (pk > 0.3f && pk < 1.0f, "top level holds the fundamental only");
    CHECK (std::abs (d[-1] - d[n - 1]) < 1e-6f && std::abs (d[n] - d[0]) < 1e-6f, "guard samples wrap");

    std::vector<float> dc (kWaveSize, 0.5f);
    for (int i = 0; i < kWaveSize; ++i) dc[(size_t) i] += 0.5f * std::sin (2.0f * kPi * i / kWaveSize);
    auto wt2 = Wavetable::fromFrames ({ dc }, "dc");
    CHECK (std::abs (wt2->frameDc (0) - 0.5f) < 1e-4f, "frame DC reported");
}

static void testPolyphony()
{
    section ("Polyphony and release");
    Rig r;
    r.set ("env1_release", 0.05f);
    std::vector<float> l, rr;
    r.render (1.5, { { 0, on (60) }, { 0, on (64) }, { 0, on (67) }, { 24000, off (60) }, { 24000, off (64) }, { 24000, off (67) } }, l, rr);
    CHECK (allFinite (l) && allFinite (rr), "finite output");
    const float rmsHeld = rmsOf (l, 4800, 20000);
    CHECK (rmsHeld > 0.02f, "chord produces sound");
    CHECK (rmsOf (l, 60000, 72000) < 1e-4f, "silence after release");
    CHECK (r.proc->getEngine().getActiveVoiceCount() == 0, "voices freed after release");
    std::printf ("  chord rms %.4f, peak %.3f\n", rmsHeld, peakOf (l));

    // voice count while holding
    Rig r2;
    std::vector<std::pair<int, juce::MidiMessage>> ev;
    for (int k = 0; k < 8; ++k) ev.push_back ({ 0, on (48 + k * 3) });
    r2.render (0.2, ev, l, rr);
    CHECK (r2.proc->getEngine().getActiveVoiceCount() == 8, "eight held notes use eight voices");
    r2.set ("polyphony", 4.0f);
    ev.clear();
    for (int k = 0; k < 8; ++k) ev.push_back ({ 0, on (72 + k) });
    r2.render (0.2, ev, l, rr);
    CHECK (allFinite (l), "voice stealing stays finite");
}

static void testPitchAndGlide()
{
    section ("Pitch, pitch bend and glide");
    Rig r;
    r.set ("osc1_wtpos", 0.0f); // sine frame of Basic Shapes
    std::vector<float> l, rr;
    r.render (0.5, { { 0, on (69) } }, l, rr);
    const double f = pitchOf (l, 4800, 24000, r.sr);
    std::printf ("  A4 measured %.2f Hz\n", f);
    CHECK (std::abs (f - 440.0) < 0.5, "A4 = 440 Hz");

    Rig b;
    b.set ("bend_up", 12.0f);
    b.render (0.5, { { 0, on (57) }, { 0, juce::MidiMessage::pitchWheel (1, 16383) } }, l, rr);
    const double fb = pitchOf (l, 9600, 24000, b.sr);
    std::printf ("  A3 + full bend (12 st) measured %.2f Hz\n", fb);
    CHECK (std::abs (fb - 440.0) < 1.0, "pitch bend range applied");

    Rig g;
    g.set ("voice_mode", 2.0f);  // legato
    g.set ("glide_mode", 2.0f);  // overlap only
    g.set ("glide_time", 0.2f);
    // note 57 (220 Hz) then overlapping note 69 (440 Hz)
    g.render (0.9, { { 0, on (57) }, { 14400, on (69) }, { 40000, off (57) } }, l, rr);
    const double before = pitchOf (l, 4800, 14000, g.sr);
    const double mid = pitchOf (l, 14400 + 3800, 14400 + 5800, g.sr);
    const double after = pitchOf (l, 14400 + 12000, 14400 + 24000, g.sr);
    std::printf ("  glide: before %.1f Hz, mid %.1f Hz, after %.1f Hz\n", before, mid, after);
    CHECK (std::abs (before - 220.0) < 1.0, "legato start pitch");
    CHECK (mid > 240.0 && mid < 420.0, "pitch is between notes during glide");
    CHECK (std::abs (after - 440.0) < 1.0, "glide reaches target");

    // Mono return-to-held: release top note returns to held lower note
    Rig m;
    m.set ("voice_mode", 1.0f);
    m.render (0.8, { { 0, on (57) }, { 9600, on (69) }, { 19200, off (69) } }, l, rr);
    const double ret = pitchOf (l, 28800, 38000, m.sr);
    std::printf ("  mono return to held note: %.1f Hz\n", ret);
    CHECK (std::abs (ret - 220.0) < 1.0, "mono returns to held note");
}

static void testAliasing()
{
    section ("Band-limiting (saw at 3.5 kHz)");
    for (int q = 0; q < 2; ++q)
    {
        Rig r (48000.0, 512);
        r.set ("osc1_wtpos", 0.5f); // saw frame of Basic Shapes
        r.set ("quality", (float) q);
        r.set ("vel_sens", 0.0f);
        std::vector<float> l, rr;
        const int note = 106; // ~3729 Hz
        r.render (0.6, { { 0, on (note) } }, l, rr);
        const int n = 16384;
        auto db = spectrumDb (l, 8000, n);
        const double f0 = midiToHz ((float) note);
        double harmonicPeak = -300.0, aliasPeak = -300.0;
        for (int k = 2; k < n / 2 - 2; ++k)
        {
            const double freq = (double) k * r.sr / n;
            if (freq > 20000.0) break;
            const double h = freq / f0;
            const bool nearHarmonic = std::abs (h - std::round (h)) * f0 < 40.0;
            if (nearHarmonic) harmonicPeak = juce::jmax (harmonicPeak, db[(size_t) k]);
            else aliasPeak = juce::jmax (aliasPeak, db[(size_t) k]);
        }
        const double rel = aliasPeak - harmonicPeak;
        std::printf ("  %s: worst non-harmonic component %.1f dB below fundamental\n", q ? "ULTRA" : "STANDARD", -rel);
        CHECK (rel < -60.0, "aliasing below -60 dB");
    }
}

static void testBias()
{
    section ("Vertical Bias acts on the real waveform");
    // A sine with soft drive: bias makes the waveform asymmetric -> even harmonics appear.
    auto secondHarmonic = [] (float bias)
    {
        Rig r;
        r.set ("osc1_wtpos", 0.0f);
        r.set ("osc1_bias", bias);
        r.set ("osc1_drive", 0.6f);
        r.set ("vel_sens", 0.0f);
        std::vector<float> l, rr;
        r.render (0.6, { { 0, on (57) } }, l, rr);
        const int n = 16384;
        auto db = spectrumDb (l, 8000, n);
        auto at = [&] (double hz) { const int k = (int) std::round (hz * n / r.sr); double m = -300; for (int j = k - 2; j <= k + 2; ++j) m = juce::jmax (m, db[(size_t) j]); return m; };
        return at (440.0) - at (220.0);
    };
    const double noBias = secondHarmonic (0.0f), withBias = secondHarmonic (0.4f);
    std::printf ("  2nd harmonic relative to fundamental: bias 0 -> %.1f dB, bias 0.4 -> %.1f dB\n", noBias, withBias);
    CHECK (noBias < -50.0, "symmetric sine has no even harmonics");
    CHECK (withBias > -30.0, "bias produces even harmonics through drive");

    // Output stays centred: DC is managed by the voice DC blocker.
    Rig r;
    r.set ("osc1_wtpos", 0.0f);
    r.set ("osc1_bias", 0.8f);
    std::vector<float> l, rr;
    r.render (1.0, { { 0, on (57) } }, l, rr);
    double mean = 0.0;
    for (size_t i = 24000; i < 48000; ++i) mean += l[i];
    mean /= 24000.0;
    std::printf ("  output mean with bias 0.8: %.5f\n", mean);
    CHECK (std::abs (mean) < 0.01, "voice output DC is removed");
}

static void testRatesAndBlocks()
{
    section ("Sample rates and block sizes");
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int blocks[] = { 1, 17, 64, 333, 2048 };
    for (double sr : rates)
        for (int bs : blocks)
        {
            if (sr > 96000.0 && bs == 1) continue; // slow, adds nothing
            Rig r (sr, bs);
            r.set ("osc1_unison", 4.0f);
            r.set ("filter1_on", 1.0f);
            r.set ("fx_rev_on", 1.0f);
            r.set ("fx_dly_on", 1.0f);
            r.set ("quality", bs == 333 ? 1.0f : 0.0f);
            std::vector<float> l, rr;
            const int s = (int) (0.05 * sr);
            r.render (0.25, { { 0, on (60) }, { s, on (67) }, { 2 * s, off (60) } }, l, rr);
            const bool ok = allFinite (l) && allFinite (rr) && rmsOf (l, (size_t) s, (size_t) (3 * s)) > 0.005f;
            if (! ok) std::printf ("  failed at %.0f Hz / %d\n", sr, bs);
            CHECK (ok, "finite non-silent output at this rate/block size");
        }
}

static void testEffectsStress()
{
    section ("Effects stress (all units, extreme settings)");
    Rig r;
    const char* units[] = { "dist", "sat", "cho", "pha", "fla", "dly", "rev", "cmp", "eq", "img", "crs" };
    for (auto* u : units) r.set (juce::String ("fx_") + u + "_on", 1.0f);
    r.set ("fx_dist_drive", 48.0f);
    r.set ("fx_cho_fb", 0.95f);
    r.set ("fx_fla_fb", -0.95f);
    r.set ("fx_pha_fb", 0.95f);
    r.set ("fx_dly_fb", 0.98f);
    r.set ("fx_rev_decay", 20.0f);
    r.set ("fx_eq_lowgain", 18.0f);
    r.set ("fx_crs_bits", 2.0f);
    r.set ("osc1_fxon", 1.0f);
    r.set ("osc1fx_rev_on", 1.0f);
    r.set ("osc1fx_dist_on", 1.0f);
    std::vector<float> l, rr;
    r.render (3.0, { { 0, on (48) }, { 0, on (60) }, { 40000, off (48) }, { 40000, off (60) } }, l, rr);
    CHECK (allFinite (l) && allFinite (rr), "finite with every effect at extreme settings");
    CHECK (peakOf (l) <= 8.0f, "safety ceiling holds");
    std::printf ("  peak %.2f\n", peakOf (l));
}

static void testModulation()
{
    section ("Modulation matrix");
    Rig r;
    r.set ("filter1_on", 1.0f);
    r.set ("filter1_cutoff", 300.0f);
    r.set ("lfo1_sync", 0.0f);
    r.set ("lfo1_rate", 2.0f);
    r.set ("osc1_wtpos", 0.5f);
    const int slot = r.proc->addModulation (ModSrc::Lfo1, "filter1_cutoff", 0.6f);
    CHECK (slot == 0, "routing added to first slot");
    std::vector<float> l, rr;
    r.render (1.2, { { 0, on (48) } }, l, rr);
    float lo = 1e9f, hi = 0.0f;
    for (int w = 0; w < 20; ++w)
    {
        const float x = rmsOf (l, (size_t) (4800 + w * 2400), (size_t) (4800 + (w + 1) * 2400));
        lo = juce::jmin (lo, x); hi = juce::jmax (hi, x);
    }
    std::printf ("  rms range with LFO on cutoff: %.4f .. %.4f\n", lo, hi);
    CHECK (hi > lo * 1.5f, "LFO -> cutoff audibly modulates");

    r.proc->clearRouting (slot);
    CHECK (! r.proc->getRouting (slot).isUsed(), "routing cleared");
}

static void testMidiFx()
{
    section ("Arpeggiator / sequencer");
    MidiFx fx;
    fx.prepare (48000.0);
    std::vector<float> v ((size_t) P::Count);
    for (int i = 0; i < P::Count; ++i) v[(size_t) i] = Registry::get().spec (i).def;
    v[(size_t) P::arp (ArpP::MidiFxMode)] = 1.0f;
    v[(size_t) P::arp (ArpP::ArpRate)] = 10.0f; // 1/16
    juce::MidiBuffer in, out;
    out.ensureSize (4096);
    in.addEvent (on (60), 0);
    in.addEvent (on (64), 0);
    in.addEvent (on (67), 0);
    int noteOns = 0;
    std::vector<int> notes;
    double ppq = 0.0;
    for (int b = 0; b < 94; ++b) // ~1 s at 512
    {
        fx.process (in, out, 512, 120.0, true, ppq, v.data());
        for (const auto m : out)
            if (m.getMessage().isNoteOn()) { ++noteOns; notes.push_back (m.getMessage().getNoteNumber()); }
        in.clear();
        ppq += 512.0 / 48000.0 * 2.0;
    }
    std::printf ("  arp 1/16 @120 BPM for 1 s: %d notes (expected ~8)\n", noteOns);
    CHECK (noteOns >= 7 && noteOns <= 9, "arp step count");
    CHECK (notes.size() >= 3 && notes[0] == 60 && notes[1] == 64 && notes[2] == 67, "arp up order");

    MidiFx sq;
    sq.prepare (48000.0);
    v[(size_t) P::arp (ArpP::MidiFxMode)] = 2.0f;
    v[(size_t) P::seqStep (1, SeqStepP::Pitch)] = 7.0f;
    in.clear();
    in.addEvent (on (48), 100);
    notes.clear();
    for (int b = 0; b < 30; ++b)
    {
        sq.process (in, out, 512, 120.0, false, 0.0, v.data());
        for (const auto m : out)
            if (m.getMessage().isNoteOn()) notes.push_back (m.getMessage().getNoteNumber());
        in.clear();
    }
    CHECK (notes.size() >= 2 && notes[0] == 48 && notes[1] == 55, "sequencer transposes from held root (free clock)");
}

static void testRealtimeAllocations()
{
    section ("Real-time safety (heap allocations in processBlock)");
    Rig r (48000.0, 256);
    r.set ("osc1_unison", 16.0f);
    r.set ("osc2_on", 1.0f);
    r.set ("osc2_fxon", 1.0f);
    r.set ("osc2fx_cho_on", 1.0f);
    r.set ("filter1_on", 1.0f);
    r.set ("fx_rev_on", 1.0f);
    r.set ("fx_dly_on", 1.0f);
    r.set ("quality", 1.0f);
    r.set ("midifx_mode", 1.0f);
    r.proc->addModulation (ModSrc::Lfo1, "osc1_wtpos", 0.5f);
    std::vector<float> l, rr;
    r.render (0.2, { { 0, on (60) } }, l, rr); // warm up
    int allocs = -1;
    std::vector<std::pair<int, juce::MidiMessage>> ev;
    for (int k = 0; k < 12; ++k) ev.push_back ({ k * 2000, on (48 + k) });
    for (int k = 0; k < 12; ++k) ev.push_back ({ 30000 + k * 1000, off (48 + k) });
    r.render (1.0, ev, l, rr, true, &allocs);
    std::printf ("  allocations during 1 s of heavy processing: %d\n", allocs);
    CHECK (allocs == 0, "no heap allocation on the audio thread");
}

static void testState()
{
    section ("State round-trip");
    Rig a;
    a.set ("osc1_bias", 0.33f);
    a.set ("filter1_type", 9.0f);
    a.set ("fx_rev_on", 1.0f);
    a.proc->getWave (1).generateRandom (wavegen::RandomStyle::Segments, 99u, 0.6f, 8, false);
    a.proc->addModulation (ModSrc::Env2, "osc2_wtpos", -0.4f);
    a.proc->setLfoPoints (2, { { 0.0f, 0.2f, 0.3f }, { 0.6f, 0.9f, -0.2f } });
    juce::MemoryBlock mb;
    a.proc->getStateInformation (mb);

    Rig b;
    b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
    const auto sa = a.proc->captureState().toXmlString();
    const auto sb = b.proc->captureState().toXmlString();
    CHECK (sa == sb, "captured state identical after reload");
    CHECK (std::abs (b.proc->getValue (P::osc (0, OscP::Bias)) - 0.33f) < 1e-5f, "parameter restored");
    CHECK (b.proc->getWave (1).getNumFrames() == 8, "edited wavetable restored");
    CHECK (b.proc->getRouting (0).dstId == "osc2_wtpos", "matrix restored");
    std::printf ("  state size: %d bytes\n", (int) mb.getSize());
}

static void testFactoryPresets()
{
    section ("Factory presets");
    const auto& list = factoryPresets();
    std::printf ("  %d factory presets\n", (int) list.size());
    Rig r (48000.0, 512);
    int bad = 0, silent = 0, loud = 0;
    double worstRtf = 0.0;
    juce::String worstName;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        r.proc->getPresetManager().load (i);
        r.pump (5);
        // Rebuild tables synchronously for the test: wait until builder publishes.
        for (int k = 0; k < 40; ++k) r.pump (5);
        std::vector<float> l, rr;
        auto t0 = std::chrono::high_resolution_clock::now();
        r.render (1.5, { { 0, on (48) }, { 0, on (55) }, { 0, on (60) }, { 0, on (64) }, { 48000, off (48) }, { 48000, off (55) }, { 48000, off (60) }, { 48000, off (64) } }, l, rr);
        auto t1 = std::chrono::high_resolution_clock::now();
        const double rtf = std::chrono::duration<double> (t1 - t0).count() / 1.5;
        if (rtf > worstRtf) { worstRtf = rtf; worstName = list[(size_t) i].name; }
        const bool fin = allFinite (l) && allFinite (rr);
        const float rms = rmsOf (l, 2400, 48000) + rmsOf (rr, 2400, 48000);
        const float pk = juce::jmax (peakOf (l), peakOf (rr));
        if (! fin) { ++bad; std::printf ("  NON-FINITE: %s\n", list[(size_t) i].name.toRawUTF8()); }
        if (rms < 0.003f) { ++silent; std::printf ("  near-silent: %s (rms %.5f)\n", list[(size_t) i].name.toRawUTF8(), rms); }
        if (pk > 2.5f) { ++loud; std::printf ("  hot: %s (peak %.2f)\n", list[(size_t) i].name.toRawUTF8(), pk); }
        r.proc->getEngine().panic();
    }
    CHECK (bad == 0, "every factory preset renders finite audio");
    CHECK (silent == 0, "every factory preset makes sound");
    CHECK (loud == 0, "no factory preset peaks above +8 dBFS");
    std::printf ("  heaviest preset: %s at %.1f%% of one core (4-note chord)\n", worstName.toRawUTF8(), worstRtf * 100.0);
}

static void testPerformance()
{
    section ("Performance");
    for (int q = 0; q < 2; ++q)
    {
        Rig r (48000.0, 256);
        for (int o = 0; o < 4; ++o)
        {
            const juce::String p = "osc" + juce::String (o + 1) + "_";
            r.set (p + "on", 1.0f);
            r.set (p + "unison", 8.0f);
        }
        r.set ("filter1_on", 1.0f);
        r.set ("quality", (float) q);
        std::vector<std::pair<int, juce::MidiMessage>> ev;
        for (int k = 0; k < 16; ++k) ev.push_back ({ 0, on (36 + k * 3) });
        std::vector<float> l, rr;
        auto t0 = std::chrono::high_resolution_clock::now();
        r.render (2.0, ev, l, rr);
        auto t1 = std::chrono::high_resolution_clock::now();
        const double rtf = std::chrono::duration<double> (t1 - t0).count() / 2.0;
        std::printf ("  %s: 16 voices x 4 osc x 8 unison = 512 oscillators -> %.1f%% of one core\n", q ? "ULTRA" : "STANDARD", rtf * 100.0);
        CHECK (allFinite (l), "finite under load");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const bool quick = argc > 1 && juce::String (argv[1]) == "--quick";

    testRegistry();
    testExpression();
    testWavetable();
    testPolyphony();
    testPitchAndGlide();
    testAliasing();
    testBias();
    testModulation();
    testMidiFx();
    testState();
    testRealtimeAllocations();
    testEffectsStress();
    if (! quick)
    {
        testRatesAndBlocks();
        testFactoryPresets();
        testPerformance();
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
