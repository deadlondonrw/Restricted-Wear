#include "PresetBuilder.h"
#include "PresetManager.h"

namespace dw
{
const juce::StringArray& presetCategories()
{
    static const juce::StringArray c { "Bass", "Sub Bass", "Reese", "Growl", "Lead", "Pad", "Pluck", "Keys", "Bell",
                                       "Atmospheric", "Cinematic", "Texture", "Drone", "Experimental", "Digital",
                                       "Analog", "Dark", "Ethereal", "Aggressive", "FX", "Init" };
    return c;
}

namespace
{
void addInit (std::vector<FactoryPreset>& out)
{
    PresetBuilder b ("Init", "Init");
    b.description ("Single sine-to-pulse wavetable oscillator, filter off.");
    out.push_back ({ "Init", "Init", "Default starting point", b.build() });
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = []
    {
        std::vector<FactoryPreset> out;
        addInit (out);
        return out;
    }();
    return list;
}
} // namespace dw
