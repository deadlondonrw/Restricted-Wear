#include "SampleImport.h"

namespace dw::sampleimport
{
namespace
{
float mono (const Audio& a, int i)
{
    if (i < 0 || i >= a.length()) return 0.0f;
    float s = a.buffer.getSample (0, i);
    if (a.buffer.getNumChannels() > 1) s = 0.5f * (s + a.buffer.getSample (1, i));
    return s;
}

float readCubic (const Audio& a, double pos)
{
    const int i = (int) std::floor (pos);
    const float t = (float) (pos - i);
    return hermite (mono (a, i - 1), mono (a, i), mono (a, i + 1), mono (a, i + 2), t);
}

/** Resamples a span [from, from + len) of the source to kWaveSize samples. */
wavegen::Frame resampleSpan (const Audio& a, double from, double len)
{
    wavegen::Frame f ((size_t) kWaveSize);
    for (int i = 0; i < kWaveSize; ++i)
        f[(size_t) i] = readCubic (a, from + len * (double) i / (double) kWaveSize);
    return f;
}
} // namespace

bool load (const juce::File& file, Audio& out, juce::String& error, double maxSeconds)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported or unreadable audio file."; return false; }
    if (reader->sampleRate <= 0.0) { error = "Invalid sample rate."; return false; }
    const auto maxLen = (juce::int64) (maxSeconds * reader->sampleRate);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len < 64) { error = "The file is too short."; return false; }
    const int ch = juce::jlimit (1, 2, (int) reader->numChannels);
    out.buffer.setSize (ch, len);
    reader->read (&out.buffer, 0, len, 0, true, ch > 1);
    out.rate = reader->sampleRate;
    out.name = file.getFileNameWithoutExtension();
    return true;
}

float detectPeriod (const Audio& a, int start, int length)
{
    const int minP = juce::jmax (2, (int) (a.rate / 2000.0));
    const int maxP = juce::jmin ((int) (a.rate / 30.0), length / 2);
    if (maxP <= minP + 2) return 0.0f;
    const int W = juce::jmin (length - maxP, (int) (a.rate * 0.06));
    if (W < minP * 2) return 0.0f;

    std::vector<float> d ((size_t) maxP + 1, 0.0f);
    for (int tau = 1; tau <= maxP; ++tau)
    {
        double acc = 0.0;
        for (int i = 0; i < W; i += 2)
        {
            const float diff = mono (a, start + i) - mono (a, start + i + tau);
            acc += diff * diff;
        }
        d[(size_t) tau] = (float) acc;
    }
    // cumulative mean normalised difference
    std::vector<float> cm ((size_t) maxP + 1, 1.0f);
    double running = 0.0;
    for (int tau = 1; tau <= maxP; ++tau)
    {
        running += d[(size_t) tau];
        cm[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = minP; tau < maxP; ++tau)
        if (cm[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < maxP && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        float m = 1e9f;
        for (int tau = minP; tau < maxP; ++tau)
            if (cm[(size_t) tau] < m) { m = cm[(size_t) tau]; best = tau; }
        if (m > 0.4f) return 0.0f; // unpitched
    }
    // parabolic refinement
    const float a0 = cm[(size_t) best - 1], b0 = cm[(size_t) best], c0 = cm[(size_t) juce::jmin (maxP, best + 1)];
    const float denom = a0 - 2.0f * b0 + c0;
    const float off = std::abs (denom) > 1e-9f ? 0.5f * (a0 - c0) / denom : 0.0f;
    return (float) best + juce::jlimit (-0.5f, 0.5f, off);
}

float periodToNote (float period, double rate)
{
    if (period <= 0.0f) return 60.0f;
    const double hz = rate / period;
    return (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
}

wavegen::Frames extractCycles (const Audio& a, int start, int end, int numFrames, float period)
{
    start = juce::jlimit (0, a.length() - 1, start);
    end = juce::jlimit (start + 1, a.length(), end);
    numFrames = juce::jlimit (1, kMaxFrames, numFrames);
    if (period <= 0.0f) period = detectPeriod (a, start, end - start);
    if (period <= 2.0f) period = (float) (a.rate / 110.0); // fallback for unpitched material

    wavegen::Frames frames;
    const double span = juce::jmax (0.0, (double) (end - start) - period * 2.0);
    for (int f = 0; f < numFrames; ++f)
    {
        double pos = start + (numFrames > 1 ? span * f / (numFrames - 1) : 0.0);
        // align to the nearest upward zero crossing within one period
        for (int k = 0; k < (int) period; ++k)
        {
            const int i = (int) pos + k;
            if (mono (a, i - 1) <= 0.0f && mono (a, i) > 0.0f)
            {
                const float x0 = mono (a, i - 1), x1 = mono (a, i);
                pos = (double) (i - 1) + (double) (-x0 / (x1 - x0 + 1e-12f));
                break;
            }
        }
        // refine the local period so consecutive frames stay phase-coherent
        const float local = detectPeriod (a, (int) pos, juce::jmin (end - (int) pos, (int) (period * 6.0f)));
        const float p = (local > period * 0.8f && local < period * 1.25f) ? local : period;
        auto fr = resampleSpan (a, pos, (double) p);
        // remove the seam: blend the last few percent toward the start value
        const int fade = kWaveSize / 32;
        for (int i = 0; i < fade; ++i)
        {
            const float t = (float) i / fade;
            fr[(size_t) (kWaveSize - fade + i)] = fr[(size_t) (kWaveSize - fade + i)] * (1.0f - t) + fr[(size_t) i] * t * 0.0f
                                                + fr[0] * t;
        }
        frames.push_back (std::move (fr));
    }
    for (auto& f : frames) wavegen::removeDc (f);
    wavegen::normaliseTable (frames, 1.0f);
    return frames;
}

wavegen::Frames sliceFixed (const Audio& a, int start, int end, int numFrames)
{
    start = juce::jlimit (0, a.length() - 1, start);
    end = juce::jlimit (start + 1, a.length(), end);
    numFrames = juce::jlimit (1, kMaxFrames, numFrames);
    const double len = (double) (end - start) / numFrames;
    wavegen::Frames frames;
    for (int f = 0; f < numFrames; ++f)
    {
        auto fr = resampleSpan (a, start + len * f, len);
        // window edges to keep the cycle seamless
        const int fade = kWaveSize / 64;
        for (int i = 0; i < fade; ++i)
        {
            const float w = (float) i / fade;
            fr[(size_t) i] *= w;
            fr[(size_t) (kWaveSize - 1 - i)] *= w;
        }
        wavegen::removeDc (fr);
        frames.push_back (std::move (fr));
    }
    wavegen::normaliseTable (frames, 1.0f);
    return frames;
}

std::unique_ptr<SampleAsset> toSample (const Audio& a, int start, int end, float rootNote, bool loop)
{
    start = juce::jlimit (0, a.length() - 1, start);
    end = juce::jlimit (start + 2, a.length(), end);
    auto s = std::make_unique<SampleAsset>();
    s->name = a.name;
    s->sourceRate = a.rate;
    s->rootNote = rootNote;
    s->numChannels = juce::jmin (2, a.buffer.getNumChannels());
    s->length = end - start;
    s->loop = loop;
    s->loopStart = 0;
    s->loopEnd = s->length;
    for (int c = 0; c < s->numChannels; ++c)
    {
        auto& d = s->data[0][c];
        d.assign ((size_t) s->length + 4, 0.0f);
        for (int i = 0; i < s->length; ++i) d[(size_t) i] = a.buffer.getSample (c, start + i);
    }
    s->buildLevels();
    return s;
}
} // namespace dw::sampleimport
