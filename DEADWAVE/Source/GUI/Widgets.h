#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace dw::ui
{
/** Card container with DEADNOISE panel styling and an optional tracked title. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String titleText = {}) : title (std::move (titleText)) {}
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void paint (juce::Graphics& g) override;
    /** Content area below the title. */
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (12).withTrimmedTop (title.isEmpty() ? 0 : 18); }
    juce::String title;
    bool highlighted = false;
};

/** Parameter knob (custom drawn) with modulation display, drag-and-drop modulation and context menu. */
class ParamKnob : public juce::Component,
                  public juce::DragAndDropTarget,
                  public juce::SettableTooltipClient
{
public:
    ParamKnob (DeadwaveProcessor& p, int paramIndex, juce::String labelOverride = {});
    ~ParamKnob() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    /** Pulls live modulation from the engine (called by the editor's UI timer). */
    void refreshModulation();
    int getParamIndex() const noexcept { return index; }
    void setLabelVisible (bool v) { showLabel = v; repaint(); }
    void setCompact (bool c) { compact = c; repaint(); }

private:
    void setNormalised (float v);
    void showMenu();
    void editValueAsText();

    DeadwaveProcessor& proc;
    int index;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float norm = 0.0f;
    float modLo = -1.0f, modHi = -1.0f, modCur = -1.0f;
    bool bipolar = false, hover = false, dragging = false, dropHover = false, showLabel = true, compact = false;
    bool modDrag = false;
    int modDragSlot = -1;
    float dragStartNorm = 0.0f, dragStartAmount = 0.0f;
    juce::Point<float> lastDragPos;
    std::unique_ptr<juce::TextEditor> textEntry;
};

/** Pill switch bound to a boolean parameter. */
class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (DeadwaveProcessor& p, int paramIndex, const juce::String& text = {});
private:
    juce::ButtonParameterAttachment attachment;
};

/** DEADNOISE segmented control bound to a choice parameter (or used free-standing). */
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::StringArray items);
    Segmented (DeadwaveProcessor& p, int paramIndex, juce::StringArray labelsOverride = {});
    ~Segmented() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (int i, bool notify);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange;
    float fontHeight = 11.0f;

private:
    juce::StringArray items;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Combo box bound to a choice parameter. */
class ParamChoice : public juce::ComboBox
{
public:
    ParamChoice (DeadwaveProcessor& p, int paramIndex);
private:
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

/** Draggable modulation source chip (drop it on any knob to create a routing). */
class ModSourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceChip (DeadwaveProcessor& p, ModSrc s);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void refresh();
    ModSrc getSource() const noexcept { return src; }

private:
    DeadwaveProcessor& proc;
    ModSrc src;
    float value = 0.0f;
    bool hover = false;
};

/** Small flat text button (header actions, tool buttons). */
class FlatButton : public juce::TextButton
{
public:
    explicit FlatButton (const juce::String& text, bool flat = false, float fontHeight = 11.5f)
        : juce::TextButton (text)
    {
        getProperties().set ("flat", flat);
        getProperties().set ("fontHeight", fontHeight);
    }
};

/** Static tracked label. */
class Caption : public juce::Component
{
public:
    Caption (juce::String t, juce::Colour c = Colours::secondary, float h = 11.0f, juce::Justification j = juce::Justification::centredLeft)
        : text (std::move (t)), colour (c), height (h), just (j) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override { drawTracked (g, text, getLocalBounds().toFloat(), font (height, true), colour, just, 0.12f); }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    juce::String text;
    juce::Colour colour;
    float height;
    juce::Justification just;
};

/** Lays out knobs in a row with equal spacing inside an area. */
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int itemWidth = -1);
} // namespace dw::ui
