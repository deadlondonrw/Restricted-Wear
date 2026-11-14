#include "SynthEngine.h"

namespace dw
{
SynthEngine::SynthEngine (juce::AudioProcessorValueTreeState& state)
    : apvts (state), uiModNorm ((size_t) P::Count)
{
    const auto& reg = Registry::get();
    raw.resize ((size_t) P::Count, nullptr);
    ranges.resize ((size_t) P::Count);
    isPerVoice.assign ((size_t) P::Count, 0);
    for (int i = 0; i < P::Count; ++i)
    {
        const auto& s = reg.spec (i);
        raw[(size_t) i] = apvts.getRawParameterValue (s.id);
        jassert (raw[(size_t) i] != nullptr);
        ranges[(size_t) i] = s.range();
        isPerVoice[(size_t) i] = s.perVoice ? 1 : 0;
    }
    base.assign ((size_t) P::Count, 0.0f);
    baseNorm.assign ((size_t) P::Count, 0.0f);
    values.assign ((size_t) P::Count, 0.0f);
    modActive.assign ((size_t) P::Count, 0);
    globalAccum.assign ((size_t) P::Count, 0.0f);
    voiceDests.reserve ((size_t) kNumModSlots);
    globalDests.reserve ((size_t) kNumModSlots);
    for (auto& a : modSrc) a.store (0);
    for (auto& a : modDst) a.store (-1);
    for (auto& a : uiSrc) a.store (0.0f);
    for (auto& a : uiWtPos) a.store (0.0f);
    for (auto& a : uiLfoPhase) a.store (0.0f);
    for (auto& a : uiModNorm) a.store (-1.0f);
}

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    baseRate = sampleRate;
    maxBlock = maxBlockSize;
    for (auto& v : voices) v.prepare (sampleRate, 2);
    for (auto& r : racks) r.prepare (sampleRate);
    for (auto& b : decim) for (auto& d : b) d.reset();
    midiFx.prepare (sampleRate);
    fxMidi.ensureSize (8192);
    for (int i = 0; i < kNumLfos; ++i) globalLfo[i].reset (LfoSettings {}, 0xABCDu + (uint32_t) i);
    allNotesOff (true);
    previewPos = -1.0;
    currentOs = 1;
    readParameters();
}

void SynthEngine::release()
{
    allNotesOff (true);
}

void SynthEngine::collectGarbage()
{
    for (auto& t : tables) t.collectGarbage();
    for (auto& s : samples) s.collectGarbage();
    for (auto& l : lfoShapes) l.collectGarbage();
    preview.collectGarbage();
}

float SynthEngine::getModulatedNormalised (int paramIndex) const noexcept
{
    if (paramIndex < 0 || paramIndex >= P::Count) return -1.0f;
    return uiModNorm[(size_t) paramIndex].load (std::memory_order_relaxed);
}

// =============================================================================== parameters / mod
void SynthEngine::readParameters() noexcept
{
    for (int i = 0; i < P::Count; ++i)
    {
        const float v = raw[(size_t) i]->load (std::memory_order_relaxed);
        base[(size_t) i] = v;
        values[(size_t) i] = v;
    }
}

void SynthEngine::buildModulation() noexcept
{
    for (int d : voiceDests) modActive[(size_t) d] = 0;
    voiceDests.clear();
    globalDests.clear();
    numSlots = 0;

    const auto& reg = Registry::get();
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = modSrc[(size_t) s].load (std::memory_order_relaxed);
        const int dst = modDst[(size_t) s].load (std::memory_order_relaxed);
        if (src <= 0 || src >= (int) ModSrc::NumSources || dst < 0 || dst >= P::Count) continue;
        if (! reg.spec (dst).modulatable) continue;
        if (base[(size_t) P::modAmt (s)] == 0.0f) continue;

        slots[numSlots++] = { src, dst, P::modAmt (s) };
        if (isPerVoice[(size_t) dst])
        {
            if (! modActive[(size_t) dst])
            {
                modActive[(size_t) dst] = 1;
                voiceDests.push_back (dst); // capacity reserved: no allocation
            }
        }
        else if (std::find (globalDests.begin(), globalDests.end(), dst) == globalDests.end())
            globalDests.push_back (dst);
    }

    for (int d : voiceDests) baseNorm[(size_t) d] = ranges[(size_t) d].convertTo0to1 (base[(size_t) d]);
    for (int d : globalDests) baseNorm[(size_t) d] = ranges[(size_t) d].convertTo0to1 (base[(size_t) d]);
}

void SynthEngine::updateGlobalModulation (double bpm) noexcept
{
    juce::ignoreUnused (bpm);
    if (globalDests.empty()) return;

    float src[(int) ModSrc::NumSources];
    for (int i = 0; i < (int) ModSrc::NumSources; ++i) src[i] = lastVoiceSrc[i];
    for (int i = 0; i < kNumLfos; ++i)
        if (ctx.lfo[i].mode == 0) src[(int) ModSrc::Lfo1 + i] = ctx.globalLfo[i];
    src[(int) ModSrc::ModWheel] = modWheel;
    src[(int) ModSrc::Aftertouch] = channelPressure;
    src[(int) ModSrc::PitchBend] = bendSmoothed;
    for (int i = 0; i < kNumMacros; ++i) src[(int) ModSrc::Macro1 + i] = ctx.macro[i];

    for (int d : globalDests) globalAccum[(size_t) d] = 0.0f;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& sl = slots[s];
        if (isPerVoice[(size_t) sl.dst]) continue;
        globalAccum[(size_t) sl.dst] += src[sl.src] * base[(size_t) sl.amtIdx];
    }
    for (int d : globalDests)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, baseNorm[(size_t) d] + globalAccum[(size_t) d]);
        values[(size_t) d] = ranges[(size_t) d].convertFrom0to1 (norm);
    }
}

// =============================================================================================== MIDI
void SynthEngine::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        bendTarget = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f);
    else if (m.isChannelPressure())
        channelPressure = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
    {
        for (auto& v : voices)
            if (v.isActive() && v.note == m.getNoteNumber())
                v.polyPressure = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int val = m.getControllerValue();
        if (cc == 1) modWheel = (float) val / 127.0f;
        else if (cc == 64)
        {
            const bool down = val >= 64;
            if (sustainDown && ! down)
            {
                sustainDown = false;
                if ((int) base[(size_t) P::voice (VoiceP::Mode)] == 0)
                {
                    for (auto& v : voices)
                        if (v.isActive() && v.sustained) { v.sustained = false; v.release(); }
                }
                else
                {
                    for (int n = 0; n < 128; ++n)
                        if (pendingMonoOff[n]) { pendingMonoOff[n] = false; monoNoteOff (n); }
                }
            }
            sustainDown = down;
        }
        else if (cc == 120) allNotesOff (true);
        else if (cc == 123) allNotesOff (false);
    }
    else if (m.isAllNotesOff())
        allNotesOff (false);
    else if (m.isAllSoundOff())
        allNotesOff (true);
}

float SynthEngine::glideSourcePitch (bool anyHeldBefore) const noexcept
{
    const int mode = (int) base[(size_t) P::voice (VoiceP::GlideMode)];
    if (mode == 0 || lastPitch < 0.0f) return -1.0f;
    if (mode == 1) return lastPitch;
    return anyHeldBefore ? lastPitch : -1.0f;
}

Voice* SynthEngine::findFreeVoice() noexcept
{
    for (auto& v : voices)
        if (! v.isActive()) return &v;
    // All physical voices busy: hard-recycle the quietest fading/releasing voice.
    Voice* best = nullptr;
    for (auto& v : voices)
        if (best == nullptr || (v.isReleasing() && ! best->isReleasing())
            || (v.isReleasing() == best->isReleasing() && v.getAmpLevel() < best->getAmpLevel()))
            best = &v;
    if (best != nullptr) best->kill();
    return best;
}

void SynthEngine::noteOn (int note, float vel) noexcept
{
    note = juce::jlimit (0, 127, note);
    bool anyHeldBefore = false;
    for (int n = 0; n < 128; ++n)
        if (keyDown[n] && n != note) { anyHeldBefore = true; break; }
    keyDown[note] = true;
    keyVel[note] = vel;

    const int mode = (int) base[(size_t) P::voice (VoiceP::Mode)];
    if (mode != 0)
    {
        monoNoteOn (note, vel);
        return;
    }

    // Re-pressing a sounding key releases its previous voice.
    for (auto& v : voices)
        if (v.isActive() && v.note == note && ! v.isReleasing())
            v.release();

    const int poly = juce::jlimit (1, kMaxVoices, (int) base[(size_t) P::voice (VoiceP::Polyphony)]);
    int activeCount = 0;
    for (auto& v : voices)
        if (v.isActive() && ! v.isFading()) ++activeCount;

    while (activeCount >= poly)
    {
        Voice* victim = nullptr;
        for (auto& v : voices)
        {
            if (! v.isActive() || v.isFading()) continue;
            if (victim == nullptr) { victim = &v; continue; }
            const bool vr = v.isReleasing(), br = victim->isReleasing();
            if (vr && ! br) victim = &v;
            else if (vr == br && v.age < victim->age) victim = &v;
        }
        if (victim == nullptr) break;
        victim->steal();
        --activeCount;
    }

    Voice* v = findFreeVoice();
    if (v == nullptr) return;
    v->start (note, vel, glideSourcePitch (anyHeldBefore), ageCounter++, rng.next(), ctx);
    lastPitch = (float) note;
}

void SynthEngine::noteOff (int note) noexcept
{
    note = juce::jlimit (0, 127, note);
    keyDown[note] = false;
    const int mode = (int) base[(size_t) P::voice (VoiceP::Mode)];
    if (mode != 0)
    {
        monoNoteOff (note);
        return;
    }
    for (auto& v : voices)
    {
        if (! v.isActive() || v.note != note || ! v.keyHeld) continue;
        if (sustainDown) { v.sustained = true; v.keyHeld = false; }
        else v.release();
    }
}

int SynthEngine::pickMonoNote() const noexcept
{
    if (monoCount == 0) return -1;
    const int prio = (int) base[(size_t) P::voice (VoiceP::Priority)];
    if (prio == 0) return monoStack[monoCount - 1];
    int best = monoStack[0];
    for (int i = 1; i < monoCount; ++i)
        best = prio == 1 ? juce::jmin (best, monoStack[i]) : juce::jmax (best, monoStack[i]);
    return best;
}

void SynthEngine::monoNoteOn (int note, float vel) noexcept
{
    const bool overlap = monoCount > 0;
    for (int i = 0; i < monoCount; ++i)
        if (monoStack[i] == note)
        {
            for (int k = i; k < monoCount - 1; ++k) monoStack[k] = monoStack[k + 1];
            --monoCount;
            break;
        }
    if (monoCount < 128) monoStack[monoCount++] = note;
    pendingMonoOff[note] = false;

    const int target = pickMonoNote();
    Voice& v = voices[0];
    const int glideMode = (int) base[(size_t) P::voice (VoiceP::GlideMode)];
    const bool legatoMode = (int) base[(size_t) P::voice (VoiceP::Mode)] == 2;
    const bool hard = base[(size_t) P::voice (VoiceP::Retrigger)] >= 0.5f;

    if (! v.isActive())
    {
        const float from = glideMode == 1 && lastPitch >= 0.0f ? lastPitch : -1.0f;
        v.start (target, keyVel[target], from, ageCounter++, rng.next(), ctx);
    }
    else if (target == v.note && ! v.isReleasing())
    {
        // Priority keeps the current note; nothing changes.
    }
    else
    {
        const bool glide = glideMode == 1 || (glideMode == 2 && overlap);
        const bool retrig = ! (legatoMode && overlap);
        v.legatoTo (target, target == note ? vel : keyVel[target], glide, retrig, hard, ageCounter++);
    }
    lastPitch = (float) target;
}

void SynthEngine::monoNoteOff (int note) noexcept
{
    if (sustainDown)
    {
        pendingMonoOff[note] = true;
        return;
    }
    for (int i = 0; i < monoCount; ++i)
        if (monoStack[i] == note)
        {
            for (int k = i; k < monoCount - 1; ++k) monoStack[k] = monoStack[k + 1];
            --monoCount;
            break;
        }

    Voice& v = voices[0];
    if (! v.isActive()) return;

    if (monoCount == 0)
    {
        v.release();
        return;
    }
    const int target = pickMonoNote();
    if (target != v.note && base[(size_t) P::voice (VoiceP::ReturnToHeld)] >= 0.5f)
    {
        const int glideMode = (int) base[(size_t) P::voice (VoiceP::GlideMode)];
        v.legatoTo (target, keyVel[target], glideMode != 0, false, false, ageCounter++);
        lastPitch = (float) target;
    }
    else if (target != v.note)
        v.release();
}

void SynthEngine::allNotesOff (bool hard) noexcept
{
    for (auto& v : voices)
    {
        if (hard) v.kill();
        else v.release();
    }
    monoCount = 0;
    for (int n = 0; n < 128; ++n) { keyDown[n] = false; pendingMonoOff[n] = false; }
    sustainDown = false;
}

// ============================================================================================ process
void SynthEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples == 0) return;

    if (panicRequest.exchange (false))
    {
        allNotesOff (true);
        for (auto& r : racks) r.reset();
    }

    for (int i = 0; i < kNumOsc; ++i)
    {
        ctx.tables[i] = tables[i].acquire();
        ctx.samples[i] = samples[i].acquire();
    }

    readParameters();
    buildModulation();

    const int voiceMode = (int) base[(size_t) P::voice (VoiceP::Mode)];
    if (voiceMode != lastVoiceMode)
    {
        if (lastVoiceMode >= 0) allNotesOff (false);
        lastVoiceMode = voiceMode;
    }

    const int os = (int) base[(size_t) P::global (GlobalP::Quality)] >= 1 ? 2 : 1;
    if (os != currentOs)
    {
        for (auto& b : decim) for (auto& d : b) d.reset();
        currentOs = os;
    }

    const double bpm = juce::jlimit (20.0, 999.0, transport.bpm);
    ctx.base = base.data();
    ctx.baseNorm = baseNorm.data();
    ctx.modActive = modActive.data();
    ctx.ranges = ranges.data();
    ctx.slots = slots;
    ctx.numSlots = numSlots;
    ctx.voiceDests = voiceDests.data();
    ctx.numVoiceDests = (int) voiceDests.size();
    ctx.processRate = baseRate * os;
    ctx.oversample = os;
    ctx.ultra = os == 2;

    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& s = ctx.lfo[i];
        s.shape = (int) base[(size_t) P::lfo (i, LfoP::Shape)];
        s.mode = (int) base[(size_t) P::lfo (i, LfoP::Mode)];
        const bool sync = base[(size_t) P::lfo (i, LfoP::Sync)] >= 0.5f;
        s.rateHz = sync ? (float) (bpm / 60.0 / Choices::divisionToBeats ((int) base[(size_t) P::lfo (i, LfoP::Div)]))
                        : base[(size_t) P::lfo (i, LfoP::Rate)];
        s.phase = base[(size_t) P::lfo (i, LfoP::Phase)];
        s.smooth = base[(size_t) P::lfo (i, LfoP::Smooth)];
        s.rise = base[(size_t) P::lfo (i, LfoP::Rise)];
        s.bipolar = base[(size_t) P::lfo (i, LfoP::Bipolar)] >= 0.5f;
        s.custom = lfoShapes[i].acquire();
    }
    for (int i = 0; i < kNumEnvs; ++i)
    {
        auto& e = ctx.envBase[i];
        e.aCurve = base[(size_t) P::env (i, EnvP::AttackCurve)];
        e.dCurve = base[(size_t) P::env (i, EnvP::DecayCurve)];
        e.rCurve = base[(size_t) P::env (i, EnvP::ReleaseCurve)];
    }

    // ---- MIDI FX -------------------------------------------------------------------------------
    midiFx.process (midi, fxMidi, numSamples, bpm, transport.playing, transport.ppq, base.data());

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float monoScratch[kChunk];

    const double samplesPerBeat = baseRate * 60.0 / bpm;
    int pos = 0;
    auto renderUpTo = [&] (int endPos)
    {
        while (pos < endPos)
        {
            const int n = juce::jmin (kChunk, endPos - pos);
            TransportInfo t = transport;
            t.bpm = bpm;
            t.ppq = transport.ppq + pos / samplesPerBeat;
            renderChunk (outL + pos, outR != nullptr ? outR + pos : monoScratch, n, t);
            if (outR == nullptr)
                for (int i = 0; i < n; ++i) outL[pos + i] = 0.5f * (outL[pos + i] + monoScratch[i]);
            pos += n;
        }
    };

    for (const auto meta : fxMidi)
    {
        renderUpTo (juce::jlimit (0, numSamples, meta.samplePosition));
        handleMidi (meta.getMessage());
    }
    renderUpTo (numSamples);

    // ---- preview player (sample import audition) ----------------------------------------------
    if (previewStop.exchange (false)) previewPos = -1.0;
    auto* clip = preview.acquire();
    if (previewStart.exchange (false)) previewPos = 0.0;
    if (clip != nullptr && previewPos >= 0.0)
    {
        const double step = clip->rate / baseRate;
        const int len = (int) clip->l.size();
        for (int i = 0; i < numSamples && previewPos >= 0.0; ++i)
        {
            const int k = (int) previewPos;
            if (k + 1 >= len) { previewPos = -1.0; break; }
            const float t = (float) (previewPos - k);
            const float l = clip->l[(size_t) k] + (clip->l[(size_t) k + 1] - clip->l[(size_t) k]) * t;
            const auto& rr = clip->r.empty() ? clip->l : clip->r;
            const float r = rr[(size_t) k] + (rr[(size_t) k + 1] - rr[(size_t) k]) * t;
            outL[i] += l * 0.8f;
            if (outR != nullptr) outR[i] += r * 0.8f;
            previewPos += step;
        }
    }

    // ---- safety + telemetry -------------------------------------------------------------------
    float peak = 0.0f;
    bool bad = false;
    for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
    {
        const float* d = buffer.getReadPointer (c);
        for (int i = 0; i < numSamples; ++i)
        {
            if (! std::isfinite (d[i])) { bad = true; break; }
            peak = juce::jmax (peak, std::abs (d[i]));
        }
    }
    if (bad)
    {
        buffer.clear();
        allNotesOff (true);
        for (auto& r : racks) r.reset();
        peak = 0.0f;
    }
    else if (peak > 8.0f)
    {
        // Hard safety ceiling (+18 dBFS): protects ears and speakers from runaway feedback.
        for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
        {
            float* d = buffer.getWritePointer (c);
            for (int i = 0; i < numSamples; ++i) d[i] = juce::jlimit (-8.0f, 8.0f, d[i]);
        }
    }
    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    uiPeak.store (juce::jmax (peak, uiPeak.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    int count = 0;
    const Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.isActive())
        {
            ++count;
            if (newest == nullptr || v.age > newest->age) newest = &v;
        }
    uiActiveVoices.store (count, std::memory_order_relaxed);
    for (int s = 1; s < (int) ModSrc::NumSources; ++s)
        uiSrc[(size_t) s].store (newest != nullptr ? newest->getModSourceValue ((ModSrc) s) : lastVoiceSrc[s], std::memory_order_relaxed);
    for (int i = 0; i < kNumLfos; ++i)
        uiLfoPhase[(size_t) i].store ((float) globalLfo[i].getPhase(), std::memory_order_relaxed);

    // Modulated knob positions for the UI (newest voice for per-voice dests).
    for (int d = 0; d < P::Count; ++d)
        if (uiModNorm[(size_t) d].load (std::memory_order_relaxed) >= 0.0f && ! modActive[(size_t) d]
            && std::find (globalDests.begin(), globalDests.end(), d) == globalDests.end())
            uiModNorm[(size_t) d].store (-1.0f, std::memory_order_relaxed);
    for (int d : voiceDests)
    {
        float norm = baseNorm[(size_t) d];
        if (newest != nullptr)
        {
            // Recompute the newest voice's normalised value from its sources.
            float acc = 0.0f;
            for (int s = 0; s < numSlots; ++s)
                if (slots[s].dst == d) acc += newest->getModSourceValue ((ModSrc) slots[s].src) * base[(size_t) slots[s].amtIdx];
            norm = juce::jlimit (0.0f, 1.0f, norm + acc);
        }
        uiModNorm[(size_t) d].store (norm, std::memory_order_relaxed);
    }
    for (int d : globalDests)
        uiModNorm[(size_t) d].store (ranges[(size_t) d].convertTo0to1 (values[(size_t) d]), std::memory_order_relaxed);
    for (int i = 0; i < kNumOsc; ++i)
    {
        const int idx = P::osc (i, OscP::WtPos);
        const float m = uiModNorm[(size_t) idx].load (std::memory_order_relaxed);
        uiWtPos[(size_t) i].store (m >= 0.0f ? m : base[(size_t) idx], std::memory_order_relaxed);
    }
}

void SynthEngine::renderChunk (float* outL, float* outR, int n, const TransportInfo& t) noexcept
{
    const float dt = (float) n / (float) baseRate;

    // ---- global controllers ---------------------------------------------------------------------
    const float bendCoef = 1.0f - std::exp (-dt / 0.004f);
    bendSmoothed += (bendTarget - bendSmoothed) * bendCoef;
    ctx.pitchBendNorm = bendSmoothed;
    ctx.pitchBendSemis = bendSmoothed >= 0.0f ? bendSmoothed * base[(size_t) P::voice (VoiceP::BendUp)]
                                              : bendSmoothed * base[(size_t) P::voice (VoiceP::BendDown)];
    ctx.modWheel = modWheel;
    ctx.channelPressure = channelPressure;
    for (int i = 0; i < kNumMacros; ++i) ctx.macro[i] = base[(size_t) P::macro (i)];

    for (int i = 0; i < kNumLfos; ++i)
    {
        const bool sync = base[(size_t) P::lfo (i, LfoP::Sync)] >= 0.5f;
        if (sync && t.playing && ctx.lfo[i].mode == 0)
        {
            const double beats = Choices::divisionToBeats ((int) base[(size_t) P::lfo (i, LfoP::Div)]);
            globalLfo[i].setPhase (t.ppq / beats);
            ctx.globalLfo[i] = globalLfo[i].advance (ctx.lfo[i], dt, true);
        }
        else
            ctx.globalLfo[i] = globalLfo[i].advance (ctx.lfo[i], dt);
    }

    updateGlobalModulation (t.bpm);

    // ---- voices -------------------------------------------------------------------------------
    const int os = currentOs;
    const int pn = n * os;
    for (int b = 0; b < 1 + kNumOsc; ++b)
    {
        std::fill (busL[b], busL[b] + pn, 0.0f);
        std::fill (busR[b], busR[b] + pn, 0.0f);
    }
    for (auto& u : laneUsed) u = false;
    float* laneL[kNumOsc] = { busL[1], busL[2], busL[3], busL[4] };
    float* laneR[kNumOsc] = { busR[1], busR[2], busR[3], busR[4] };

    const Voice* newest = nullptr;
    for (auto& v : voices)
    {
        if (! v.isActive()) continue;
        v.render (ctx, busL[0], busR[0], laneL, laneR, laneUsed, pn);
        if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
    }
    if (newest != nullptr)
        for (int s = 0; s < (int) ModSrc::NumSources; ++s)
            lastVoiceSrc[s] = newest->getModSourceValue ((ModSrc) s);

    // ---- decimation ----------------------------------------------------------------------------
    bool laneOn[kNumOsc];
    for (int i = 0; i < kNumOsc; ++i)
        laneOn[i] = base[(size_t) P::osc (i, OscP::FxOn)] >= 0.5f && base[(size_t) P::osc (i, OscP::On)] >= 0.5f;

    for (int b = 0; b < 1 + kNumOsc; ++b)
    {
        if (b > 0 && ! laneOn[b - 1] && ! laneWasActive[b - 1]) continue;
        if (os == 2)
        {
            decim[b][0].process (busL[b], outBufL[b], n);
            decim[b][1].process (busR[b], outBufR[b], n);
        }
        else
        {
            std::copy (busL[b], busL[b] + n, outBufL[b]);
            std::copy (busR[b], busR[b] + n, outBufR[b]);
        }
    }

    // ---- oscillator lanes ---------------------------------------------------------------------
    const bool lanesToMaster = (int) base[(size_t) P::global (GlobalP::VoiceFxRouting)] == 0;
    float directL[kChunk] {}, directR[kChunk] {};
    for (int i = 0; i < kNumOsc; ++i)
    {
        if (! laneOn[i])
        {
            if (laneWasActive[i]) { racks[1 + i].reset(); laneWasActive[i] = false; }
            continue;
        }
        laneWasActive[i] = true;
        int paramRack = 1 + i;
        if (base[(size_t) P::osc (i, OscP::FxLink)] >= 0.5f)
            for (int j = 0; j < i; ++j)
                if (base[(size_t) P::osc (j, OscP::FxLink)] >= 0.5f) { paramRack = 1 + j; break; }

        racks[1 + i].process (outBufL[1 + i], outBufR[1 + i], n, values.data(), paramRack, t.bpm);
        float* dl = lanesToMaster ? outBufL[0] : directL;
        float* dr = lanesToMaster ? outBufR[0] : directR;
        for (int s = 0; s < n; ++s) { dl[s] += outBufL[1 + i][s]; dr[s] += outBufR[1 + i][s]; }
    }

    // ---- master ------------------------------------------------------------------------------
    racks[0].process (outBufL[0], outBufR[0], n, values.data(), 0, t.bpm);
    const float gain = dbToGain (values[(size_t) P::global (GlobalP::MasterVol)]);
    for (int s = 0; s < n; ++s)
    {
        outL[s] = (outBufL[0][s] + directL[s]) * gain;
        outR[s] = (outBufR[0][s] + directR[s]) * gain;
    }
}
} // namespace dw
