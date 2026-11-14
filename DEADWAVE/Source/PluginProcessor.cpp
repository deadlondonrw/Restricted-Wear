#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/PresetManager.h"

namespace dw
{
DeadwaveProcessor::DeadwaveProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", Registry::get().createLayout()),
      engine (apvts)
{
    const auto& reg = Registry::get();
    params.resize ((size_t) P::Count, nullptr);
    for (int i = 0; i < P::Count; ++i)
    {
        params[(size_t) i] = apvts.getParameter (reg.spec (i).id);
        jassert (params[(size_t) i] != nullptr);
    }

    for (auto& l : lfoPoints) l = stateio::defaultLfoPoints();
    for (auto& o : rackOrders) o = FxRack::defaultOrder();

    // Initial tables (built once, copied) so the first note never waits for the builder thread.
    auto first = waves[0].buildTable();
    for (int i = 0; i < kNumOsc; ++i)
    {
        engine.setWavetable (i, std::make_unique<Wavetable> (*first));
        builtVersion[i] = waves[i].getVersion();
    }
    for (int i = 0; i < kNumLfos; ++i)
        engine.setLfoShape (i, stateio::renderLfo (lfoPoints[i]));

    presets = std::make_unique<PresetManager> (*this);
    builder.startThread (juce::Thread::Priority::low);
    startTimerHz (30);
}

DeadwaveProcessor::~DeadwaveProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    builder.signalThreadShouldExit();
    builder.stopThread (4000);
}

// =============================================================================== AudioProcessor
void DeadwaveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void DeadwaveProcessor::releaseResources()
{
    engine.release();
}

bool DeadwaveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet().isDisabled() || layouts.inputBuses.isEmpty();
}

void DeadwaveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    TransportInfo t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }
    engine.process (buffer, midi, t);
}

juce::AudioProcessorEditor* DeadwaveProcessor::createEditor()
{
    return new DeadwaveEditor (*this);
}

// ========================================================================================= state
void DeadwaveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state;
    {
        const juce::ScopedLock sl (stateLock);
        state = pendingState.isValid() ? pendingState.createCopy() : captureState();
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DeadwaveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || ! tree.hasType ("DEADWAVE")) return;

    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        applyState (tree);
        return;
    }
    // Off the message thread: parameters are atomic and applied now; wavetables, samples and the
    // matrix are applied on the message thread (the UI owns those objects).
    applyParameters (tree);
    {
        const juce::ScopedLock sl (stateLock);
        pendingState = tree;
    }
    triggerAsyncUpdate();
}

void DeadwaveProcessor::handleAsyncUpdate()
{
    juce::ValueTree tree;
    {
        const juce::ScopedLock sl (stateLock);
        tree = pendingState;
    }
    if (tree.isValid())
        applyState (tree);
}

juce::ValueTree DeadwaveProcessor::captureState() const
{
    const juce::ScopedLock sl (stateLock);
    juce::ValueTree root ("DEADWAVE");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("preset", currentPresetName, nullptr);

    juce::ValueTree ps ("PARAMS");
    const auto& reg = Registry::get();
    for (int i = 0; i < P::Count; ++i)
    {
        const auto* p = params[(size_t) i];
        const float v = p->convertFrom0to1 (p->getValue());
        if (std::abs (v - reg.spec (i).def) < 1.0e-6f) continue; // compact: defaults are implicit
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", reg.spec (i).id, nullptr);
        c.setProperty ("value", v, nullptr);
        ps.appendChild (c, nullptr);
    }
    root.appendChild (ps, nullptr);

    juce::ValueTree w ("WAVES");
    for (int i = 0; i < kNumOsc; ++i)
    {
        auto c = waves[i].toValueTree();
        c.setProperty ("osc", i, nullptr);
        w.appendChild (c, nullptr);
    }
    root.appendChild (w, nullptr);

    juce::ValueTree smp ("SAMPLES");
    for (int i = 0; i < kNumOsc; ++i)
        if (sampleAssets[i] != nullptr)
        {
            auto c = stateio::sampleToTree (*sampleAssets[i]);
            c.setProperty ("osc", i, nullptr);
            smp.appendChild (c, nullptr);
        }
    root.appendChild (smp, nullptr);

    juce::ValueTree lfos ("LFOSHAPES");
    for (int i = 0; i < kNumLfos; ++i)
    {
        juce::ValueTree c ("LFO");
        c.setProperty ("index", i, nullptr);
        c.setProperty ("pts", stateio::lfoPointsToString (lfoPoints[i]), nullptr);
        lfos.appendChild (c, nullptr);
    }
    root.appendChild (lfos, nullptr);

    juce::ValueTree mm ("MATRIX");
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const auto& r = routings[(size_t) s];
        if (! r.isUsed()) continue;
        juce::ValueTree c ("SLOT");
        c.setProperty ("index", s, nullptr);
        c.setProperty ("src", modSourceId (r.src), nullptr);
        c.setProperty ("dst", r.dstId, nullptr);
        mm.appendChild (c, nullptr);
    }
    root.appendChild (mm, nullptr);

    juce::ValueTree fo ("FXORDER");
    for (int r = 0; r < kNumRacks; ++r)
    {
        juce::StringArray s;
        for (auto u : rackOrders[(size_t) r]) s.add (juce::String (u));
        juce::ValueTree c ("RACK");
        c.setProperty ("index", r, nullptr);
        c.setProperty ("order", s.joinIntoString (","), nullptr);
        fo.appendChild (c, nullptr);
    }
    root.appendChild (fo, nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    return root;
}

void DeadwaveProcessor::applyState (const juce::ValueTree& root)
{
    if (! root.hasType ("DEADWAVE")) return;
    const juce::ScopedLock sl (stateLock);
    applyParameters (root);
    applyAssets (root);
    pendingState = {};
    ++stateSerial;
    sendChangeMessage();
}

void DeadwaveProcessor::applyParameters (const juce::ValueTree& root)
{
    const auto& reg = Registry::get();

    // Parameters: defaults first so absent IDs (compact storage / older versions) are deterministic.
    std::vector<float> vals ((size_t) P::Count);
    for (int i = 0; i < P::Count; ++i) vals[(size_t) i] = reg.spec (i).def;
    const auto ps = root.getChildWithName ("PARAMS");
    for (const auto& c : ps)
    {
        const int idx = reg.indexOf (c["id"].toString());
        if (idx >= 0) vals[(size_t) idx] = (float) c["value"];
    }
    for (int i = 0; i < P::Count; ++i)
    {
        auto* p = params[(size_t) i];
        const float norm = p->convertTo0to1 (juce::jlimit (reg.spec (i).min, reg.spec (i).max, vals[(size_t) i]));
        if (std::abs (p->getValue() - norm) > 1.0e-7f)
            p->setValueNotifyingHost (norm);
    }
}

void DeadwaveProcessor::applyAssets (const juce::ValueTree& root)
{
    const auto& reg = Registry::get();

    // Wavetables
    const auto w = root.getChildWithName ("WAVES");
    for (int i = 0; i < kNumOsc; ++i)
    {
        auto c = w.getChildWithProperty ("osc", i);
        if (c.isValid()) waves[i].fromValueTree (c);
        else waves[i].loadFactory (0);
        waveChanged (i);
    }

    // Samples
    const auto smp = root.getChildWithName ("SAMPLES");
    for (int i = 0; i < kNumOsc; ++i)
    {
        auto c = smp.getChildWithProperty ("osc", i);
        if (c.isValid()) setSample (i, stateio::sampleFromTree (c));
        else clearSample (i);
    }

    // LFO shapes
    const auto lfos = root.getChildWithName ("LFOSHAPES");
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto c = lfos.getChildWithProperty ("index", i);
        setLfoPoints (i, c.isValid() ? stateio::lfoPointsFromString (c["pts"].toString()) : stateio::defaultLfoPoints());
    }

    // Matrix
    for (int s = 0; s < kNumModSlots; ++s)
    {
        routings[(size_t) s] = {};
        syncRoutingToEngine (s);
    }
    for (const auto& c : root.getChildWithName ("MATRIX"))
    {
        const int s = (int) c["index"];
        if (s < 0 || s >= kNumModSlots) continue;
        const auto src = modSourceFromId (c["src"].toString());
        const auto dst = c["dst"].toString();
        if (reg.indexOf (dst) >= 0)
        {
            routings[(size_t) s] = { src, dst };
            syncRoutingToEngine (s);
        }
    }

    // FX order
    for (int r = 0; r < kNumRacks; ++r)
    {
        auto order = FxRack::defaultOrder();
        auto c = root.getChildWithName ("FXORDER").getChildWithProperty ("index", r);
        if (c.isValid())
        {
            const auto toks = juce::StringArray::fromTokens (c["order"].toString(), ",", "");
            if (toks.size() == FxRack::NumUnits)
            {
                std::array<int, FxRack::NumUnits> o {};
                bool seen[FxRack::NumUnits] {};
                bool ok = true;
                for (int k = 0; k < FxRack::NumUnits; ++k)
                {
                    o[(size_t) k] = toks[k].getIntValue();
                    if (o[(size_t) k] < 0 || o[(size_t) k] >= FxRack::NumUnits || seen[o[(size_t) k]]) { ok = false; break; }
                    seen[o[(size_t) k]] = true;
                }
                if (ok) order = o;
            }
        }
        setRackOrder (r, order);
    }

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid()) uiState = ui.createCopy();

    currentPresetName = root.getProperty ("preset", "Init").toString();
}

void DeadwaveProcessor::resetToInit()
{
    juce::ValueTree root ("DEADWAVE");
    root.setProperty ("preset", "Init", nullptr);
    applyState (root);
}

// ===================================================================================== documents
void DeadwaveProcessor::waveChanged (int osc)
{
    juce::ignoreUnused (osc);
    // Picked up by the timer (coalesces rapid edits while drawing).
}

void DeadwaveProcessor::rebuildDirtyTables()
{
    const juce::ScopedTryLock sl (stateLock);
    if (! sl.isLocked()) return; // a state restore is running; try again on the next tick
    for (int i = 0; i < kNumOsc; ++i)
    {
        const auto v = waves[i].getVersion();
        if (v == builtVersion[i]) continue;
        builtVersion[i] = v;
        builder.request (i, waves[i].getAllFrames(), waves[i].getName());
    }
}

std::shared_ptr<const SampleAsset> DeadwaveProcessor::getSample (int osc) const
{
    return sampleAssets[osc];
}

void DeadwaveProcessor::setSample (int osc, std::unique_ptr<SampleAsset> s)
{
    if (s == nullptr) { clearSample (osc); return; }
    std::shared_ptr<const SampleAsset> shared (std::move (s));
    sampleAssets[osc] = shared;
    engine.setSample (osc, std::make_unique<SampleAsset> (*shared));
    sendChangeMessage();
}

void DeadwaveProcessor::clearSample (int osc)
{
    sampleAssets[osc] = nullptr;
    engine.setSample (osc, nullptr);
}

void DeadwaveProcessor::setLfoPoints (int lfo, const stateio::LfoPoints& pts)
{
    lfoPoints[lfo] = pts;
    engine.setLfoShape (lfo, stateio::renderLfo (pts));
}

// ==================================================================================== modulation
void DeadwaveProcessor::syncRoutingToEngine (int slot)
{
    const auto& r = routings[(size_t) slot];
    engine.setModSlot (slot, r.isUsed() ? r.src : ModSrc::None, r.isUsed() ? Registry::get().indexOf (r.dstId) : -1);
}

int DeadwaveProcessor::addModulation (ModSrc src, const juce::String& dstId, float amount)
{
    const auto& reg = Registry::get();
    const int idx = reg.indexOf (dstId);
    if (idx < 0 || ! reg.spec (idx).modulatable || src == ModSrc::None) return -1;

    int slot = -1;
    for (int s = 0; s < kNumModSlots && slot < 0; ++s)
        if (routings[(size_t) s].src == src && routings[(size_t) s].dstId == dstId) slot = s;
    for (int s = 0; s < kNumModSlots && slot < 0; ++s)
        if (! routings[(size_t) s].isUsed()) slot = s;
    if (slot < 0) return -1;

    setValue (P::modAmt (slot), amount);
    setRouting (slot, src, dstId);
    return slot;
}

void DeadwaveProcessor::setRouting (int slot, ModSrc src, const juce::String& dstId)
{
    routings[(size_t) slot] = { src, dstId };
    syncRoutingToEngine (slot);
    sendChangeMessage();
}

void DeadwaveProcessor::clearRouting (int slot)
{
    routings[(size_t) slot] = {};
    setValue (P::modAmt (slot), 0.0f);
    syncRoutingToEngine (slot);
    sendChangeMessage();
}

juce::Array<int> DeadwaveProcessor::getRoutingsForParam (const juce::String& dstId) const
{
    juce::Array<int> r;
    for (int s = 0; s < kNumModSlots; ++s)
        if (routings[(size_t) s].isUsed() && routings[(size_t) s].dstId == dstId) r.add (s);
    return r;
}

void DeadwaveProcessor::setValue (int index, float realValue)
{
    auto* p = params[(size_t) index];
    const float norm = p->convertTo0to1 (realValue);
    if (std::abs (p->getValue() - norm) < 1.0e-7f) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (norm);
    p->endChangeGesture();
}

void DeadwaveProcessor::setRackOrder (int rack, const std::array<int, FxRack::NumUnits>& order)
{
    rackOrders[(size_t) rack] = order;
    engine.setRackOrder (rack, order);
}

void DeadwaveProcessor::timerCallback()
{
    rebuildDirtyTables();
    engine.collectGarbage();
}

// =================================================================================== table builder
void DeadwaveProcessor::TableBuilder::request (int osc, wavegen::Frames frames, juce::String name)
{
    {
        const juce::ScopedLock sl (lock);
        jobs[osc].pending = true;
        jobs[osc].frames = std::move (frames);
        jobs[osc].name = std::move (name);
    }
    wake.signal();
}

void DeadwaveProcessor::TableBuilder::run()
{
    while (! threadShouldExit())
    {
        wake.wait (200);
        for (int i = 0; i < kNumOsc && ! threadShouldExit(); ++i)
        {
            wavegen::Frames frames;
            juce::String name;
            {
                const juce::ScopedLock sl (lock);
                if (! jobs[i].pending) continue;
                jobs[i].pending = false;
                frames = std::move (jobs[i].frames);
                name = jobs[i].name;
            }
            auto table = Wavetable::fromFrames (frames, name);
            owner.engine.setWavetable (i, std::move (table));
        }
    }
}
} // namespace dw

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dw::DeadwaveProcessor();
}
