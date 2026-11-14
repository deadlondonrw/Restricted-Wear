// Loads the built DEADWAVE.vst3 through JUCE's VST3 hosting layer and exercises it like a DAW:
// instantiate, enumerate parameters, play polyphonic MIDI, save/restore state, open the editor.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

static int failures = 0;
#define EXPECT(c, msg) do { if (! (c)) { ++failures; std::printf ("  FAIL: %s\n", msg); } else std::printf ("  ok: %s\n", msg); } while (0)

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: DeadwaveHostTest <path-to-DEADWAVE.vst3>\n"); return 2; }
    const juce::File bundle (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));

    juce::AudioPluginFormatManager fm;
    fm.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> types;
    juce::KnownPluginList list;
    list.scanAndAddFile (bundle.getFullPathName(), true, types, *fm.getFormat (0));
    EXPECT (types.size() == 1, "VST3 module exposes exactly one plug-in class");
    if (types.isEmpty()) return 1;
    const auto& desc = *types[0];
    std::printf ("  found: %s by %s, %s, instrument=%d\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                 desc.version.toRawUTF8(), (int) desc.isInstrument);
    EXPECT (desc.isInstrument, "registered as an instrument");

    juce::String err;
    const double sr = 44100.0;
    const int block = 256;
    auto plugin = fm.createPluginInstance (desc, sr, block, err);
    EXPECT (plugin != nullptr, "instance created");
    if (plugin == nullptr) { std::printf ("  error: %s\n", err.toRawUTF8()); return 1; }

    const int numParams = plugin->getParameters().size();
    std::printf ("  parameters exposed to the host: %d\n", numParams);
    EXPECT (numParams > 700, "full parameter set visible to the host");

    plugin->setPlayConfigDetails (0, 2, sr, block);
    plugin->prepareToPlay (sr, block);

    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double energy = 0.0;
    bool finite = true;
    for (int b = 0; b < 400; ++b)
    {
        midi.clear();
        if (b == 2) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 10); midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.9f), 20); }
        if (b == 200) for (int n : { 48, 52, 55 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        buf.clear();
        plugin->processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) finite = false;
                energy += (double) v * v;
            }
    }
    std::printf ("  rendered energy: %.2f\n", energy);
    EXPECT (finite, "output finite");
    EXPECT (energy > 1.0, "plays a chord through the VST3 wrapper");

    // automate a few parameters through the host interface
    for (auto* p : plugin->getParameters())
        if (p->getName (64).contains ("Cutoff") || p->getName (64).contains ("Wave Position"))
            p->setValueNotifyingHost (0.3f);
    buf.clear();
    plugin->processBlock (buf, midi);

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    std::printf ("  state size: %d bytes\n", (int) state.getSize());
    EXPECT (state.getSize() > 100, "state saved");
    auto second = fm.createPluginInstance (desc, sr, block, err);
    second->setStateInformation (state.getData(), (int) state.getSize());
    juce::MemoryBlock state2;
    second->getStateInformation (state2);
    EXPECT (state == state2, "state restored identically in a second instance");

    if (auto* ed = plugin->createEditorIfNeeded())
    {
        std::printf ("  editor: %d x %d\n", ed->getWidth(), ed->getHeight());
        EXPECT (ed->getWidth() >= 768 && ed->getHeight() >= 490, "editor opens at a sensible size");
        ed->setSize (1600, 1025);
        plugin->editorBeingDeleted (ed);
        delete ed;
        EXPECT (true, "editor closed cleanly");
    }
    else
        EXPECT (false, "editor created");

    plugin->releaseResources();
    plugin.reset();
    second.reset();
    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
