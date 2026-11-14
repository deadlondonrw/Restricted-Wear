#include "BrowserView.h"

namespace dw::ui
{
BrowserView::BrowserView (DeadwaveProcessor& p) : Panel ("PRESETS"), proc (p), pm (p.getPresetManager())
{
    categories.add ("All");
    categories.addArray (presetCategories());
    categories.add ("User");
    categories.removeDuplicates (false);

    search.setTextToShowWhenEmpty ("Search presets, categories, descriptions", Colours::muted);
    search.setFont (font (13.0f));
    search.onTextChange = [this] { requery(); };
    filter.onChange = [this] (int) { requery(); };
    filter.fontHeight = 10.0f;
    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, Colours::raised);
    list.setOutlineThickness (0);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &search, &filter, &list, &catList, &loadBtn, &saveBtn, &renameBtn,
                                                                        &deleteBtn, &importBtn, &folderBtn, &closeBtn })
        addAndMakeVisible (c);

    loadBtn.onClick = [this] { if (selectedIndex() >= 0) pm.load (selectedIndex()); };
    saveBtn.onClick = [this] { saveAs(); };
    renameBtn.onClick = [this] { renameSelected(); };
    deleteBtn.onClick = [this] { deleteSelected(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    folderBtn.onClick = [] { PresetManager::userPresetDirectory().revealToUser(); };
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.dwpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            juce::String err;
            if (fc.getResult().existsAsFile() && ! pm.importFile (fc.getResult(), err))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", err);
        });
    };
    pm.addChangeListener (this);
    requery();
}

BrowserView::~BrowserView() { pm.removeChangeListener (this); }

void BrowserView::visibilityChanged()
{
    if (isVisible())
    {
        pm.rescan();
        search.grabKeyboardFocus();
    }
}

int BrowserView::selectedIndex() const
{
    const int row = list.getSelectedRow();
    return row >= 0 && row < (int) results.size() ? results[(size_t) row] : -1;
}

void BrowserView::requery()
{
    const auto f = (PresetManager::Filter) filter.getSelected();
    results = pm.query (search.getText(), category, f);
    list.updateContent();
    for (int i = 0; i < (int) results.size(); ++i)
        if (results[(size_t) i] == pm.getCurrentIndex()) list.selectRow (i, true, true);
    repaint();
}

void BrowserView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) results.size()) return;
    const auto& info = pm.getAll()[(size_t) results[(size_t) row]];
    if (selected)
    {
        g.setColour (Colours::text);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f), 3.0f);
    }
    const auto fg = selected ? Colours::bg : Colours::text;
    const bool fav = pm.isFavorite (results[(size_t) row]);
    g.setColour (fav ? Colours::accent : (selected ? Colours::bg.withAlpha (0.4f) : Colours::track));
    juce::Path star;
    star.addStar ({ 16.0f, (float) h * 0.5f }, 5, 3.0f, 7.0f);
    g.fillPath (star);
    g.setColour (fg);
    g.setFont (font (13.0f, results[(size_t) row] == pm.getCurrentIndex()));
    g.drawText (info.name, 32, 0, w - 200, h, juce::Justification::centredLeft, true);
    g.setColour (selected ? Colours::bg.withAlpha (0.7f) : Colours::secondary);
    g.setFont (font (11.0f, true));
    g.drawText (info.category.toUpperCase() + (info.factory ? "" : "  -  USER"), w - 170, 0, 160, h, juce::Justification::centredRight, true);
}

void BrowserView::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.x < 28 && row >= 0 && row < (int) results.size())
    {
        const int idx = results[(size_t) row];
        pm.setFavorite (idx, ! pm.isFavorite (idx));
        list.repaintRow (row);
    }
}

void BrowserView::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) results.size()) pm.load (results[(size_t) row]);
}

void BrowserView::saveAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Save the current sound as a user preset.", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getCurrentPresetName(), "Name");
    juce::StringArray cats = presetCategories();
    cats.removeString ("Init");
    cats.insert (0, "User");
    w->addComboBox ("category", cats, "Category");
    w->addTextEditor ("author", "", "Author");
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r != 1) return;
        const auto name = w->getTextEditorContents ("name");
        const auto cat = w->getComboBoxComponent ("category")->getText();
        const auto author = w->getTextEditorContents ("author");
        juce::String err;
        if (pm.saveUser (name, cat, author, false, err) < 0)
        {
            if (err.contains ("already exists"))
            {
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Overwrite?", "\"" + name + "\" exists. Overwrite it?",
                                                    "OVERWRITE", "CANCEL", nullptr,
                                                    juce::ModalCallbackFunction::create ([this, name, cat, author] (int ok)
                                                    {
                                                        juce::String e2;
                                                        if (ok) pm.saveUser (name, cat, author, true, e2);
                                                    }));
            }
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", err);
        }
    }), true);
}

void BrowserView::renameSelected()
{
    const int idx = selectedIndex();
    if (idx < 0 || pm.getAll()[(size_t) idx].factory)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Rename", "Select a user preset to rename.");
        return;
    }
    auto* w = new juce::AlertWindow ("Rename preset", "", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", pm.getAll()[(size_t) idx].name, "New name");
    w->addButton ("RENAME", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, idx] (int r)
    {
        if (r != 1) return;
        juce::String err;
        if (! pm.rename (idx, w->getTextEditorContents ("name"), err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Rename failed", err);
    }), true);
}

void BrowserView::deleteSelected()
{
    const int idx = selectedIndex();
    if (idx < 0 || pm.getAll()[(size_t) idx].factory)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Delete", "Select a user preset to delete.");
        return;
    }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete preset",
                                        "Move \"" + pm.getAll()[(size_t) idx].name + "\" to the trash?", "DELETE", "CANCEL", nullptr,
                                        juce::ModalCallbackFunction::create ([this, idx] (int ok)
                                        {
                                            juce::String err;
                                            if (ok && ! pm.remove (idx, err))
                                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Delete failed", err);
                                        }));
}

void BrowserView::CategoryList::paint (juce::Graphics& g)
{
    const int rowH = 24;
    for (int i = 0; i < owner.categories.size(); ++i)
    {
        auto r = juce::Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH - 2.0f);
        const bool sel = owner.categories[i] == owner.category;
        if (sel)
        {
            g.setColour (Colours::raised);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colours::text);
            g.fillRect (r.removeFromLeft (2.0f));
        }
        drawTracked (g, owner.categories[i].toUpperCase(), r.withTrimmedLeft (10.0f), font (10.5f, true),
                     sel ? Colours::text : Colours::secondary, juce::Justification::centredLeft, 0.1f);
    }
}

void BrowserView::CategoryList::mouseDown (const juce::MouseEvent& e)
{
    const int i = e.y / 24;
    if (i >= 0 && i < owner.categories.size())
    {
        owner.category = owner.categories[i];
        owner.requery();
        repaint();
    }
}

void BrowserView::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const int idx = selectedIndex();
    auto info = getLocalBounds().reduced (14).removeFromRight (250).withTrimmedTop (64).withTrimmedBottom (40);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (info.toFloat(), Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (info.toFloat(), Metrics::controlRadius, 1.0f);
    if (idx >= 0)
    {
        const auto& p = pm.getAll()[(size_t) idx];
        auto r = info.reduced (12);
        g.setColour (Colours::text);
        g.setFont (font (16.0f, true));
        g.drawText (p.name, r.removeFromTop (24), juce::Justification::centredLeft, true);
        drawTracked (g, p.category + (p.factory ? "  -  FACTORY" : "  -  USER"), r.removeFromTop (18).toFloat(), font (10.5f, true),
                     Colours::secondary, juce::Justification::centredLeft);
        if (p.author.isNotEmpty())
        {
            g.setColour (Colours::dim);
            g.setFont (font (11.5f));
            g.drawText ("by " + p.author, r.removeFromTop (18), juce::Justification::centredLeft, true);
        }
        r.removeFromTop (8);
        g.setColour (Colours::value);
        g.setFont (font (12.5f));
        g.drawFittedText (p.description, r, juce::Justification::topLeft, 12);
    }
    g.setColour (Colours::muted);
    g.setFont (font (11.0f));
    g.drawText (juce::String (results.size()) + " PRESETS", getLocalBounds().reduced (14).removeFromBottom (30).removeFromLeft (200),
                juce::Justification::centredLeft, false);
}

void BrowserView::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (22);
    auto top = r.removeFromTop (30);
    closeBtn.setBounds (top.removeFromRight (80).reduced (0, 2));
    top.removeFromRight (8);
    filter.setBounds (top.removeFromRight (440).reduced (0, 2));
    top.removeFromRight (8);
    search.setBounds (top.reduced (0, 1));
    r.removeFromTop (12);
    auto bottom = r.removeFromBottom (30);
    for (auto* b : { &folderBtn, &importBtn, &deleteBtn, &renameBtn, &saveBtn, &loadBtn })
    {
        b->setBounds (bottom.removeFromRight (b == &folderBtn ? 110 : 84).reduced (0, 2));
        bottom.removeFromRight (6);
    }
    r.removeFromBottom (10);
    catList.setBounds (r.removeFromLeft (150));
    r.removeFromLeft (12);
    r.removeFromRight (262);
    list.setBounds (r);
}
} // namespace dw::ui
