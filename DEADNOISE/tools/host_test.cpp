// DEADNOISE host-level test: loads the compiled DEADNOISE.vst3 through JUCE's
// VST3 hosting code (the same interfaces FL Studio uses) and checks it like a DAW.
//
//   deadnoise_host_test path/to/DEADNOISE.vst3 [--no-gui]
//
// Exit code 0 = all checks passed.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DeadnoiseEngine.h"

#include <random>

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("    FAIL %s -- ", #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

using Buf = std::vector<float>;

static Buf hiss (size_t n, float rms, unsigned seed, double sr)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, 1.0f);
    Buf w (n + 1), b (n);
    for (auto& x : w) x = d (rng);
    const float a = (float) (sr / (2.0 * juce::MathConstants<double>::pi * 2000.0)) * 0.25f;
    double s = 0;
    for (size_t i = 0; i < n; ++i) { b[i] = 0.35f * w[i + 1] + a * (w[i + 1] - w[i]); s += (double) b[i] * b[i]; }
    const float g = rms / (float) std::sqrt (s / (double) n);
    for (auto& x : b) x *= g;
    return b;
}

static Buf vowel (size_t n, double sr, double f0, float amp)
{
    Buf b (n, 0.0f);
    for (int h = 1; h * f0 < std::min (7000.0, sr * 0.45); ++h)
    {
        const double env = 0.1 + std::exp (-std::pow ((h * f0 - 700.0) / 300.0, 2.0)) + std::exp (-std::pow ((h * f0 - 2500.0) / 400.0, 2.0));
        for (size_t i = 0; i < n; ++i) b[i] += (float) (env * std::sin (2.0 * juce::MathConstants<double>::pi * h * f0 * (double) i / sr + 0.3 * h * h));
    }
    float mx = 0; for (float x : b) mx = std::max (mx, std::abs (x));
    for (size_t i = 0; i < n; ++i) b[i] *= amp / mx * (float) std::max (0.0, std::sin (2.0 * juce::MathConstants<double>::pi * 1.3 * (double) i / sr));
    return b;
}

static double rmsDb (const Buf& b, size_t a, size_t e)
{
    double s = 0; e = std::min (e, b.size());
    for (size_t i = a; i < e; ++i) s += (double) b[i] * b[i];
    return 10.0 * std::log10 (s / (double) std::max<size_t> (1, e - a) + 1e-30);
}

static juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name) return prm;
    return nullptr;
}

static void setParam (juce::AudioPluginInstance& p, const juce::String& name, float normalised)
{
    if (auto* prm = findParam (p, name)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (normalised); prm->endChangeGesture(); }
    else { ++failures; std::printf ("    FAIL parameter '%s' not found\n", name.toRawUTF8()); }
}

// Process mono/stereo buffers through the hosted plugin with a given block size.
static std::vector<Buf> processHosted (juce::AudioPluginInstance& p, const std::vector<Buf>& in, int block)
{
    const int nch = (int) in.size();
    std::vector<Buf> out = in;
    juce::AudioBuffer<float> buf (std::max (nch, p.getTotalNumOutputChannels()), block);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in[0].size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in[0].size() - pos);
        buf.setSize (buf.getNumChannels(), n, false, false, true);
        buf.clear();
        for (int c = 0; c < nch; ++c) buf.copyFrom (c, 0, in[(size_t) c].data() + pos, n);
        p.processBlock (buf, midi);
        for (int c = 0; c < nch; ++c) std::copy (buf.getReadPointer (c), buf.getReadPointer (c) + n, out[(size_t) c].data() + pos);
    }
    return out;
}

// Inject a noise profile into the hosted plugin's state via the VST3 IComponent
// state (exactly the data a DAW saves in its project).
static bool injectProfile (juce::AudioPluginInstance& p, const deadnoise::NoiseProfile& profile)
{
    juce::MemoryBlock hostState;
    p.getStateInformation (hostState);
    auto outer = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
    if (outer == nullptr) return false;
    auto* comp = outer->getChildByName ("IComponent");
    if (comp == nullptr) return false;
    juce::MemoryBlock compData;
    compData.fromBase64Encoding (comp->getAllSubText());
    auto inner = juce::AudioProcessor::getXmlFromBinary (compData.getData(), (int) compData.getSize());
    if (inner == nullptr) return false;
    if (auto* old = inner->getChildByName ("NOISE_PROFILE")) inner->removeChildElement (old, true);
    const auto bytes = profile.serialise();
    juce::MemoryBlock mb (bytes.data(), bytes.size());
    inner->createNewChildElement ("NOISE_PROFILE")->setAttribute ("data", mb.toBase64Encoding());
    juce::MemoryBlock newComp;
    juce::AudioProcessor::copyXmlToBinary (*inner, newComp);
    comp->deleteAllTextElements();
    comp->addTextElement (newComp.toBase64Encoding());
    juce::MemoryBlock newHost;
    juce::AudioProcessor::copyXmlToBinary (*outer, newHost);
    p.setStateInformation (newHost.getData(), (int) newHost.getSize());
    return true;
}

static bool stateHasProfile (juce::AudioPluginInstance& p)
{
    juce::MemoryBlock hostState;
    p.getStateInformation (hostState);
    auto outer = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
    if (outer == nullptr) return false;
    auto* comp = outer->getChildByName ("IComponent");
    if (comp == nullptr) return false;
    juce::MemoryBlock compData;
    compData.fromBase64Encoding (comp->getAllSubText());
    auto inner = juce::AudioProcessor::getXmlFromBinary (compData.getData(), (int) compData.getSize());
    return inner != nullptr && inner->getChildByName ("NOISE_PROFILE") != nullptr;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: deadnoise_host_test DEADNOISE.vst3 [--no-gui]\n"); return 2; }
    const bool gui = ! (argc > 2 && juce::String (argv[2]) == "--no-gui");
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String path = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).getFullPathName();
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);
    std::printf ("[scan] %s\n", path.toRawUTF8());
    CHECK (types.size() == 1, "found %d plugin types", types.size());
    if (types.isEmpty()) return 1;
    const auto& desc = *types[0];
    std::printf ("    name '%s'  vendor '%s'  category '%s'  version %s  ins %d outs %d  instrument %d\n",
                 desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8(),
                 desc.version.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels, (int) desc.isInstrument);
    CHECK (desc.name == "DEADNOISE", "name");
    CHECK (! desc.isInstrument, "registered as instrument");

    // ------------------------------------------------------------------ layouts / latency / processing
    struct Config { double sr; int block; int ch; int expectLatency; };
    const Config configs[] = { { 44100.0, 512, 2, 1024 }, { 48000.0, 256, 2, 1024 }, { 48000.0, 333, 1, 1024 },
                               { 96000.0, 1024, 2, 2048 }, { 48000.0, 64, 2, 1024 }, { 44100.0, 4096, 1, 1024 } };
    for (const auto& cfg : configs)
    {
        juce::String err;
        auto inst = format.createInstanceFromDescription (desc, cfg.sr, cfg.block, err);
        CHECK (inst != nullptr, "instantiate: %s", err.toRawUTF8());
        if (inst == nullptr) continue;
        auto layout = inst->getBusesLayout();
        layout.inputBuses.getReference (0) = cfg.ch == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        layout.outputBuses.getReference (0) = layout.inputBuses[0];
        CHECK (inst->setBusesLayout (layout), "%d-channel layout rejected", cfg.ch);
        inst->prepareToPlay (cfg.sr, cfg.block);
        const int lat = inst->getLatencySamples();
        CHECK (lat == cfg.expectLatency, "latency %d (expected %d)", lat, cfg.expectLatency);

        // 0 % reduction: must be a pure delay (null test through the host)
        setParam (*inst, "Noise Reduction", 0.0f);
        const size_t n = (size_t) (cfg.sr * 3);
        Buf h = hiss (n, 0.01f, 3, cfg.sr);
        std::vector<Buf> in ((size_t) cfg.ch, h);
        auto out = processHosted (*inst, in, cfg.block);
        double maxErr = 0;
        for (size_t i = (size_t) lat + 4800; i < n; ++i) maxErr = std::max (maxErr, (double) std::abs (out[0][i] - h[i - (size_t) lat]));
        CHECK (maxErr < 1e-4, "0 %% reduction is not transparent (err %g)", maxErr);

        // adaptive suppression of stationary hiss at default 80 %
        setParam (*inst, "Noise Reduction", 0.8f);
        Buf h2 = hiss (n * 2, 0.003f, 4, cfg.sr);
        auto out2 = processHosted (*inst, std::vector<Buf> ((size_t) cfg.ch, h2), cfg.block);
        const double att = rmsDb (out2[0], n, 2 * n) - rmsDb (h2, n, 2 * n);
        std::printf ("[process] %6.0f Hz  block %4d  %d ch  latency %4d  transparency err %.1e  hiss attenuation %6.1f dB\n",
                     cfg.sr, cfg.block, cfg.ch, lat, maxErr, att);
        CHECK (att < -20.0, "attenuation only %g dB", att);
        bool finite = true;
        for (auto& c : out2) for (float v : c) finite &= std::isfinite (v);
        CHECK (finite, "non-finite output");
        inst->releaseResources();
    }

    // ------------------------------------------------------------------ state + learned profile through IComponent
    {
        const double sr = 48000.0;
        juce::String err;
        auto inst = format.createInstanceFromDescription (desc, sr, 512, err);
        inst->prepareToPlay (sr, 512);

        // learn a profile offline with the DSP core (equivalent to pressing Learn)
        Buf h = hiss ((size_t) (sr * 12), 0.003f, 9, sr);
        deadnoise::DeadnoiseEngine eng;
        eng.prepare (sr, 512, 1);
        eng.getDenoiser().requestLearn();
        for (size_t pos = 0; pos + 512 <= (size_t) (sr * 5); pos += 512)
        {
            Buf tmp (h.begin() + (long) pos, h.begin() + (long) pos + 512);
            float* ptr = tmp.data();
            eng.process (&ptr, 1, 512);
            eng.getDenoiser().serviceBackgroundTasks();
        }
        const auto profile = eng.getDenoiser().getProfile();
        CHECK (profile.isValid(), "offline learn failed");

        CHECK (injectProfile (*inst, profile), "could not inject profile");
        setParam (*inst, "Noise Reduction", 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);   // profile handover is lock-free, give it a block or two

        auto out = processHosted (*inst, { h, h }, 512);
        const double att = rmsDb (out[0], (size_t) (sr * 6), h.size()) - rmsDb (h, (size_t) (sr * 6), h.size());
        std::printf ("[state] learned profile restored via IComponent state: attenuation %6.1f dB (Natural 100 %%)\n", att);
        CHECK (att < -29.0, "profile from state not used (%g dB)", att);
        CHECK (stateHasProfile (*inst), "profile missing from saved state");

        // parameters survive a save/load into a fresh instance
        setParam (*inst, "Voice Preservation", 0.73f);
        setParam (*inst, "Processing Mode", 1.0f);
        setParam (*inst, "Noise Gate", 1.0f);
        juce::MemoryBlock saved;
        inst->getStateInformation (saved);
        auto inst2 = format.createInstanceFromDescription (desc, sr, 512, err);
        inst2->prepareToPlay (sr, 512);
        inst2->setStateInformation (saved.getData(), (int) saved.getSize());
        const float vp = findParam (*inst2, "Voice Preservation")->getValue();
        const float md = findParam (*inst2, "Processing Mode")->getValue();
        const float gt = findParam (*inst2, "Noise Gate")->getValue();
        std::printf ("[state] reload into new instance: voice %.2f  mode %.0f  gate %.0f  profile %s\n",
                     vp, md, gt, stateHasProfile (*inst2) ? "present" : "MISSING");
        CHECK (std::abs (vp - 0.73f) < 0.01f && md > 0.5f && gt > 0.5f, "parameters not restored");
        CHECK (stateHasProfile (*inst2), "profile not restored");

        // the reloaded instance produces the same audio
        setParam (*inst, "Processing Mode", 0.0f);  setParam (*inst2, "Processing Mode", 0.0f);
        setParam (*inst, "Noise Gate", 0.0f);       setParam (*inst2, "Noise Gate", 0.0f);
        inst->reset(); inst2->reset();
        Buf h3 = hiss ((size_t) sr * 4, 0.003f, 10, sr);
        Buf v = vowel (h3.size(), sr, 140.0, 0.3f);
        for (size_t i = 0; i < h3.size(); ++i) h3[i] += v[i];
        auto a = processHosted (*inst, { h3, h3 }, 512);
        auto b = processHosted (*inst2, { h3, h3 }, 512);
        double d = 0; for (size_t i = 0; i < h3.size(); ++i) d = std::max (d, (double) std::abs (a[0][i] - b[0][i]));
        std::printf ("[state] original vs reloaded instance output difference %.2e\n", d);
        CHECK (d < 1e-3, "reloaded instance differs (%g)", d);
    }

    // ------------------------------------------------------------------ editor open / close
    if (gui)
    {
        juce::String err;
        auto inst = format.createInstanceFromDescription (desc, 48000.0, 512, err);
        inst->prepareToPlay (48000.0, 512);
        int opened = 0;
        for (int i = 0; i < 12; ++i)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
            if (ed != nullptr)
            {
                ++opened;
                juce::DocumentWindow w ("DEADNOISE", juce::Colours::black, 0);
                w.setContentNonOwned (ed.get(), true);
                w.setVisible (true);
                Buf h = hiss (48000, 0.003f, (unsigned) i, 48000.0);
                processHosted (*inst, { h, h }, 512);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
                if (i == 0) std::printf ("[editor] size %d x %d\n", ed->getWidth(), ed->getHeight());
                w.clearContentComponent();
            }
            ed.reset();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        }
        std::printf ("[editor] opened and closed %d times while processing audio\n", opened);
        CHECK (opened == 12, "editor creation failed");
    }

    std::printf ("\n%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED");
    return failures == 0 ? 0 : 1;
}
