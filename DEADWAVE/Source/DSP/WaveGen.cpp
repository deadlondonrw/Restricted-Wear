#include "WaveGen.h"
#include "../Core/DspUtil.h"
#include <cmath>

namespace dw::wavegen
{
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

double frac (double v) { return v - std::floor (v); }

Frame lerpFrames (const Frame& a, const Frame& b, float t)
{
    Frame r ((size_t) kWaveSize);
    for (int i = 0; i < kWaveSize; ++i)
        r[(size_t) i] = a[(size_t) i] + (b[(size_t) i] - a[(size_t) i]) * t;
    return r;
}

/** Morphs through keyframes (time-domain crossfade) to produce numFrames frames. */
Frames morphKeys (const Frames& keys, int numFrames)
{
    Frames out;
    out.reserve ((size_t) numFrames);
    for (int f = 0; f < numFrames; ++f)
    {
        const double pos = numFrames > 1 ? (double) f / (double) (numFrames - 1) * (double) (keys.size() - 1) : 0.0;
        const int k = juce::jmin ((int) keys.size() - 2, (int) std::floor (pos));
        if (keys.size() == 1) { out.push_back (keys[0]); continue; }
        out.push_back (lerpFrames (keys[(size_t) k], keys[(size_t) k + 1], (float) (pos - k)));
    }
    return out;
}

Frames generate (int numFrames, const std::function<Frame (double t)>& fn)
{
    Frames out;
    out.reserve ((size_t) numFrames);
    for (int f = 0; f < numFrames; ++f)
        out.push_back (fn (numFrames > 1 ? (double) f / (double) (numFrames - 1) : 0.0));
    return out;
}

/** Formant envelope gain at a frequency (in harmonic units) for a set of formants. */
double formantGain (double h, const double* centres, const double* widths, const double* gains, int n)
{
    double g = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double d = (h - centres[i]) / widths[i];
        g += gains[i] / (1.0 + d * d);
    }
    return g;
}
} // namespace

// ================================================================================== primitives
Frame bandLimited (const std::function<double (double)>& fn, int oversample)
{
    const int n = kWaveSize * juce::jmax (1, oversample);
    std::vector<std::complex<double>> buf ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double v = fn ((double) i / (double) n);
        buf[(size_t) i] = { std::isfinite (v) ? v : 0.0, 0.0 };
    }
    fft (buf, false);

    Spectrum s;
    s.bins.resize ((size_t) kMaxHarmonics + 1);
    const double scale = (double) kWaveSize / (double) n;
    for (int h = 0; h <= kMaxHarmonics; ++h)
        s.bins[(size_t) h] = buf[(size_t) h] * scale;
    s.bins[(size_t) kMaxHarmonics] = { s.bins[(size_t) kMaxHarmonics].real(), 0.0 };

    Frame out ((size_t) kWaveSize);
    synthesise (s, out.data());
    return out;
}

Frame fromHarmonics (const std::vector<float>& mags, const std::vector<float>& phases)
{
    Spectrum s;
    s.bins.assign ((size_t) kMaxHarmonics + 1, { 0.0, 0.0 });
    const int n = juce::jmin ((int) mags.size(), kMaxHarmonics - 1);
    for (int h = 1; h <= n; ++h)
    {
        const double m = mags[(size_t) h - 1];
        const double ph = h - 1 < (int) phases.size() ? (double) phases[(size_t) h - 1] : 0.0;
        // sin(2 pi h x + ph) => bin = (N/2) * m * e^{i(ph - pi/2)}
        s.bins[(size_t) h] = std::polar (m * (double) kWaveSize * 0.5, ph - pi * 0.5);
    }
    Frame out ((size_t) kWaveSize);
    synthesise (s, out.data());
    return out;
}

Frame sine()
{
    Frame f ((size_t) kWaveSize);
    for (int i = 0; i < kWaveSize; ++i)
        f[(size_t) i] = (float) std::sin (twoPi * i / kWaveSize);
    return f;
}

Frame saw()
{
    std::vector<float> m ((size_t) kMaxHarmonics - 1);
    for (int h = 1; h < kMaxHarmonics; ++h)
        m[(size_t) h - 1] = (float) (2.0 / (pi * h)) * (h % 2 == 1 ? 1.0f : -1.0f);
    // Rising ramp from -1 to 1 starting at 0: x(t) = 2 * sum (-1)^(h+1) sin(2 pi h t) / (pi h), shifted by half a cycle.
    std::vector<float> p ((size_t) kMaxHarmonics - 1, 0.0f);
    for (int h = 1; h < kMaxHarmonics; ++h)
        p[(size_t) h - 1] = (float) (h % 2 == 1 ? pi : 0.0) + (float) (pi * h); // half-cycle shift so it starts at 0 rising
    auto f = fromHarmonics (m, p);
    normalise (f, 1.0f);
    return f;
}

Frame square()
{
    std::vector<float> m ((size_t) kMaxHarmonics - 1, 0.0f);
    for (int h = 1; h < kMaxHarmonics; h += 2)
        m[(size_t) h - 1] = (float) (4.0 / (pi * h));
    auto f = fromHarmonics (m);
    normalise (f, 1.0f);
    return f;
}

Frame triangle()
{
    std::vector<float> m ((size_t) kMaxHarmonics - 1, 0.0f);
    for (int h = 1; h < kMaxHarmonics; h += 2)
        m[(size_t) h - 1] = (float) (8.0 / (pi * pi * h * h)) * (((h - 1) / 2) % 2 == 0 ? 1.0f : -1.0f);
    auto f = fromHarmonics (m);
    normalise (f, 1.0f);
    return f;
}

Frame pulse (double width)
{
    width = juce::jlimit (0.005, 0.995, width);
    auto f = bandLimited ([width] (double x) { return x < width ? 1.0 : -1.0; }, 16);
    removeDc (f);
    normalise (f, 1.0f);
    return f;
}

void normalise (Frame& f, float peak)
{
    float m = 0.0f;
    for (auto v : f) m = juce::jmax (m, std::abs (v));
    if (m > 1.0e-9f)
        for (auto& v : f) v *= peak / m;
}

void removeDc (Frame& f)
{
    double sum = 0.0;
    for (auto v : f) sum += v;
    const float mean = (float) (sum / (double) f.size());
    for (auto& v : f) v -= mean;
}

void normaliseTable (Frames& frames, float peak)
{
    float m = 0.0f;
    for (auto& f : frames)
        for (auto v : f) m = juce::jmax (m, std::abs (v));
    if (m > 1.0e-9f)
        for (auto& f : frames)
            for (auto& v : f) v *= peak / m;
}

// ================================================================================ factory tables
const juce::StringArray& factoryTableNames()
{
    static const juce::StringArray names {
        "Basic Shapes", "Saw Tilt", "PWM", "Harmonic Sweep", "Formant Vowels", "FM 1-1", "FM 1-3", "Sync Saw",
        "Sync Sine", "Wavefold", "Tanh Drive", "Odd Harmonics", "Digital Steps", "Chip", "Growl", "Comb Sweep",
        "Spectral Noise", "Organ", "Sub Shapes", "Metallic", "Glass", "Phase Distortion", "Skew Triangle",
        "Hollow Square", "Octave Stack", "Vocal Growl"
    };
    return names;
}

int factoryTableIndex (const juce::String& name)
{
    return factoryTableNames().indexOf (name);
}

Frames factoryTable (int index)
{
    constexpr int F = 64;
    Frames out;

    switch (index)
    {
        case 0: // Basic Shapes: sine > triangle > saw > square > narrow pulse
            out = morphKeys ({ sine(), triangle(), saw(), square(), pulse (0.12) }, F);
            break;

        case 1: // Saw Tilt: dark saw opening to full brightness
            out = generate (F, [] (double t)
            {
                std::vector<float> m ((size_t) kMaxHarmonics - 1);
                const double slope = 2.5 * (1.0 - t); // extra dB/oct-like rolloff exponent
                for (int h = 1; h < kMaxHarmonics; ++h)
                    m[(size_t) h - 1] = (float) (1.0 / (h * std::pow ((double) h, slope)));
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 2: // PWM 50% -> 3%
            out = generate (F, [] (double t) { return pulse (0.5 - 0.47 * t); });
            break;

        case 3: // Harmonic Sweep: harmonics added one region at a time
            out = generate (F, [] (double t)
            {
                const double top = 1.0 + std::pow (t, 2.0) * 127.0;
                std::vector<float> m (128, 0.0f);
                for (int h = 1; h <= 128; ++h)
                {
                    const double g = juce::jlimit (0.0, 1.0, top - (h - 1));
                    m[(size_t) h - 1] = (float) (g / std::sqrt ((double) h));
                }
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 4: // Formant Vowels A E I O U (formants in harmonics of a ~110 Hz fundamental)
        {
            const double vowels[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                          { 570, 840, 2410 }, { 300, 870, 2240 } };
            out = generate (F, [&] (double t)
            {
                const double pos = t * 4.0;
                const int a = juce::jmin (3, (int) pos);
                const double u = pos - a;
                double c[3], w[3] = { 1.2, 2.0, 3.0 }, g[3] = { 1.0, 0.6, 0.35 };
                for (int i = 0; i < 3; ++i)
                    c[i] = (vowels[a][i] + (vowels[a + 1][i] - vowels[a][i]) * u) / 110.0;
                std::vector<float> m (256);
                for (int h = 1; h <= 256; ++h)
                    m[(size_t) h - 1] = (float) (formantGain (h, c, w, g, 3) / std::sqrt ((double) h) + 0.02 / h);
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;
        }

        case 5: // FM 1:1 index 0..6
            out = generate (F, [] (double t)
            {
                const double idx = 6.0 * t * t;
                auto f = bandLimited ([idx] (double x) { return std::sin (twoPi * x + idx * std::sin (twoPi * x)); });
                normalise (f);
                return f;
            });
            break;

        case 6: // FM 1:3
            out = generate (F, [] (double t)
            {
                const double idx = 5.0 * t;
                auto f = bandLimited ([idx] (double x) { return std::sin (twoPi * x + idx * std::sin (3.0 * twoPi * x)); });
                normalise (f);
                return f;
            });
            break;

        case 7: // Sync Saw ratio 1..7
            out = generate (F, [] (double t)
            {
                const double r = 1.0 + 6.0 * t;
                auto f = bandLimited ([r] (double x) { return 2.0 * frac (x * r) - 1.0; }, 16);
                removeDc (f);
                normalise (f);
                return f;
            });
            break;

        case 8: // Sync Sine ratio 1..8, windowed to soften the reset
            out = generate (F, [] (double t)
            {
                const double r = 1.0 + 7.0 * t;
                auto f = bandLimited ([r] (double x) { return std::sin (twoPi * x * r) * (1.0 - x); }, 16);
                removeDc (f);
                normalise (f);
                return f;
            });
            break;

        case 9: // Wavefold
            out = generate (F, [] (double t)
            {
                const double gain = 1.0 + 7.0 * t;
                auto f = bandLimited ([gain] (double x)
                {
                    const double v = std::sin (twoPi * x) * gain;
                    return std::sin (v * pi * 0.5);
                }, 16);
                normalise (f);
                return f;
            });
            break;

        case 10: // Tanh Drive: sine -> saturated square
            out = generate (F, [] (double t)
            {
                const double gain = 1.0 + 30.0 * t * t;
                auto f = bandLimited ([gain] (double x) { return std::tanh (gain * std::sin (twoPi * x)); });
                normalise (f);
                return f;
            });
            break;

        case 11: // Odd Harmonics: count sweep 1..63 odd harmonics, equal-ish
            out = generate (F, [] (double t)
            {
                const int count = 1 + (int) std::round (t * 40.0);
                std::vector<float> m (128, 0.0f);
                for (int i = 0; i < count; ++i)
                {
                    const int h = 2 * i + 1;
                    if (h <= 128) m[(size_t) h - 1] = (float) (1.0 / std::pow ((double) h, 0.7));
                }
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 12: // Digital Steps: sine quantised from 64 to 2 levels
            out = generate (F, [] (double t)
            {
                const double steps = std::round (std::pow (2.0, 6.0 - 5.0 * t));
                auto f = bandLimited ([steps] (double x)
                {
                    return std::round (std::sin (twoPi * x) * steps) / steps;
                }, 16);
                normalise (f);
                return f;
            });
            break;

        case 13: // Chip: 50 / 25 / 12.5% pulses, 4-bit triangle, 4-bit saw
        {
            auto quantTri = bandLimited ([] (double x)
            {
                const double tri = x < 0.5 ? 4.0 * x - 1.0 : 3.0 - 4.0 * x;
                return std::round (tri * 7.5) / 7.5;
            }, 16);
            auto quantSaw = bandLimited ([] (double x) { return std::round ((2.0 * x - 1.0) * 7.5) / 7.5; }, 16);
            removeDc (quantSaw);
            normalise (quantTri); normalise (quantSaw);
            Frames keys { pulse (0.5), pulse (0.25), pulse (0.125), quantTri, quantSaw };
            for (auto& k : keys)
                for (int r = 0; r < 4; ++r)
                    out.push_back (k); // stepped: no interpolation between characters
            break;
        }

        case 14: // Growl: saw through a moving resonant formant plus mild fold
            out = generate (F, [] (double t)
            {
                const double centre = 3.0 + 22.0 * (0.5 - 0.5 * std::cos (t * pi * 2.0)) + 6.0 * t;
                const double c[2] = { centre, centre * 2.4 }, w[2] = { 1.5 + 2.0 * t, 3.0 }, g[2] = { 1.0, 0.5 };
                std::vector<float> m (512);
                for (int h = 1; h <= 512; ++h)
                    m[(size_t) h - 1] = (float) ((formantGain (h, c, w, g, 2) + 0.05) / h);
                auto base = fromHarmonics (m);
                normalise (base);
                const double fold = 1.0 + 1.5 * t;
                auto f = bandLimited ([&base, fold] (double x)
                {
                    const double pos = x * kWaveSize;
                    const int i = (int) pos;
                    const double u = pos - i;
                    const double v = base[(size_t) (i % kWaveSize)] * (1.0 - u) + base[(size_t) ((i + 1) % kWaveSize)] * u;
                    return std::sin (v * fold * pi * 0.5);
                }, 8);
                normalise (f);
                return f;
            });
            break;

        case 15: // Comb Sweep: saw with a moving comb (notches)
            out = generate (F, [] (double t)
            {
                const double spacing = 2.0 + 14.0 * t;
                std::vector<float> m (512);
                for (int h = 1; h <= 512; ++h)
                {
                    const double comb = 0.5 + 0.5 * std::cos (twoPi * h / spacing);
                    m[(size_t) h - 1] = (float) (comb / h);
                }
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 16: // Spectral Noise: deterministic random spectra, smoothly morphed
            out = randomTable (RandomStyle::Spectral, 0xDEAD5EEDu, 0.85f, F);
            break;

        case 17: // Organ: drawbar registrations (16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1')
        {
            const int harm[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 }; // relative to 16' = sub
            const int regs[6][9] = { { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 0, 0, 0 },
                                     { 8, 3, 8, 0, 0, 0, 0, 0, 3 }, { 6, 8, 8, 6, 0, 0, 0, 0, 0 },
                                     { 8, 8, 8, 8, 8, 8, 8, 8, 8 }, { 0, 0, 8, 8, 8, 8, 0, 0, 8 } };
            Frames keys;
            for (auto& r : regs)
            {
                std::vector<float> m (16, 0.0f);
                for (int i = 0; i < 9; ++i)
                    m[(size_t) harm[i] - 1] += (float) (r[i] > 0 ? std::pow (10.0, (r[i] - 8) * 3.0 / 20.0) : 0.0);
                auto f = fromHarmonics (m);
                normalise (f);
                keys.push_back (f);
            }
            out = morphKeys (keys, 48);
            break;
        }

        case 18: // Sub Shapes: sine, sine+octave, rounded triangle, squashed sine
        {
            auto s2 = fromHarmonics ({ 1.0f, 0.35f });
            normalise (s2);
            auto rtri = fromHarmonics ({ 1.0f, 0.0f, -0.111f, 0.0f, 0.04f });
            normalise (rtri);
            auto squash = bandLimited ([] (double x) { return std::tanh (2.5 * std::sin (twoPi * x)); });
            normalise (squash);
            out = morphKeys ({ sine(), s2, rtri, squash }, 32);
            break;
        }

        case 19: // Metallic: prime-numbered partials growing in density
            out = generate (F, [] (double t)
            {
                static const int primes[] = { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };
                std::vector<float> m (100, 0.0f);
                const int count = 2 + (int) (t * 23.0);
                for (int i = 0; i < count; ++i)
                    m[(size_t) primes[i] - 1] = (float) (1.0 / std::pow (1.0 + i, 0.6));
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 20: // Glass: square-number partials (bell-like brightness)
            out = generate (F, [] (double t)
            {
                std::vector<float> m (256, 0.0f);
                m[0] = 1.0f;
                for (int k = 2; k * k <= 256; ++k)
                    m[(size_t) (k * k) - 1] = (float) (std::pow (t, 0.5) * 0.8 / (k - 1));
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 21: // Phase Distortion: CZ-style resonant saw
            out = generate (F, [] (double t)
            {
                const double res = 1.0 + 14.0 * t;
                auto f = bandLimited ([res] (double x)
                {
                    const double window = 1.0 - x;
                    return std::cos (twoPi * frac (x * res)) * window - 0.0;
                }, 16);
                removeDc (f);
                normalise (f);
                return f;
            });
            break;

        case 22: // Skew Triangle: triangle -> ramp
            out = generate (F, [] (double t)
            {
                const double peak = 0.5 + 0.49 * t;
                auto f = bandLimited ([peak] (double x)
                {
                    return x < peak ? -1.0 + 2.0 * x / peak : 1.0 - 2.0 * (x - peak) / (1.0 - peak);
                }, 16);
                removeDc (f);
                normalise (f);
                return f;
            });
            break;

        case 23: // Hollow Square: square losing its lower harmonics
            out = generate (F, [] (double t)
            {
                std::vector<float> m (512, 0.0f);
                const double lowCut = 1.0 + 20.0 * t;
                for (int h = 1; h <= 512; h += 2)
                {
                    const double g = h < lowCut ? std::pow (h / lowCut, 2.0) : 1.0;
                    m[(size_t) h - 1] = (float) (g / h);
                }
                m[0] = juce::jmax (m[0], 0.15f); // keep pitch reference
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 24: // Octave Stack: partials at powers of two, brightness sweep
            out = generate (F, [] (double t)
            {
                std::vector<float> m (512, 0.0f);
                for (int k = 0, h = 1; h <= 512; ++k, h *= 2)
                    m[(size_t) h - 1] = (float) std::exp (-std::pow ((k - t * 8.0) / 2.0, 2.0));
                m[0] = juce::jmax (m[0], 0.2f);
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;

        case 25: // Vocal Growl: formant pairs morphing "oo" -> "ah" -> "ee" with a raspy top
        {
            const double v[3][2] = { { 3.0, 8.0 }, { 7.0, 11.0 }, { 3.0, 21.0 } };
            out = generate (F, [&] (double t)
            {
                const double pos = t * 2.0;
                const int a = juce::jmin (1, (int) pos);
                const double u = pos - a;
                const double c[3] = { v[a][0] + (v[a + 1][0] - v[a][0]) * u, v[a][1] + (v[a + 1][1] - v[a][1]) * u, 26.0 };
                const double w[3] = { 1.0, 1.6, 6.0 }, g[3] = { 1.0, 0.7, 0.2 + 0.3 * t };
                std::vector<float> m (512);
                for (int h = 1; h <= 512; ++h)
                    m[(size_t) h - 1] = (float) ((formantGain (h, c, w, g, 3) + 0.03) / std::pow ((double) h, 0.8));
                auto f = fromHarmonics (m);
                normalise (f);
                return f;
            });
            break;
        }

        default:
            out = { sine() };
            break;
    }

    if (out.empty())
        out = { sine() };
    return out;
}

// =================================================================================== random waves
const juce::StringArray& randomStyleNames()
{
    static const juce::StringArray n { "Spectral", "Harmonic", "Segments", "Stepped", "Chaotic" };
    return n;
}

Frame randomCycle (RandomStyle style, uint32_t seed, float complexity)
{
    Rng rng (seed * 2654435761u + 12345u);
    complexity = juce::jlimit (0.0f, 1.0f, complexity);

    switch (style)
    {
        case RandomStyle::Spectral:
        {
            const int count = 4 + (int) (complexity * 250.0f);
            const double tilt = 1.6 - 1.2 * complexity + 0.4 * rng.uni();
            std::vector<float> m ((size_t) count), p ((size_t) count);
            for (int h = 1; h <= count; ++h)
            {
                m[(size_t) h - 1] = (float) (std::pow (rng.uni(), 1.5) / std::pow ((double) h, tilt));
                p[(size_t) h - 1] = rng.uni() * kTwoPi;
            }
            m[0] = juce::jmax (m[0], 0.5f);
            auto f = fromHarmonics (m, p);
            normalise (f);
            return f;
        }
        case RandomStyle::Harmonic:
        {
            const int count = 2 + (int) (complexity * 12.0f);
            std::vector<float> m (64, 0.0f);
            m[0] = 1.0f;
            for (int i = 0; i < count; ++i)
            {
                const int h = 2 + (int) (rng.uni() * (8.0f + 55.0f * complexity));
                m[(size_t) juce::jmin (63, h - 1)] += rng.uni() * 0.8f;
            }
            auto f = fromHarmonics (m);
            normalise (f);
            return f;
        }
        case RandomStyle::Segments:
        {
            const int pts = 3 + (int) (complexity * 29.0f);
            std::vector<double> xs ((size_t) pts), ys ((size_t) pts);
            for (int i = 0; i < pts; ++i)
            {
                xs[(size_t) i] = (double) i / pts + rng.uni() * 0.6 / pts;
                ys[(size_t) i] = rng.bi();
            }
            auto f = bandLimited ([&] (double x)
            {
                int i = 0;
                while (i < pts - 1 && xs[(size_t) i + 1] <= x) ++i;
                const double x0 = xs[(size_t) i], y0 = ys[(size_t) i];
                const double x1 = i + 1 < pts ? xs[(size_t) i + 1] : 1.0 + xs[0];
                const double y1 = i + 1 < pts ? ys[(size_t) i + 1] : ys[0];
                double u = (x - x0) / juce::jmax (1.0e-9, x1 - x0);
                if (x < x0) u = 0.0;
                u = 0.5 - 0.5 * std::cos (u * pi); // smooth segments
                return y0 + (y1 - y0) * u;
            }, 8);
            removeDc (f);
            normalise (f);
            return f;
        }
        case RandomStyle::Stepped:
        {
            const int steps = 2 + (int) (complexity * 30.0f);
            std::vector<double> ys ((size_t) steps);
            for (auto& y : ys) y = rng.bi();
            auto f = bandLimited ([&] (double x) { return ys[(size_t) juce::jmin (steps - 1, (int) (x * steps))]; }, 16);
            removeDc (f);
            normalise (f);
            return f;
        }
        case RandomStyle::Chaotic:
        default:
        {
            double v = 0.3 + 0.4 * rng.uni();
            const double r = 3.6 + 0.39 * complexity;
            std::vector<double> ys (512);
            for (auto& y : ys) { v = r * v * (1.0 - v); y = v * 2.0 - 1.0; }
            // smooth with a moving average so the result is musical rather than white
            const int win = 1 + (int) ((1.0f - complexity) * 24.0f);
            auto f = bandLimited ([&] (double x)
            {
                const int c = (int) (x * 512.0);
                double acc = 0.0;
                for (int k = -win; k <= win; ++k)
                    acc += ys[(size_t) ((c + k + 512) % 512)];
                return acc / (2 * win + 1);
            }, 4);
            removeDc (f);
            normalise (f);
            return f;
        }
    }
}

Frames randomTable (RandomStyle style, uint32_t seed, float complexity, int numFrames)
{
    numFrames = juce::jlimit (1, kMaxFrames, numFrames);
    const int keys = juce::jlimit (2, 6, 2 + (int) (complexity * 4.0f));
    Frames keyFrames;
    for (int k = 0; k < keys; ++k)
        keyFrames.push_back (randomCycle (style, seed + (uint32_t) k * 7919u, complexity));

    Frames out;
    for (int f = 0; f < numFrames; ++f)
    {
        const double pos = numFrames > 1 ? (double) f / (numFrames - 1) * (keys - 1) : 0.0;
        const int a = juce::jmin (keys - 2, (int) pos);
        out.push_back (spectralMorph (keyFrames[(size_t) a], keyFrames[(size_t) a + 1], (float) (pos - a)));
    }
    normaliseTable (out);
    return out;
}

Frame spectralMorph (const Frame& a, const Frame& b, float t)
{
    const auto sa = analyse (a.data());
    const auto sb = analyse (b.data());
    Spectrum s;
    s.bins.resize (sa.bins.size());
    for (size_t h = 0; h < sa.bins.size(); ++h)
    {
        const double ma = std::abs (sa.bins[h]), mb = std::abs (sb.bins[h]);
        double pa = std::arg (sa.bins[h]), pb = std::arg (sb.bins[h]);
        double d = pb - pa;
        while (d > pi) d -= twoPi;
        while (d < -pi) d += twoPi;
        // Weight the phase path toward whichever side dominates in magnitude.
        const double m = ma + (mb - ma) * t;
        const double ph = pa + d * t;
        s.bins[h] = std::polar (m, ph);
    }
    s.bins[0] = sa.bins[0] + (sb.bins[0] - sa.bins[0]) * (double) t;
    Frame out ((size_t) kWaveSize);
    synthesise (s, out.data());
    return out;
}
} // namespace dw::wavegen
