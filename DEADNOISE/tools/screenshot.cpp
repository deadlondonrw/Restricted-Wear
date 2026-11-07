// Renders the DEADNOISE editor to a PNG after running audio through the
// processor (learn pass + speech-like signal), so the meters, spectrum and
// profile status show realistic content.
//   deadnoise_screenshot out.png
#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/PluginEditor.h"

#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outFile = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "deadnoise-ui.png");

    DeadnoiseAudioProcessor proc;
    const double sr = 48000.0;
    const int block = 480;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::DocumentWindow window ("DEADNOISE", juce::Colours::black, 0);
    window.setContentNonOwned (editor.get(), true);
    window.setVisible (true);

    std::mt19937 rng (7);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    float prev = 0.0f;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double t = 0.0;
    auto runSeconds = [&] (double seconds, bool speech)
    {
        const int blocks = (int) (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i, t += 1.0 / sr)
            {
                const float w = nd (rng);
                float x = 0.0006f * w + 0.004f * (w - prev);   // rising high-frequency hiss
                prev = w;
                if (speech)
                {
                    const double env = std::max (0.0, std::sin (2.0 * juce::MathConstants<double>::pi * 1.6 * t));
                    double v = 0.0;
                    for (int h = 1; h < 40; ++h)
                    {
                        const double f = 135.0 * h;
                        const double fe = 0.08 + std::exp (-std::pow ((f - 650.0) / 260.0, 2.0)) + 0.6 * std::exp (-std::pow ((f - 2300.0) / 400.0, 2.0));
                        v += fe * std::sin (2.0 * juce::MathConstants<double>::pi * f * t + 0.4 * h * h);
                    }
                    x += (float) (0.05 * env * v);
                }
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            proc.processBlock (buf, midi);
            if (b % 4 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
    };

    runSeconds (0.5, false);
    proc.startLearning();
    runSeconds (4.8, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    runSeconds (2.35, true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);

    const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    outFile.deleteFile();
    juce::FileOutputStream os (outFile);
    juce::PNGImageFormat png;
    const bool ok = os.openedOk() && png.writeImageToStream (img, os);
    std::printf ("%s %s (%d x %d)\n", ok ? "wrote" : "FAILED to write", outFile.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    window.clearContentComponent();
    editor.reset();
    return ok ? 0 : 1;
}
