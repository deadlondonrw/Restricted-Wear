#include "Voice.h"

namespace dw
{
void Voice::prepare (double baseRate, int maxOversample)
{
    const double maxRate = baseRate * juce::jmax (1, maxOversample);
    const int combSize = (int) std::ceil (maxRate / 25.0) + 8;
    for (auto& f : filter) f.prepare (maxRate, combSize);
    for (auto& lane : laneFilter)
        for (auto& f : lane) f.prepare (maxRate, combSize);
    processRate = baseRate;
    for (auto& o : osc) o.prepare (baseRate);
    for (auto& f : filter) f.setSampleRate (baseRate);
    for (auto& lane : laneFilter)
        for (auto& f : lane) f.setSampleRate (baseRate);
    for (auto& d : dcMain) d.prepare (baseRate, 6.0f);
    for (auto& lane : dcLane)
        for (auto& d : lane) d.prepare (baseRate, 6.0f);
    active = false;
}

void Voice::start (int n, float vel, float glideFromPitch, uint32_t a, uint32_t seed, const EngineContext& ctx)
{
    active = true;
    released = false;
    fadeOut = false;
    fadeGain = 1.0f;
    firstTick = true;
    note = n;
    velocity = vel;
    age = a;
    keyHeld = true;
    sustained = false;
    polyPressure = 0.0f;
    rng.seed (seed);
    randomValue = rng.bi();

    if (glideFromPitch >= 0.0f && std::abs (glideFromPitch - (float) n) > 0.001f)
    {
        glideFrom = glideFromPitch;
        glideTo = (float) n;
        glidePos = 0.0f;
        gliding = true;
        pitch = glideFrom;
    }
    else
    {
        pitch = glideFrom = glideTo = (float) n;
        glidePos = 1.0f;
        gliding = false;
    }

    for (auto& e : env) e.trigger (true);
    for (int i = 0; i < kNumLfos; ++i)
        lfo[i].reset (ctx.lfo[i], seed + 977u * (uint32_t) (i + 1));

    for (int i = 0; i < kNumOsc; ++i)
    {
        instab[i] = rng.bi();
        driftTarget[i] = rng.bi();
    }

    for (auto& f : filter) f.reset();
    for (auto& lane : laneFilter)
        for (auto& f : lane) f.reset();
    for (auto& d : dcMain) d.reset();
    for (auto& lane : dcLane)
        for (auto& d : lane) d.reset();
}

void Voice::legatoTo (int n, float vel, bool glide, bool retrigger, bool hardRetrigger, uint32_t a)
{
    note = n;
    velocity = vel;
    age = a;
    keyHeld = true;
    sustained = false;
    released = false;
    fadeOut = false;
    fadeGain = 1.0f;

    if (glide && std::abs (pitch - (float) n) > 0.001f)
    {
        glideFrom = pitch;
        glideTo = (float) n;
        glidePos = 0.0f;
        gliding = true;
    }
    else
    {
        pitch = glideFrom = glideTo = (float) n;
        glidePos = 1.0f;
        gliding = false;
    }

    if (retrigger)
        for (auto& e : env) e.trigger (hardRetrigger);
    else
        for (auto& e : env)
            if (e.isReleasing() || ! e.isActive()) e.trigger (false);
}

void Voice::release() noexcept
{
    if (! active) return;
    released = true;
    keyHeld = false;
    for (auto& e : env) e.release();
}

void Voice::kill() noexcept
{
    active = false;
    released = false;
    fadeOut = false;
    for (auto& e : env) e.kill();
    note = -1;
}

void Voice::computeModulation (const EngineContext& ctx) noexcept
{
    for (int i = 0; i < kNumEnvs; ++i)
        srcVal[(int) ModSrc::Env1 + i] = env[i].getValue();
    for (int i = 0; i < kNumLfos; ++i)
        srcVal[(int) ModSrc::Lfo1 + i] = ctx.lfo[i].mode == 0 ? ctx.globalLfo[i] : lfo[i].getValue();
    srcVal[(int) ModSrc::Velocity] = velocity;
    srcVal[(int) ModSrc::Note] = juce::jlimit (-1.0f, 1.0f, ((float) note - 60.0f) / 60.0f);
    srcVal[(int) ModSrc::ModWheel] = ctx.modWheel;
    srcVal[(int) ModSrc::Aftertouch] = juce::jmax (ctx.channelPressure, polyPressure);
    srcVal[(int) ModSrc::PitchBend] = ctx.pitchBendNorm;
    srcVal[(int) ModSrc::Random] = randomValue;
    for (int i = 0; i < kNumMacros; ++i)
        srcVal[(int) ModSrc::Macro1 + i] = ctx.macro[i];

    if (ctx.numVoiceDests == 0) return;

    for (int d = 0; d < ctx.numVoiceDests; ++d)
        modAccum[ctx.voiceDests[d]] = 0.0f;

    for (int s = 0; s < ctx.numSlots; ++s)
    {
        const auto& slot = ctx.slots[s];
        if (slot.dst < 0 || ! ctx.modActive[slot.dst]) continue;
        modAccum[slot.dst] += srcVal[slot.src] * ctx.base[slot.amtIdx];
    }

    for (int d = 0; d < ctx.numVoiceDests; ++d)
    {
        const int idx = ctx.voiceDests[d];
        const float norm = juce::jlimit (0.0f, 1.0f, ctx.baseNorm[idx] + modAccum[idx]);
        modVal[idx] = ctx.ranges[idx].convertFrom0to1 (norm);
    }
}

void Voice::buildTicks (const EngineContext& ctx, float dt) noexcept
{
    const float* b = ctx.base;

    // ---- envelopes ---------------------------------------------------------------------------
    for (int i = 0; i < kNumEnvs; ++i)
    {
        EnvSettings s = ctx.envBase[i];
        s.delay = param (ctx, P::env (i, EnvP::Delay));
        s.attack = param (ctx, P::env (i, EnvP::Attack));
        s.hold = param (ctx, P::env (i, EnvP::Hold));
        s.decay = param (ctx, P::env (i, EnvP::Decay));
        s.sustain = param (ctx, P::env (i, EnvP::Sustain));
        s.release = param (ctx, P::env (i, EnvP::Release));
        env[i].advance (s, dt);
    }

    // ---- LFOs --------------------------------------------------------------------------------
    for (int i = 0; i < kNumLfos; ++i)
    {
        if (ctx.lfo[i].mode == 0) continue; // free-running: the engine's global instance is used
        LfoSettings s = ctx.lfo[i];
        if (b[P::lfo (i, LfoP::Sync)] < 0.5f)
            s.rateHz = param (ctx, P::lfo (i, LfoP::Rate));
        s.phase = param (ctx, P::lfo (i, LfoP::Phase));
        s.smooth = param (ctx, P::lfo (i, LfoP::Smooth));
        lfo[i].advance (s, dt);
    }

    // ---- glide -------------------------------------------------------------------------------
    if (gliding)
    {
        const float time = param (ctx, P::voice (VoiceP::GlideTime));
        const bool constRate = b[P::voice (VoiceP::GlideConstRate)] >= 0.5f;
        const float dist = std::abs (glideTo - glideFrom);
        const float dur = constRate ? time * dist / 12.0f : time;
        if (dur <= 1.0e-4f)
            glidePos = 1.0f;
        else
            glidePos += dt / dur;
        if (glidePos >= 1.0f)
        {
            glidePos = 1.0f;
            gliding = false;
        }
        const float curve = b[P::voice (VoiceP::GlideCurve)];
        pitch = glideFrom + (glideTo - glideFrom) * Envelope::curve (glidePos, curve);
    }
    else
        pitch = glideTo;

    // ---- analog drift / instability ------------------------------------------------------------
    const float driftAmt = param (ctx, P::analog (AnalogP::Drift));
    const float instabAmt = param (ctx, P::analog (AnalogP::Instability));
    driftTimer += dt;
    if (driftTimer > 0.35f)
    {
        driftTimer = 0.0f;
        for (auto& t : driftTarget) t = rng.bi();
    }
    const float driftCoef = 1.0f - std::exp (-dt / 0.6f);
    for (int i = 0; i < kNumOsc; ++i)
        drift[i] += (driftTarget[i] - drift[i]) * driftCoef;

    // ---- oscillators ---------------------------------------------------------------------------
    const float stereoVar = b[P::analog (AnalogP::StereoVar)];
    for (int i = 0; i < kNumOsc; ++i)
    {
        auto& t = curOsc[i];
        t.type = (int) b[P::osc (i, OscP::Type)];
        t.level = param (ctx, P::osc (i, OscP::Level));
        t.pan = param (ctx, P::osc (i, OscP::Pan));
        const bool keyTrack = b[P::osc (i, OscP::KeyTrack)] >= 0.5f;
        float p = keyTrack ? pitch : 60.0f;
        p += b[P::osc (i, OscP::Octave)] * 12.0f + b[P::osc (i, OscP::Semi)] + param (ctx, P::osc (i, OscP::Fine)) * 0.01f
           + param (ctx, P::osc (i, OscP::Pitch));
        p += ctx.pitchBendSemis;
        p += driftAmt * 0.15f * drift[i] + instabAmt * 0.08f * instab[i];
        t.freq = juce::jlimit (0.5f, (float) ctx.processRate * 0.45f, midiToHz (p));
        t.wtPos = param (ctx, P::osc (i, OscP::WtPos));
        t.bias = param (ctx, P::osc (i, OscP::Bias));
        t.amp = param (ctx, P::osc (i, OscP::Amp));
        t.phase = param (ctx, P::osc (i, OscP::Phase));
        t.shape = param (ctx, P::osc (i, OscP::Shape));
        t.warp = (int) b[P::osc (i, OscP::WarpMode)];
        t.unison = (int) b[P::osc (i, OscP::Unison)];
        t.detune = param (ctx, P::osc (i, OscP::Detune));
        t.spread = param (ctx, P::osc (i, OscP::Spread));
        t.blend = param (ctx, P::osc (i, OscP::Blend));
        t.fmSrc = (int) b[P::osc (i, OscP::FmSrc)];
        t.fmAmt = param (ctx, P::osc (i, OscP::FmAmt));
        t.fmRatio = param (ctx, P::osc (i, OscP::FmRatio));
        t.drive = param (ctx, P::osc (i, OscP::Drive));
        t.driveMode = (int) b[P::osc (i, OscP::DriveMode)];
        t.crush = param (ctx, P::osc (i, OscP::Crush));
        t.dcRemove = b[P::osc (i, OscP::DcRemove)] >= 0.5f;
        t.stereoVar = stereoVar;
    }

    // ---- filters ------------------------------------------------------------------------------
    const float nonlin = param (ctx, P::analog (AnalogP::FilterNonlin));
    const float env2 = env[1].getValue();
    for (int f = 0; f < kNumFilters; ++f)
    {
        auto& t = curFilt[f];
        t.type = (int) b[P::filt (f, FiltP::Type)];
        const float envAmt = param (ctx, P::filt (f, FiltP::EnvAmt));
        const float key = param (ctx, P::filt (f, FiltP::KeyTrack));
        const float octaves = envAmt * 8.0f * env2 + key * (pitch - 60.0f) / 12.0f;
        t.cutoff = juce::jlimit (16.0f, (float) ctx.processRate * 0.47f, param (ctx, P::filt (f, FiltP::Cutoff)) * std::exp2 (octaves));
        t.reso = param (ctx, P::filt (f, FiltP::Reso));
        t.drive = param (ctx, P::filt (f, FiltP::Drive));
        t.mix = param (ctx, P::filt (f, FiltP::Mix));
        t.spread = param (ctx, P::filt (f, FiltP::Spread));
        t.nonlin = nonlin;
    }

    // ---- amplitude ----------------------------------------------------------------------------
    const float velSens = b[P::voice (VoiceP::VelSens)];
    const float velGain = 1.0f - velSens + velSens * std::pow (velocity, 1.5f);
    if (fadeOut)
        fadeGain = juce::jmax (0.0f, fadeGain - dt / 0.004f);
    curAmp = env[0].getValue() * velGain * fadeGain;
}

void Voice::render (const EngineContext& ctx, float* mainL, float* mainR, float* const* laneL, float* const* laneR,
                    bool* laneUsed, int n) noexcept
{
    if (! active) return;
    jassert (n <= kBuf);

    if (ctx.processRate != processRate)
    {
        processRate = ctx.processRate;
        for (auto& o : osc) o.prepare (processRate);
        for (auto& f : filter) f.setSampleRate (processRate);
        for (auto& lane : laneFilter)
            for (auto& f : lane) f.setSampleRate (processRate);
        for (auto& d : dcMain) d.prepare (processRate, 6.0f);
        for (auto& lane : dcLane)
            for (auto& d : lane) d.prepare (processRate, 6.0f);
    }

    const float dt = (float) n / (float) processRate;

    if (firstTick)
    {
        computeModulation (ctx);
        buildTicks (ctx, 0.0f);
        for (int i = 0; i < kNumOsc; ++i)
        {
            const double start = curOsc[i].type == 2 && ctx.samples[i] != nullptr
                                   ? (double) curOsc[i].wtPos * (double) ctx.samples[i]->length
                                   : 0.0;
            osc[i].noteOn (curOsc[i], ctx.base[P::osc (i, OscP::PhaseRand)], rng, true, start);
            prevOsc[i] = curOsc[i];
        }
        for (int f = 0; f < kNumFilters; ++f) prevFilt[f] = curFilt[f];
        prevAmp = 0.0f;
        firstTick = false;
    }
    else
    {
        for (int i = 0; i < kNumOsc; ++i) prevOsc[i] = curOsc[i];
        for (int f = 0; f < kNumFilters; ++f) prevFilt[f] = curFilt[f];
        prevAmp = curAmp;
    }

    computeModulation (ctx);
    buildTicks (ctx, dt);

    // ---- oscillators ---------------------------------------------------------------------------
    bool oscOn[kNumOsc];
    for (int i = 0; i < kNumOsc; ++i)
    {
        oscOn[i] = ctx.base[P::osc (i, OscP::On)] >= 0.5f;
        std::fill (oscL[i], oscL[i] + n, 0.0f);
        std::fill (oscR[i], oscR[i] + n, 0.0f);
        if (! oscOn[i])
        {
            std::fill (oscMono[i], oscMono[i] + n, 0.0f);
            continue;
        }
        const int fmSrc = curOsc[i].fmSrc;
        const float* fmIn = nullptr;
        if (fmSrc >= 2)
        {
            const int k = fmSrc - 2;
            if (k < i && oscOn[k]) fmIn = oscMono[k];
        }
        osc[i].render (ctx.tables[i], ctx.samples[i], prevOsc[i], curOsc[i], fmIn, oscL[i], oscR[i], oscMono[i], n, rng);
    }

    // ---- routing + filters ---------------------------------------------------------------------
    const bool serial = ctx.base[P::global (GlobalP::FilterRouting)] < 0.5f;
    const bool fOn[2] = { ctx.base[P::filt (0, FiltP::On)] >= 0.5f, ctx.base[P::filt (1, FiltP::On)] >= 0.5f };

    auto runFilters = [&] (VoiceFilter* filters, float* aL, float* aR, float* bL, float* bR)
    {
        if (serial)
        {
            if (fOn[0]) filters[0].process (prevFilt[0], curFilt[0], aL, aR, n);
            for (int s = 0; s < n; ++s) { aL[s] += bL[s]; aR[s] += bR[s]; }
            if (fOn[1]) filters[1].process (prevFilt[1], curFilt[1], aL, aR, n);
        }
        else
        {
            if (fOn[0]) filters[0].process (prevFilt[0], curFilt[0], aL, aR, n);
            if (fOn[1]) filters[1].process (prevFilt[1], curFilt[1], bL, bR, n);
            for (int s = 0; s < n; ++s) { aL[s] += bL[s]; aR[s] += bR[s]; }
        }
    };

    auto routeInto = [&] (int i, float* aL, float* aR, float* bL, float* bR, float* dL, float* dR)
    {
        const int route = (int) ctx.base[P::osc (i, OscP::Route)];
        const float* sl = oscL[i];
        const float* sr = oscR[i];
        if (route == 3)
            for (int s = 0; s < n; ++s) { dL[s] += sl[s]; dR[s] += sr[s]; }
        else if (route == 0 || (route == 2 && serial))
            for (int s = 0; s < n; ++s) { aL[s] += sl[s]; aR[s] += sr[s]; }
        else if (route == 1)
            for (int s = 0; s < n; ++s) { bL[s] += sl[s]; bR[s] += sr[s]; }
        else // parallel, both filters
            for (int s = 0; s < n; ++s)
            {
                aL[s] += 0.5f * sl[s]; aR[s] += 0.5f * sr[s];
                bL[s] += 0.5f * sl[s]; bR[s] += 0.5f * sr[s];
            }
    };

    const float sat = param (ctx, P::analog (AnalogP::Saturation));
    const float satG = 1.0f + sat * 5.0f;
    const float satNorm = 1.0f / fastTanh (satG);
    const float invN = 1.0f / (float) n;
    const float dAmp = (curAmp - prevAmp) * invN;

    auto finishInto = [&] (float* srcL, float* srcR, DcBlocker* dc, float* dstL, float* dstR)
    {
        float a = prevAmp;
        for (int s = 0; s < n; ++s)
        {
            float l = srcL[s] * a, r = srcR[s] * a;
            if (sat > 0.001f)
            {
                l = fastTanh (l * satG) * satNorm;
                r = fastTanh (r * satG) * satNorm;
            }
            dstL[s] += dc[0].process (l);
            dstR[s] += dc[1].process (r);
            a += dAmp;
        }
    };

    std::fill (bufAL, bufAL + n, 0.0f); std::fill (bufAR, bufAR + n, 0.0f);
    std::fill (bufBL, bufBL + n, 0.0f); std::fill (bufBR, bufBR + n, 0.0f);
    std::fill (bufDL, bufDL + n, 0.0f); std::fill (bufDR, bufDR + n, 0.0f);

    bool anyMain = false;
    for (int i = 0; i < kNumOsc; ++i)
    {
        if (! oscOn[i]) continue;
        if (ctx.base[P::osc (i, OscP::FxOn)] >= 0.5f && laneL != nullptr)
            continue;
        routeInto (i, bufAL, bufAR, bufBL, bufBR, bufDL, bufDR);
        anyMain = true;
    }
    if (anyMain)
    {
        runFilters (filter, bufAL, bufAR, bufBL, bufBR);
        for (int s = 0; s < n; ++s) { bufAL[s] += bufDL[s]; bufAR[s] += bufDR[s]; }
        finishInto (bufAL, bufAR, dcMain, mainL, mainR);
    }

    // ---- per-oscillator FX lanes ---------------------------------------------------------------
    if (laneL != nullptr)
    {
        for (int i = 0; i < kNumOsc; ++i)
        {
            if (! oscOn[i] || ctx.base[P::osc (i, OscP::FxOn)] < 0.5f) continue;
            std::fill (bufAL, bufAL + n, 0.0f); std::fill (bufAR, bufAR + n, 0.0f);
            std::fill (bufBL, bufBL + n, 0.0f); std::fill (bufBR, bufBR + n, 0.0f);
            std::fill (bufDL, bufDL + n, 0.0f); std::fill (bufDR, bufDR + n, 0.0f);
            routeInto (i, bufAL, bufAR, bufBL, bufBR, bufDL, bufDR);
            runFilters (laneFilter[i], bufAL, bufAR, bufBL, bufBR);
            for (int s = 0; s < n; ++s) { bufAL[s] += bufDL[s]; bufAR[s] += bufDR[s]; }
            finishInto (bufAL, bufAR, dcLane[i], laneL[i], laneR[i]);
            laneUsed[i] = true;
        }
    }

    if (! env[0].isActive() || (fadeOut && fadeGain <= 0.0f))
        kill();
}
} // namespace dw
