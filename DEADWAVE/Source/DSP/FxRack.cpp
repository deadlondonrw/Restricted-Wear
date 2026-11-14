#include "FxRack.h"

namespace dw
{
FxRack::FxRack()
{
    setOrder (defaultOrder());
}

const juce::StringArray& FxRack::unitNames()
{
    static const juce::StringArray n { "DISTORTION", "SATURATION", "CHORUS", "PHASER", "FLANGER", "DELAY", "REVERB",
                                       "COMPRESSOR", "EQ", "IMAGER", "BITCRUSHER" };
    return n;
}

int FxRack::unitOnParam (int unit)
{
    static const int on[NumUnits] = { FxP::DistOn, FxP::SatOn, FxP::ChoOn, FxP::PhaOn, FxP::FlaOn, FxP::DlyOn,
                                      FxP::RevOn, FxP::CmpOn, FxP::EqOn, FxP::ImgOn, FxP::CrsOn };
    return on[juce::jlimit (0, NumUnits - 1, unit)];
}

std::array<int, FxRack::NumUnits> FxRack::defaultOrder()
{
    return { Distortion, Saturation, Bitcrusher, Compressor, Eq, Chorus, Flanger, Phaser, Delay, Reverb, Imager };
}

void FxRack::prepare (double sampleRate)
{
    dist.prepare (sampleRate);
    sat.prepare (sampleRate);
    chorus.prepare (sampleRate);
    phaser.prepare (sampleRate);
    flanger.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    comp.prepare (sampleRate);
    eq.prepare (sampleRate);
    imager.prepare (sampleRate);
    crusher.prepare (sampleRate);
    for (auto& w : wasOn) w = false;
}

void FxRack::reset()
{
    dist.reset(); sat.reset(); chorus.reset(); phaser.reset(); flanger.reset(); delay.reset();
    reverb.reset(); comp.reset(); eq.reset(); imager.reset(); crusher.reset();
    for (auto& w : wasOn) w = false;
}

bool FxRack::anyUnitOn (const float* v, int paramRack) const noexcept
{
    if (v[P::fx (paramRack, FxP::RackOn)] < 0.5f) return false;
    for (int u = 0; u < NumUnits; ++u)
        if (v[P::fx (paramRack, unitOnParam (u))] >= 0.5f) return true;
    return false;
}

void FxRack::process (float* l, float* r, int n, const float* v, int pr, double bpm) noexcept
{
    if (v[P::fx (pr, FxP::RackOn)] < 0.5f)
        return;

    auto p = [v, pr] (int idx) { return v[P::fx (pr, idx)]; };
    const auto ord = getOrder();
    bool seen[NumUnits] {};

    for (int k = 0; k < NumUnits; ++k)
    {
        const int unit = juce::jlimit (0, NumUnits - 1, ord[(size_t) k]);
        if (seen[unit]) continue; // guard against malformed orders
        seen[unit] = true;

        const bool on = p (unitOnParam (unit)) >= 0.5f;
        if (! on)
        {
            if (wasOn[unit])
            {
                // Clear state so re-enabling never replays stale tails.
                switch (unit)
                {
                    case Distortion: dist.reset(); break;
                    case Saturation: sat.reset(); break;
                    case Chorus: chorus.reset(); break;
                    case Phaser: phaser.reset(); break;
                    case Flanger: flanger.reset(); break;
                    case Delay: delay.reset(); break;
                    case Reverb: reverb.reset(); break;
                    case Compressor: comp.reset(); break;
                    case Eq: eq.reset(); break;
                    case Imager: imager.reset(); break;
                    case Bitcrusher: crusher.reset(); break;
                    default: break;
                }
                wasOn[unit] = false;
            }
            continue;
        }
        wasOn[unit] = true;

        switch (unit)
        {
            case Distortion:
                dist.process (l, r, n, (int) p (FxP::DistMode), p (FxP::DistDrive), p (FxP::DistTone), p (FxP::DistMix));
                break;
            case Saturation:
                sat.process (l, r, n, p (FxP::SatDrive), p (FxP::SatBias), p (FxP::SatTone), p (FxP::SatMix));
                break;
            case Chorus:
                chorus.process (l, r, n, p (FxP::ChoRate), p (FxP::ChoDepth), p (FxP::ChoDelay), p (FxP::ChoFeedback),
                                p (FxP::ChoWidth), p (FxP::ChoMix));
                break;
            case Phaser:
                phaser.process (l, r, n, p (FxP::PhaRate), p (FxP::PhaDepth), p (FxP::PhaFreq), p (FxP::PhaFeedback),
                                (int) p (FxP::PhaStages), p (FxP::PhaMix));
                break;
            case Flanger:
                flanger.process (l, r, n, p (FxP::FlaRate), p (FxP::FlaDepth), p (FxP::FlaDelay), p (FxP::FlaFeedback), p (FxP::FlaMix));
                break;
            case Delay:
            {
                float t = p (FxP::DlyTime);
                if (p (FxP::DlySync) >= 0.5f)
                    t = (float) (60000.0 / juce::jmax (20.0, bpm) * Choices::divisionToBeats ((int) p (FxP::DlyDiv)));
                t = juce::jlimit (1.0f, 4000.0f, t);
                const float s = p (FxP::DlyStereo);
                delay.process (l, r, n, t * (1.0f - 0.25f * s), t * (1.0f + 0.25f * s), p (FxP::DlyFeedback),
                               p (FxP::DlyPingPong) >= 0.5f, p (FxP::DlyLowCut), p (FxP::DlyHighCut), p (FxP::DlyMix));
                break;
            }
            case Reverb:
                reverb.process (l, r, n, p (FxP::RevSize), p (FxP::RevDecay), p (FxP::RevDamp), p (FxP::RevPredelay),
                                p (FxP::RevWidth), p (FxP::RevMod), p (FxP::RevMix));
                break;
            case Compressor:
                comp.process (l, r, n, p (FxP::CmpThresh), p (FxP::CmpRatio), p (FxP::CmpAttack), p (FxP::CmpRelease),
                              p (FxP::CmpKnee), p (FxP::CmpMakeup), p (FxP::CmpMix));
                break;
            case Eq:
                eq.process (l, r, n, p (FxP::EqLowCut), p (FxP::EqLowFreq), p (FxP::EqLowGain), p (FxP::EqMidFreq),
                            p (FxP::EqMidGain), p (FxP::EqMidQ), p (FxP::EqHighFreq), p (FxP::EqHighGain), p (FxP::EqHighCut));
                break;
            case Imager:
                imager.process (l, r, n, p (FxP::ImgWidth), p (FxP::ImgBassMono), p (FxP::ImgPan));
                break;
            case Bitcrusher:
                crusher.process (l, r, n, p (FxP::CrsBits), p (FxP::CrsRate), p (FxP::CrsMix));
                break;
            default:
                break;
        }
    }
}
} // namespace dw
