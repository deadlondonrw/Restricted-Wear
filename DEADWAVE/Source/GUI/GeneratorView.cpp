#include "GeneratorView.h"
#include "../Presets/PresetManager.h"

namespace dw::ui
{
GeneratorView::GeneratorView (DeadwaveProcessor& p) : proc (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &textPanel, &paramPanel, &randPanel, &undo })
        addAndMakeVisible (c);

    prompt.setMultiLine (true);
    prompt.setReturnKeyStartsNewLine (false);
    prompt.setFont (font (14.0f));
    prompt.setTextToShowWhenEmpty ("e.g.  dark evolving reese bass with slow movement and grit", Colours::muted);
    prompt.onReturnKey = [this] { fromText(); };
    textGo.onClick = [this] { fromText(); };
    understood.setFont (font (12.0f));
    understood.setColour (juce::Label::textColourId, Colours::value);
    understood.setJustificationType (juce::Justification::topLeft);
    textNote.setFont (font (11.0f));
    textNote.setColour (juce::Label::textColourId, Colours::dim);
    textNote.setJustificationType (juce::Justification::topLeft);
    textNote.setText ("Runs entirely on your computer. Words are matched against a sound-design vocabulary "
                      "(categories, tone, movement, space, character) and a rule-based designer builds a complete preset. "
                      "This is not an AI language model, and it needs no internet connection or subscription.",
                      juce::dontSendNotification);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &prompt, &textGo, &understood, &textNote })
        textPanel.addAndMakeVisible (c);

    auto cats = presetCategories();
    cats.removeString ("Init");
    category.addItemList (cats, 1);
    category.setSelectedId (5, juce::dontSendNotification);
    paramPanel.addAndMakeVisible (category);
    const char* names[] = { "BRIGHTNESS", "MOVEMENT", "AGGRESSION", "WIDTH", "COMPLEXITY", "ATTACK", "LENGTH", "WARMTH", "SPACE" };
    const float defs[] = { 0.5f, 0.4f, 0.3f, 0.5f, 0.5f, 0.1f, 0.5f, 0.4f, 0.3f };
    for (int i = 0; i < 9; ++i)
    {
        auto t = std::make_unique<TraitSlider>();
        t->name = names[i];
        t->slider.setSliderStyle (juce::Slider::LinearBar);
        t->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        t->slider.setRange (0.0, 1.0);
        t->slider.setValue (defs[i], juce::dontSendNotification);
        t->caption = std::make_unique<Caption> (names[i], Colours::secondary, 10.5f);
        paramPanel.addAndMakeVisible (t->slider);
        paramPanel.addAndMakeVisible (*t->caption);
        traits.push_back (std::move (t));
    }
    for (auto* b : { &glide, &acid, &chip, &vocal, &metallic }) paramPanel.addAndMakeVisible (b);
    paramPanel.addAndMakeVisible (paramGo);
    paramGo.onClick = [this] { fromParams(); };

    intensity.setSliderStyle (juce::Slider::LinearBar);
    intensity.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    intensity.setRange (0.0, 1.0);
    intensity.setValue (0.35);
    randPanel.addAndMakeVisible (intensity);
    randPanel.addAndMakeVisible (intensityCap);
    randPanel.addAndMakeVisible (lockCap);
    for (const auto& n : SoundGenerator::lockableGroups())
    {
        locks.push_back (std::make_unique<juce::ToggleButton> (n));
        if (n == "ARP/SEQ") locks.back()->setToggleState (true, juce::dontSendNotification);
        randPanel.addAndMakeVisible (*locks.back());
    }
    randPanel.addAndMakeVisible (randPartial);
    randPanel.addAndMakeVisible (randFull);
    randPartial.onClick = [this] { randomize (false); };
    randFull.onClick = [this] { randomize (true); };
    randPartial.setTooltip ("Moves unlocked parameters around their current values by INTENSITY");
    randFull.setTooltip ("Builds a new sound for every unlocked group (musical limits keep it playable)");

    undo.onClick = [this]
    {
        if (previous.isValid())
        {
            auto tmp = proc.captureState();
            proc.applyState (previous);
            previous = tmp;
        }
    };
    undo.setTooltip ("Swap back to the sound before the last generation (click again to redo)");
}

void GeneratorView::apply (const juce::ValueTree& state, const juce::String& name)
{
    previous = proc.captureState();
    auto s = state.createCopy();
    s.setProperty ("preset", name, nullptr);
    proc.applyState (s);
    proc.setCurrentPresetName (name);
}

void GeneratorView::fromText()
{
    juce::StringArray tags;
    const auto text = prompt.getText().trim();
    if (text.isEmpty()) return;
    auto t = SoundGenerator::parseText (text, tags);
    seed = seed * 1664525u + 1013904223u + (uint32_t) text.hashCode();
    understood.setText (tags.isEmpty() ? juce::String ("No known words - generated a neutral " + t.category + ". Try words like bass, pad, dark, wide, evolving, gritty.")
                                       : "Understood:  " + tags.joinIntoString ("    "),
                        juce::dontSendNotification);
    apply (SoundGenerator::generate (t, seed, "Gen " + t.category), "Gen: " + text.substring (0, 28));
}

GenTraits GeneratorView::traitsFromSliders() const
{
    GenTraits t;
    t.category = category.getText();
    t.brightness = (float) traits[0]->slider.getValue();
    t.movement = (float) traits[1]->slider.getValue();
    t.aggression = (float) traits[2]->slider.getValue();
    t.width = (float) traits[3]->slider.getValue();
    t.complexity = (float) traits[4]->slider.getValue();
    t.attack = (float) traits[5]->slider.getValue();
    t.length = (float) traits[6]->slider.getValue();
    t.warmth = (float) traits[7]->slider.getValue();
    t.space = (float) traits[8]->slider.getValue();
    t.glide = glide.getToggleState();
    t.acid = acid.getToggleState();
    t.chip = chip.getToggleState();
    t.vocal = vocal.getToggleState();
    t.metallic = metallic.getToggleState();
    return t;
}

void GeneratorView::fromParams()
{
    seed = seed * 1664525u + 1013904223u + juce::Time::getMillisecondCounter();
    const auto t = traitsFromSliders();
    apply (SoundGenerator::generate (t, seed, "Gen " + t.category), "Gen " + t.category + " " + juce::String (seed % 1000));
}

void GeneratorView::randomize (bool full)
{
    std::set<Group> locked;
    for (auto& l : locks)
        if (l->getToggleState())
            for (auto g : SoundGenerator::groupsFor (l->getButtonText())) locked.insert (g);
    seed = seed * 1664525u + 1013904223u + juce::Time::getMillisecondCounter();
    const auto current = proc.captureState();
    auto next = SoundGenerator::randomize (current, (float) intensity.getValue(), locked, seed, full);
    apply (next, full ? "Random " + juce::String (seed % 10000) : proc.getCurrentPresetName().upToFirstOccurrenceOf (" *", false, false) + " *");
}

void GeneratorView::paint (juce::Graphics&) {}

void GeneratorView::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (32);
    undo.setBounds (bottom.removeFromRight (220).reduced (0, 3));
    r.removeFromBottom (6);
    const int w = (r.getWidth() - 2 * Metrics::gap) / 3;
    textPanel.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (Metrics::gap);
    paramPanel.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (Metrics::gap);
    randPanel.setBounds (r);

    auto t = textPanel.getLocalBounds().reduced (14, 10);
    t.removeFromTop (24);
    prompt.setBounds (t.removeFromTop (110));
    t.removeFromTop (8);
    textGo.setBounds (t.removeFromTop (30));
    t.removeFromTop (10);
    understood.setBounds (t.removeFromTop (150));
    textNote.setBounds (t.removeFromBottom (90));

    auto p = paramPanel.getLocalBounds().reduced (14, 10);
    p.removeFromTop (24);
    category.setBounds (p.removeFromTop (26));
    p.removeFromTop (8);
    for (auto& tr : traits)
    {
        auto row = p.removeFromTop (30);
        tr->caption->setBounds (row.removeFromLeft (110));
        tr->slider.setBounds (row.reduced (0, 6));
    }
    p.removeFromTop (8);
    auto flags = p.removeFromTop (24);
    const int fw = flags.getWidth() / 3;
    glide.setBounds (flags.removeFromLeft (fw)); acid.setBounds (flags.removeFromLeft (fw)); chip.setBounds (flags);
    auto flags2 = p.removeFromTop (26).withTrimmedTop (4);
    vocal.setBounds (flags2.removeFromLeft (fw)); metallic.setBounds (flags2.removeFromLeft (fw + 20));
    p.removeFromTop (12);
    paramGo.setBounds (p.removeFromTop (32));

    auto q = randPanel.getLocalBounds().reduced (14, 10);
    q.removeFromTop (24);
    intensityCap.setBounds (q.removeFromTop (14));
    intensity.setBounds (q.removeFromTop (18));
    q.removeFromTop (12);
    lockCap.setBounds (q.removeFromTop (14));
    q.removeFromTop (4);
    const int cols = 3, rowH = 26;
    for (int i = 0; i < (int) locks.size(); ++i)
        locks[(size_t) i]->setBounds (q.getX() + (i % cols) * (q.getWidth() / cols), q.getY() + (i / cols) * rowH, q.getWidth() / cols, 22);
    q.removeFromTop (((int) locks.size() + cols - 1) / cols * rowH + 16);
    randPartial.setBounds (q.removeFromTop (32));
    q.removeFromTop (8);
    randFull.setBounds (q.removeFromTop (32));
}
} // namespace dw::ui
