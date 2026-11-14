#include "MidiFx.h"

namespace dw
{
void MidiFx::prepare (double sampleRate)
{
    fs = sampleRate;
    reset();
}

void MidiFx::reset()
{
    numHeld = numLatched = numOffs = 0;
    latchArmed = true;
    sampleClock = 0;
    freeBeat = 0.0;
    freeRunning = false;
    stepCounter = 0;
    arpIndex = 0; arpDir = 1; cycle = 0; notesInCycle = 0;
    uiStep.store (-1);
}

void MidiFx::addHeld (int note, int vel)
{
    removeHeld (note);
    if (numHeld < 64)
        held[numHeld++] = { note, vel, orderCounter++ };
}

void MidiFx::removeHeld (int note)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[i].note == note)
        {
            for (int k = i; k < numHeld - 1; ++k) held[k] = held[k + 1];
            --numHeld;
            return;
        }
}

void MidiFx::flushAll (juce::MidiBuffer& out, int pos)
{
    for (int i = 0; i < numOffs; ++i)
        out.addEvent (juce::MidiMessage::noteOff (1, offs[i].note), pos);
    numOffs = 0;
}

int MidiFx::buildArpList (int* notes, int* vels, const float* v)
{
    const bool hold = v[P::arp (ArpP::ArpHold)] >= 0.5f;
    const Held* src = hold ? latched : held;
    const int count = hold ? numLatched : numHeld;
    if (count == 0) return 0;

    Held tmp[64];
    for (int i = 0; i < count; ++i) tmp[i] = src[i];

    const int mode = (int) v[P::arp (ArpP::ArpMode)];
    if (mode == 5) // as played
        std::sort (tmp, tmp + count, [] (const Held& a, const Held& b) { return a.order < b.order; });
    else
        std::sort (tmp, tmp + count, [] (const Held& a, const Held& b) { return a.note < b.note; });

    const int octaves = juce::jlimit (1, 4, (int) v[P::arp (ArpP::ArpOctaves)]);
    int n = 0;
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < count && n < 256; ++i)
        {
            const int note = tmp[i].note + 12 * o;
            if (note > 127) continue;
            notes[n] = note;
            vels[n] = tmp[i].vel;
            ++n;
        }
    return n;
}

void MidiFx::emitStep (juce::MidiBuffer& out, int pos, int64_t absPos, double stepSamples, const float* v, int mode)
{
    if (mode == 1) // ---------------------------------------------------------------- arpeggiator
    {
        int notes[256], vels[256];
        const int n = buildArpList (notes, vels, v);
        if (n == 0) { uiStep.store (-1); return; }

        const int arpMode = (int) v[P::arp (ArpP::ArpMode)];
        const int steps = juce::jlimit (1, 16, (int) v[P::arp (ArpP::ArpSteps)]);
        const int transposeStep = (int) v[P::arp (ArpP::ArpPitchStep)];
        const float gate = v[P::arp (ArpP::ArpGate)];
        const bool fixedVel = (int) v[P::arp (ArpP::ArpVelMode)] == 1;
        const int fixed = juce::jlimit (1, 127, (int) std::lround (v[P::arp (ArpP::ArpVel)] * 127.0f));

        if (notesInCycle >= steps)
        {
            notesInCycle = 0;
            cycle = (cycle + 1) % 4;
            arpIndex = 0;
            arpDir = 1;
        }
        const int transpose = transposeStep * cycle;

        auto play = [&] (int note, int vel)
        {
            note = juce::jlimit (0, 127, note + transpose);
            const int velocity = fixedVel ? fixed : vel;
            out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)), pos);
            if (numOffs < 128)
                offs[numOffs++] = { note, absPos + (int64_t) juce::jmax (1.0, stepSamples * gate) };
        };

        if (arpMode == 6) // chord
        {
            const int count = juce::jmin (n, 16);
            for (int i = 0; i < count; ++i) play (notes[i], vels[i]);
        }
        else
        {
            int idx = 0;
            switch (arpMode)
            {
                case 0: case 5: idx = arpIndex % n; arpIndex = (arpIndex + 1) % n; break;
                case 1: idx = n - 1 - (arpIndex % n); arpIndex = (arpIndex + 1) % n; break;
                case 2: case 3:
                {
                    if (n == 1) { idx = 0; break; }
                    const int period = 2 * n - 2;
                    const int k = arpIndex % period;
                    int up = k < n ? k : period - k;
                    idx = arpMode == 2 ? up : n - 1 - up;
                    arpIndex = (arpIndex + 1) % period;
                    break;
                }
                case 4: idx = (int) (rng.uni() * (float) n) % n; break;
                default: idx = 0; break;
            }
            play (notes[idx], vels[idx]);
        }
        ++notesInCycle;
        uiStep.store ((int) (stepCounter % 16));
        return;
    }

    // -------------------------------------------------------------------------------- sequencer
    if (numHeld == 0) { uiStep.store (-1); return; }
    const int length = juce::jlimit (1, kNumSeqSteps, (int) v[P::seq (SeqP::Length)]);
    const int s = (int) (stepCounter % length);
    uiStep.store (s);
    if (v[P::seqStep (s, SeqStepP::On)] < 0.5f) return;

    // root = most recently pressed held note
    const Held* root = &held[0];
    for (int i = 1; i < numHeld; ++i)
        if (held[i].order > root->order) root = &held[i];

    const int note = juce::jlimit (0, 127, root->note + (int) v[P::seqStep (s, SeqStepP::Pitch)]);
    const int vel = juce::jlimit (1, 127, (int) std::lround (v[P::seqStep (s, SeqStepP::Velocity)] * 127.0f));
    const float gate = v[P::seqStep (s, SeqStepP::Gate)] * v[P::seq (SeqP::GateScale)];
    out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), pos);
    if (numOffs < 128)
        offs[numOffs++] = { note, absPos + (int64_t) juce::jmax (1.0, stepSamples * gate) };
}

void MidiFx::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, double bpm, bool playing,
                      double ppqStart, const float* v) noexcept
{
    out.clear();
    const int mode = (int) v[P::arp (ArpP::MidiFxMode)];

    if (mode != lastMode)
    {
        flushAll (out, 0);
        numHeld = numLatched = 0;
        freeRunning = false;
        lastMode = mode;
        uiStep.store (-1);
    }

    if (mode == 0)
    {
        for (const auto meta : in)
            out.addEvent (meta.getMessage(), meta.samplePosition);
        sampleClock += numSamples;
        return;
    }

    bpm = juce::jlimit (20.0, 999.0, bpm);
    const double rateIdx = mode == 1 ? v[P::arp (ArpP::ArpRate)] : v[P::seq (SeqP::Rate)];
    const double stepBeats = Choices::noteDivisionToBeats ((int) rateIdx);
    const double swing = (mode == 1 ? v[P::arp (ArpP::ArpSwing)] : v[P::seq (SeqP::Swing)]) * 0.5;
    const double samplesPerBeat = fs * 60.0 / bpm;
    const double stepSamples = stepBeats * samplesPerBeat;
    const bool hold = mode == 1 && v[P::arp (ArpP::ArpHold)] >= 0.5f;

    // Gather step boundaries in this block (sample offsets).
    int stepPos[64];
    int64_t stepIdx[64];
    int numSteps = 0;

    auto computeSteps = [&] (double blockBeatStart, int fromSample)
    {
        numSteps = 0;
        const double beatEnd = blockBeatStart + numSamples / samplesPerBeat;
        int64_t k = (int64_t) std::floor (blockBeatStart / stepBeats) - 1;
        for (; numSteps < 64; ++k)
        {
            double b = (double) k * stepBeats;
            if (k % 2 != 0) b += swing * stepBeats;
            if (b >= beatEnd) break;
            if (b < blockBeatStart - 1.0e-9) continue;
            const int pos = (int) std::floor ((b - blockBeatStart) * samplesPerBeat + 1.0e-6);
            if (pos < fromSample) continue;
            stepPos[numSteps] = juce::jlimit (0, numSamples - 1, pos);
            stepIdx[numSteps] = k;
            ++numSteps;
        }
    };
    computeSteps (playing ? ppqStart : freeBeat, 0);

    auto it = in.begin();
    const auto end = in.end();
    int stepCursor = 0;

    auto handleInput = [&] (const juce::MidiMessage& m, int pos)
    {
        if (m.isNoteOn())
        {
            const bool wasEmpty = numHeld == 0;
            addHeld (m.getNoteNumber(), m.getVelocity());
            if (hold)
            {
                if (latchArmed) { numLatched = 0; latchArmed = false; }
                bool exists = false;
                for (int i = 0; i < numLatched; ++i) exists |= latched[i].note == m.getNoteNumber();
                if (! exists && numLatched < 64) latched[numLatched++] = held[numHeld - 1];
            }
            if (wasEmpty && ! playing)
            {
                // Start the internal clock so the first step lands on this key press.
                freeRunning = true;
                freeBeat = -(double) pos / samplesPerBeat; // beat 0 lands exactly on this key press
                arpIndex = 0; arpDir = 1; cycle = 0; notesInCycle = 0;
                stepCounter = 0;
                emitStep (out, pos, sampleClock + pos, stepSamples, v, mode);
                stepCounter = 1;
                computeSteps (freeBeat, pos + 1);
                stepCursor = 0;
            }
            if (wasEmpty && playing)
            {
                arpIndex = 0; cycle = 0; notesInCycle = 0;
            }
        }
        else if (m.isNoteOff())
        {
            removeHeld (m.getNoteNumber());
            if (numHeld == 0)
                latchArmed = true;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            numHeld = numLatched = 0;
            flushAll (out, pos);
        }
        else
            out.addEvent (m, pos); // CCs, pitch bend, pressure pass through
    };

    for (int s = 0; s <= numSamples; ++s)
    {
        // emit due note-offs at this sample
        if (s < numSamples)
        {
            for (int i = 0; i < numOffs;)
            {
                if (offs[i].at <= sampleClock + s)
                {
                    out.addEvent (juce::MidiMessage::noteOff (1, offs[i].note), s);
                    offs[i] = offs[--numOffs];
                }
                else ++i;
            }
        }
        if (s == numSamples) break;

        // inputs at this sample come before a step at the same sample
        while (it != end && (*it).samplePosition <= s)
        {
            const auto meta = *it;
            handleInput (meta.getMessage(), meta.samplePosition);
            ++it;
        }

        while (stepCursor < numSteps && stepPos[stepCursor] == s)
        {
            const bool active = mode == 1 ? (hold ? numLatched > 0 : numHeld > 0) : numHeld > 0;
            if (active && (playing || freeRunning))
            {
                if (playing) stepCounter = stepIdx[stepCursor];
                // avoid double-firing the step emitted directly on key press
                if (! (! playing && stepIdx[stepCursor] == 0))
                {
                    emitStep (out, s, sampleClock + s, stepSamples, v, mode);
                    if (! playing) ++stepCounter;
                }
            }
            ++stepCursor;
        }

        // Fast-forward: if nothing is scheduled, jump to the next interesting sample.
        int next = numSamples;
        if (it != end) next = juce::jmin (next, (*it).samplePosition);
        if (stepCursor < numSteps) next = juce::jmin (next, stepPos[stepCursor]);
        for (int i = 0; i < numOffs; ++i)
            next = juce::jmin (next, (int) juce::jlimit ((int64_t) s + 1, (int64_t) numSamples, offs[i].at - sampleClock));
        if (next > s + 1) s = next - 1;
    }

    // Clock bookkeeping.
    const bool anyActive = mode == 1 ? (hold ? numLatched > 0 : numHeld > 0) : numHeld > 0;
    if (! playing)
    {
        if (freeRunning) freeBeat += numSamples / samplesPerBeat;
        if (! anyActive) { freeRunning = false; freeBeat = 0.0; uiStep.store (-1); }
    }
    sampleClock += numSamples;
}
} // namespace dw
