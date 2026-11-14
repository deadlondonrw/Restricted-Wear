#pragma once

#include "../DSP/WaveGen.h"
#include "../DSP/Wavetable.h"
#include <juce_data_structures/juce_data_structures.h>

namespace dw
{
/**
    Editable multi-frame waveform owned by one oscillator (message thread only).

    Each frame stores the authoritative 2048-sample cycle. A frame may additionally carry control
    points; while a frame is in point mode its samples are regenerated from those points. All
    operations are destructive edits with snapshot undo/redo. Every change bumps the version so
    the processor can rebuild the band-limited table for the audio thread.
*/
class WaveDocument
{
public:
    struct ControlPoint
    {
        float x = 0.0f;     // 0..1
        float y = 0.0f;     // -1..1 (may exceed for headroom, clamped to +/-2)
        float curve = 0.0f; // -1..1 curvature of the segment to the next point
    };

    struct Frame
    {
        std::vector<float> samples = std::vector<float> ((size_t) kWaveSize, 0.0f);
        std::vector<ControlPoint> points;
        bool pointMode = false;
    };

    enum class Transform
    {
        Invert, Reverse, MirrorSymmetric, Normalise, RemoveDc, Smooth, Rectify, Fold, Quantise, Clear,
        ShiftPhase, StretchToCentre, CompressToCentre, Scale, Offset, Sharpen, HalfCycle, DoubleCycle
    };

    WaveDocument();

    // --------------------------------------------------------------------------- basic access
    int getNumFrames() const noexcept { return (int) frames.size(); }
    const Frame& getFrame (int i) const { return frames[(size_t) juce::jlimit (0, getNumFrames() - 1, i)]; }
    const std::vector<float>& getSamples (int i) const { return getFrame (i).samples; }
    int getSelectedFrame() const noexcept { return selected; }
    void setSelectedFrame (int f) noexcept { selected = juce::jlimit (0, getNumFrames() - 1, f); }
    const juce::String& getName() const noexcept { return name; }
    void setName (const juce::String& n) { name = n; ++version; }
    /** "factory:<name>" when unedited factory content, otherwise empty. */
    const juce::String& getFactorySource() const noexcept { return factorySource; }
    uint64_t getVersion() const noexcept { return version; }

    // ---------------------------------------------------------------------------- whole-table
    void loadFactory (int index);
    void setFrames (const wavegen::Frames& f, const juce::String& newName, bool undoable = true);
    void generateFromExpression (const juce::String& expr, int numFrames, juce::String& errorOut);
    void generateRandom (wavegen::RandomStyle style, uint32_t seed, float complexity, int numFrames, bool currentFrameOnly);

    // --------------------------------------------------------------------------- frame editing
    /** Call before an interactive gesture (snapshot for undo). */
    void beginGesture();
    /** Freehand: draws a straight segment between two points (x 0..1, y -1..1). */
    void drawSegment (int frame, float x0, float y0, float x1, float y1, bool allFrames);
    void setPoints (int frame, const std::vector<ControlPoint>& pts);
    void convertToPoints (int frame, int numPoints);
    void leavePointMode (int frame);
    void applyTransform (Transform t, float amount, bool allFrames);
    void setHarmonic (int frame, int harmonic, float magnitude, float phase, bool allFrames);
    void setHarmonics (int frame, const std::vector<float>& mags, const std::vector<float>& phases);
    void getHarmonics (int frame, std::vector<float>& mags, std::vector<float>& phases, int count) const;

    // ---------------------------------------------------------------------------- multi-frame
    void addFrame (bool duplicateSelected);
    void deleteFrame (int f);
    void moveFrame (int from, int to);
    /** Fills frames between keyframes a and b (exclusive) by crossfade or spectral morph. */
    void interpolateBetween (int a, int b, bool spectral);
    void resampleFrameCount (int newCount, bool spectral);

    // ---------------------------------------------------------------------------------- undo
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    // ---------------------------------------------------------------------------- build / IO
    std::unique_ptr<Wavetable> buildTable() const;
    wavegen::Frames getAllFrames() const;
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);

    static std::vector<float> renderPoints (const std::vector<ControlPoint>& pts);

private:
    struct Snapshot
    {
        std::vector<Frame> frames;
        juce::String name, factorySource;
        int selected = 0;
    };
    void pushUndo();
    void touched() { factorySource = {}; ++version; }
    std::vector<int> targetFrames (int frame, bool allFrames) const;

    std::vector<Frame> frames;
    juce::String name { "Basic Shapes" };
    juce::String factorySource;
    int selected = 0;
    uint64_t version = 1;
    std::vector<Snapshot> undoStack, redoStack;
    static constexpr size_t kMaxUndo = 48;
};
} // namespace dw
