#pragma once

#include "../Core/Params.h"
#include <juce_data_structures/juce_data_structures.h>
#include <set>

namespace dw
{
/**
    Local, deterministic sound designer.

    It is NOT a machine-learning model: text is matched against a sound-design lexicon to derive
    musical traits, and a rule-based designer turns traits into a complete preset (oscillators,
    wavetables, filters, envelopes, modulation, effects, voice settings and macros).
*/
struct GenTraits
{
    juce::String category = "Lead";
    float brightness = 0.5f;  // dark .. bright
    float movement = 0.4f;    // static .. evolving
    float aggression = 0.3f;  // clean .. distorted
    float width = 0.5f;       // mono .. wide
    float complexity = 0.5f;  // simple .. dense
    float attack = 0.1f;      // instant .. slow swell
    float length = 0.5f;      // short .. long tails
    float warmth = 0.4f;      // digital .. analog
    float space = 0.3f;       // dry .. huge space
    bool glide = false;
    bool acid = false;
    bool chip = false;
    bool vocal = false;
    bool metallic = false;
};

class SoundGenerator
{
public:
    /** Parses free text. understood receives the recognised words with their effect. */
    static GenTraits parseText (const juce::String& text, juce::StringArray& understood);

    /** Builds a complete DEADWAVE state tree from traits. */
    static juce::ValueTree generate (const GenTraits& t, uint32_t seed, const juce::String& name);

    /**
        Randomises a state. full = new sound from scratch for unlocked groups; otherwise parameters
        are nudged by intensity (0..1). Locked groups are copied unchanged.
    */
    static juce::ValueTree randomize (const juce::ValueTree& current, float intensity, const std::set<Group>& locked,
                                      uint32_t seed, bool full);

    static const juce::StringArray& lockableGroups();          // UI names
    static std::vector<Group> groupsFor (const juce::String& lockName);
};
} // namespace dw
