#include "PresetManager.h"
#include "../PluginProcessor.h"

namespace dw
{
PresetManager::PresetManager (DeadwaveProcessor& p) : processor (p)
{
    loadPersisted();
    rescan();
}

juce::File PresetManager::rootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DEADWAVE");
}

juce::File PresetManager::userPresetDirectory()
{
    auto d = rootDirectory().getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::File PresetManager::userWavetableDirectory()
{
    auto d = rootDirectory().getChildFile ("Wavetables");
    d.createDirectory();
    return d;
}

juce::String PresetManager::sanitise (const juce::String& name)
{
    auto s = name.trim().retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_()#&+.");
    return s.substring (0, 64).trim();
}

void PresetManager::rescan()
{
    juce::String currentKey = current >= 0 && current < (int) all.size() ? all[(size_t) current].key() : juce::String();
    all.clear();
    const auto& fac = factoryPresets();
    for (int i = 0; i < (int) fac.size(); ++i)
    {
        PresetInfo info;
        info.name = fac[(size_t) i].name;
        info.category = fac[(size_t) i].category;
        info.description = fac[(size_t) i].description;
        info.author = "DEADWAVE";
        info.factory = true;
        info.factoryIndex = i;
        all.push_back (info);
    }

    const auto dir = rootDirectory().getChildFile ("Presets");
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, true, "*" + fileExtension());
        files.sort();
        for (const auto& f : files)
        {
            PresetInfo info;
            info.name = f.getFileNameWithoutExtension();
            info.file = f;
            info.category = "User";
            if (auto xml = juce::XmlDocument::parse (f))
            {
                info.category = xml->getStringAttribute ("category", "User");
                info.author = xml->getStringAttribute ("author");
                info.description = xml->getStringAttribute ("description");
            }
            all.push_back (info);
        }
    }

    current = -1;
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].key() == currentKey) current = i;
    sendChangeMessage();
}

std::vector<int> PresetManager::query (const juce::String& search, const juce::String& category, Filter f) const
{
    std::vector<int> out;
    const auto terms = juce::StringArray::fromTokens (search.toLowerCase(), " ", "");

    auto matches = [&] (const PresetInfo& p)
    {
        if (category.isNotEmpty() && category != "All" && p.category != category) return false;
        const auto hay = (p.name + " " + p.category + " " + p.description + " " + p.author).toLowerCase();
        for (const auto& t : terms)
            if (t.isNotEmpty() && ! hay.contains (t)) return false;
        return true;
    };

    if (f == Filter::Recent)
    {
        for (const auto& key : recent)
            for (int i = 0; i < (int) all.size(); ++i)
                if (all[(size_t) i].key() == key && matches (all[(size_t) i])) out.push_back (i);
        return out;
    }

    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (f == Filter::Factory && ! p.factory) continue;
        if (f == Filter::User && p.factory) continue;
        if (f == Filter::Favorites && favorites.count (p.key()) == 0) continue;
        if (matches (p)) out.push_back (i);
    }
    return out;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) all.size()) return false;
    const auto& info = all[(size_t) index];
    juce::ValueTree state;
    if (info.factory)
        state = factoryPresets()[(size_t) info.factoryIndex].state;
    else if (auto xml = juce::XmlDocument::parse (info.file))
        state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid() || ! state.hasType ("DEADWAVE")) return false;
    processor.applyState (state);
    processor.setCurrentPresetName (info.name);
    current = index;
    pushRecent (info.key());
    sendChangeMessage();
    return true;
}

bool PresetManager::loadNext (int delta, const std::vector<int>& within)
{
    if (within.empty()) return false;
    int pos = -1;
    for (int i = 0; i < (int) within.size(); ++i)
        if (within[(size_t) i] == current) pos = i;
    pos = pos < 0 ? (delta > 0 ? 0 : (int) within.size() - 1)
                  : (pos + delta + (int) within.size()) % (int) within.size();
    return load (within[(size_t) pos]);
}

int PresetManager::saveUser (const juce::String& rawName, const juce::String& category, const juce::String& author,
                             bool overwrite, juce::String& error)
{
    const auto name = sanitise (rawName);
    if (name.isEmpty()) { error = "Please enter a preset name."; return -1; }
    const auto file = userPresetDirectory().getChildFile (name + fileExtension());
    if (file.existsAsFile() && ! overwrite) { error = "A user preset with this name already exists."; return -1; }

    auto state = processor.captureState();
    state.setProperty ("preset", name, nullptr);
    state.setProperty ("category", category.isEmpty() ? juce::String ("User") : category, nullptr);
    state.setProperty ("author", author, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (file)) { error = "Could not write " + file.getFullPathName(); return -1; }

    processor.setCurrentPresetName (name);
    rescan();
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory && all[(size_t) i].file == file)
        {
            current = i;
            pushRecent (all[(size_t) i].key());
            return i;
        }
    return -1;
}

bool PresetManager::rename (int index, const juce::String& newName, juce::String& error)
{
    if (index < 0 || index >= (int) all.size() || all[(size_t) index].factory) { error = "Factory presets cannot be renamed."; return false; }
    const auto name = sanitise (newName);
    if (name.isEmpty()) { error = "Please enter a name."; return false; }
    const auto oldFile = all[(size_t) index].file;
    const auto target = oldFile.getParentDirectory().getChildFile (name + fileExtension());
    if (target.existsAsFile()) { error = "A preset with this name already exists."; return false; }
    if (! oldFile.moveFileTo (target)) { error = "Could not rename the file."; return false; }

    if (auto xml = juce::XmlDocument::parse (target))
    {
        xml->setAttribute ("preset", name);
        xml->writeTo (target);
    }
    const bool fav = favorites.erase (oldFile.getFullPathName()) > 0;
    if (fav) favorites.insert (target.getFullPathName());
    recent.removeString (oldFile.getFullPathName());
    savePersisted();
    if (current == index) processor.setCurrentPresetName (name);
    rescan();
    return true;
}

bool PresetManager::remove (int index, juce::String& error)
{
    if (index < 0 || index >= (int) all.size() || all[(size_t) index].factory) { error = "Factory presets cannot be deleted."; return false; }
    const auto f = all[(size_t) index].file;
    if (! f.moveToTrash() && ! f.deleteFile()) { error = "Could not delete the file."; return false; }
    favorites.erase (f.getFullPathName());
    recent.removeString (f.getFullPathName());
    savePersisted();
    if (current == index) current = -1;
    rescan();
    return true;
}

bool PresetManager::importFile (const juce::File& f, juce::String& error)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("DEADWAVE")) { error = "Not a DEADWAVE preset."; return false; }
    const auto target = userPresetDirectory().getNonexistentChildFile (sanitise (f.getFileNameWithoutExtension()), fileExtension(), false);
    if (! f.copyFileTo (target)) { error = "Could not copy the preset."; return false; }
    rescan();
    return true;
}

bool PresetManager::isFavorite (int index) const
{
    return index >= 0 && index < (int) all.size() && favorites.count (all[(size_t) index].key()) > 0;
}

void PresetManager::setFavorite (int index, bool fav)
{
    if (index < 0 || index >= (int) all.size()) return;
    if (fav) favorites.insert (all[(size_t) index].key());
    else favorites.erase (all[(size_t) index].key());
    savePersisted();
    sendChangeMessage();
}

void PresetManager::pushRecent (const juce::String& key)
{
    recent.removeString (key);
    recent.insert (0, key);
    while (recent.size() > 30) recent.remove (recent.size() - 1);
    savePersisted();
}

void PresetManager::loadPersisted()
{
    const auto f = rootDirectory().getChildFile ("browser.xml");
    if (auto xml = juce::XmlDocument::parse (f))
    {
        for (auto* e : xml->getChildWithTagNameIterator ("FAV"))
            favorites.insert (e->getStringAttribute ("key"));
        for (auto* e : xml->getChildWithTagNameIterator ("RECENT"))
            recent.add (e->getStringAttribute ("key"));
    }
}

void PresetManager::savePersisted() const
{
    juce::XmlElement xml ("BROWSER");
    for (const auto& k : favorites)
        xml.createNewChildElement ("FAV")->setAttribute ("key", k);
    for (const auto& k : recent)
        xml.createNewChildElement ("RECENT")->setAttribute ("key", k);
    rootDirectory().createDirectory();
    xml.writeTo (rootDirectory().getChildFile ("browser.xml"));
}
} // namespace dw
