#pragma once

#include "../Wave/SampleImport.h"
#include "OscPanel.h"

namespace dw::ui
{
class WaveCanvas : public juce::Component
{
public:
    enum class Mode { Draw, Points, Harmonics };
    enum class View { Wave2D, Wave3D, Spectrum };

    WaveCanvas (DeadwaveProcessor& p) : proc (p) {}
    void setOsc (int o) { osc = o; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    Mode mode = Mode::Draw;
    View view = View::Wave2D;
    bool allFrames = false;
    int grid = 0;          // x snap divisions for drawing (0 = off)
    int harmonicCount = 64;
    std::function<void()> onEdited;

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (14.0f, 12.0f); }
    juce::Point<float> toWave (juce::Point<float> p) const;
    juce::Point<float> toScreen (float x, float y) const;
    void paint2D (juce::Graphics&);
    void paint3D (juce::Graphics&);
    void paintSpectrum (juce::Graphics&, bool editable);
    int hitPoint (juce::Point<float>) const;
    void editHarmonic (juce::Point<float> p, bool phase);

    DeadwaveProcessor& proc;
    int osc = 0;
    juce::Point<float> last;
    int dragPoint = -1, curveSeg = -1;
    float curveStart = 0.0f, dragStartY = 0.0f;
    juce::Point<float> hoverWave { -1.0f, 0.0f };
    std::vector<float> mags, phases;
};

class FrameStrip : public juce::Component
{
public:
    FrameStrip (DeadwaveProcessor& p) : proc (p) {}
    void setOsc (int o) { osc = o; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int markA = -1;
    std::function<void()> onSelect;
private:
    DeadwaveProcessor& proc;
    int osc = 0;
    float scroll = 0.0f;
    int cellW() const { return 56; }
};

/** Audio import: region selection, cycle extraction / slicing / sample oscillator, preview. */
class ImportPanel : public Panel
{
public:
    ImportPanel (DeadwaveProcessor& p);
    void setAudio (int osc, sampleimport::Audio a);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    std::function<void()> onClose;
private:
    juce::Rectangle<int> overviewArea() const;
    void analyse();
    void create();
    DeadwaveProcessor& proc;
    int osc = 0;
    sampleimport::Audio audio;
    int selStart = 0, selEnd = 0, dragAnchor = 0;
    float period = 0.0f;
    Segmented mode { { "CYCLES", "SLICES", "SAMPLE OSC" } };
    juce::ComboBox frames;
    juce::ToggleButton loop { "LOOP" };
    FlatButton preview { "PREVIEW" }, stop { "STOP" }, createBtn { "CREATE" }, cancel { "CANCEL" };
    juce::String info;
};

class WaveEditor : public juce::Component, private juce::Timer
{
public:
    WaveEditor (DeadwaveProcessor& p);
    ~WaveEditor() override;
    void setOsc (int o);
    int getOsc() const noexcept { return osc; }
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void refresh();
    std::function<void()> onClose;

private:
    void timerCallback() override;
    void rebuildShapeKnobs();
    void applyTransform (WaveDocument::Transform t);
    void generateMath();
    void generateRandom();
    void importAudio();
    void saveTable();
    void loadTable();
    void updateInfo();
    void layoutRight (juce::Rectangle<int>);
    void layoutLeft (juce::Rectangle<int>);

    DeadwaveProcessor& proc;
    int osc = 0;
    uint64_t lastVersion = 0;

    // header
    Segmented oscSel { { "OSC 1", "OSC 2", "OSC 3", "OSC 4" } };
    FlatButton undoBtn { "UNDO" }, redoBtn { "REDO" }, closeBtn { "CLOSE" };
    juce::Label tableName;

    // tools (left)
    Panel toolsPanel { "EDIT" };
    Segmented modeSel { { "DRAW", "POINTS", "HARM" } };
    Segmented gridSel { { "FREE", "8", "16", "32" } };
    juce::ToggleButton allFrames { "ALL FRAMES" };
    Caption gridCap { "SNAP", Colours::muted, 9.5f }, amtCap { "AMOUNT", Colours::muted, 9.5f }, transCap { "TRANSFORM", Colours::secondary, 10.5f };
    juce::Slider amount;
    std::vector<std::unique_ptr<FlatButton>> transformButtons;
    std::vector<WaveDocument::Transform> transformIds;

    // centre
    Panel canvasPanel;
    Segmented viewSel { { "2D", "3D", "SPECTRUM" } };
    WaveCanvas canvas;
    juce::Label hint;

    // frames
    Panel framePanel { "FRAMES" };
    FrameStrip strip;
    FlatButton addBtn { "ADD" }, dupBtn { "DUPLICATE" }, delBtn { "DELETE" }, leftBtn { "<" }, rightBtn { ">" },
               markBtn { "MARK A" }, interpBtn { "INTERPOLATE A > B" }, resampleBtn { "FRAMES..." };
    Segmented morphSel { { "FADE", "SPECTRAL" } };

    // right
    Panel rightPanel;
    Segmented rightTabs { { "SHAPE", "GENERATE" } };
    std::vector<std::unique_ptr<ParamKnob>> shapeKnobs;
    std::unique_ptr<ParamChoice> warpChoice;
    std::unique_ptr<ParamToggle> dcToggle;
    juce::Label shapeInfo;
    // generate tab
    juce::ComboBox factoryBox;
    FlatButton loadFactoryBtn { "LOAD" };
    juce::TextEditor expr;
    juce::ComboBox exprFrames, exprExamples;
    FlatButton mathBtn { "GENERATE MATH" };
    juce::Label mathError;
    juce::ComboBox randStyle, randFrames;
    juce::Slider randComplexity;
    juce::ToggleButton randCurrent { "CURRENT FRAME" };
    FlatButton randBtn { "RANDOM WAVE" };
    FlatButton importBtn { "IMPORT AUDIO" }, saveBtn { "SAVE TABLE" }, loadBtn { "LOAD TABLE" };
    Caption facCap { "FACTORY TABLES", Colours::secondary, 10.5f }, mathCap { "MATHEMATICAL", Colours::secondary, 10.5f },
            randCap { "RANDOM", Colours::secondary, 10.5f }, fileCap { "FILES", Colours::secondary, 10.5f };

    ImportPanel importPanel;
    std::unique_ptr<juce::FileChooser> chooser;
    uint32_t randSeed = 1;
    float lastWt = -1.0f;
    float overlayKey[5] {};
};
} // namespace dw::ui
