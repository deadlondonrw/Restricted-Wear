#pragma once

#include "../Core/DspUtil.h"
#include "../Core/Params.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dw
{
/**
    Arpeggiator and step sequencer. Converts the incoming note stream into generated notes,
    sample-accurately, locked to the host's musical position when the transport runs and to an
    internal clock (started by the first key press) when it does not.
*/
class MidiFx
{
public:
    void prepare (double sampleRate);
    void reset();

    /** v: real parameter values (global index). out must have capacity reserved. */
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, double bpm, bool playing,
                  double ppqStart, const float* v) noexcept;

    /** Step index currently playing (for UI), -1 if idle. */
    int getCurrentStep() const noexcept { return uiStep.load (std::memory_order_relaxed); }

private:
    struct Held { int note; int vel; uint32_t order; };
    struct PendingOff { int note; int64_t at; };

    void addHeld (int note, int vel);
    void removeHeld (int note);
    void flushAll (juce::MidiBuffer& out, int pos);
    void emitStep (juce::MidiBuffer& out, int pos, int64_t absPos, double stepSamples, const float* v, int mode);
    int buildArpList (int* notes, int* vels, const float* v);

    double fs = 48000.0;
    Held held[64];
    int numHeld = 0;
    Held latched[64];
    int numLatched = 0;
    bool latchArmed = true;
    uint32_t orderCounter = 0;

    PendingOff offs[128];
    int numOffs = 0;

    int64_t sampleClock = 0;
    double freeBeat = 0.0;
    bool freeRunning = false;
    int64_t stepCounter = 0;
    int lastMode = 0;
    int arpIndex = 0, arpDir = 1, cycle = 0, notesInCycle = 0;
    Rng rng { 0x51A7u };
    std::atomic<int> uiStep { -1 };
};
} // namespace dw
