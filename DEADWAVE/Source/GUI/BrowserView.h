#pragma once

#include "../Presets/PresetManager.h"
#include "Widgets.h"

namespace dw::ui
{
class BrowserView : public Panel, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    explicit BrowserView (DeadwaveProcessor& p);
    ~BrowserView() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    std::function<void()> onClose;
    void saveAs();

private:
    int getNumRows() override { return (int) results.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override { repaint(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { requery(); }
    void requery();
    void renameSelected();
    void deleteSelected();
    int selectedIndex() const;

    DeadwaveProcessor& proc;
    PresetManager& pm;
    juce::TextEditor search;
    Segmented filter { { "ALL", "FACTORY", "USER", "FAVORITES", "RECENT" } };
    juce::ListBox list;
    juce::StringArray categories;
    juce::String category = "All";
    std::vector<int> results;
    FlatButton loadBtn { "LOAD" }, saveBtn { "SAVE AS" }, renameBtn { "RENAME" }, deleteBtn { "DELETE" }, importBtn { "IMPORT" },
               folderBtn { "OPEN FOLDER" }, closeBtn { "CLOSE" };
    struct CategoryList : public juce::Component
    {
        BrowserView& owner;
        explicit CategoryList (BrowserView& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    };
    CategoryList catList { *this };
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace dw::ui
