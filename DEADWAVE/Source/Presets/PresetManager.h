#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <set>

namespace dw
{
class DeadwaveProcessor;

struct PresetInfo
{
    juce::String name, category, author, description;
    bool factory = false;
    int factoryIndex = -1;
    juce::File file;
    juce::String key() const { return factory ? "factory/" + name : file.getFullPathName(); }
};

/** Factory presets are generated in code (deterministic, versioned with the plug-in). */
struct FactoryPreset
{
    juce::String name, category, description;
    juce::ValueTree state; // complete DEADWAVE state tree
};
const std::vector<FactoryPreset>& factoryPresets();
const juce::StringArray& presetCategories();

class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (DeadwaveProcessor& p);

    static juce::File rootDirectory();
    static juce::File userPresetDirectory();
    static juce::File userWavetableDirectory();
    static juce::String fileExtension() { return ".dwpreset"; }

    void rescan();
    const std::vector<PresetInfo>& getAll() const noexcept { return all; }

    enum class Filter { All, Factory, User, Favorites, Recent };
    std::vector<int> query (const juce::String& search, const juce::String& category, Filter f) const;

    bool load (int index);
    bool loadNext (int delta, const std::vector<int>& within);
    int getCurrentIndex() const noexcept { return current; }

    /** Saves the current sound as a user preset. Returns the new index or -1. */
    int saveUser (const juce::String& name, const juce::String& category, const juce::String& author, bool overwrite, juce::String& error);
    bool rename (int index, const juce::String& newName, juce::String& error);
    bool remove (int index, juce::String& error);
    bool importFile (const juce::File& f, juce::String& error);

    bool isFavorite (int index) const;
    void setFavorite (int index, bool fav);
    const juce::StringArray& getRecent() const noexcept { return recent; }

private:
    void loadPersisted();
    void savePersisted() const;
    void pushRecent (const juce::String& key);
    static juce::String sanitise (const juce::String& name);

    DeadwaveProcessor& processor;
    std::vector<PresetInfo> all;
    std::set<juce::String> favorites;
    juce::StringArray recent;
    int current = -1;
};
} // namespace dw
