#include "WaveDocument.h"
#include "../Core/Expression.h"
#include "../DSP/Modulators.h"

namespace dw
{
namespace
{
constexpr int N = kWaveSize;

float sampleAt (const std::vector<float>& s, double x)
{
    x -= std::floor (x);
    const double pos = x * N;
    const int i = (int) pos;
    const float t = (float) (pos - i);
    return s[(size_t) (i % N)] + (s[(size_t) ((i + 1) % N)] - s[(size_t) (i % N)]) * t;
}

juce::String encodeFloats (const std::vector<float>& data)
{
    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream gz (compressed, 9);
        gz.write (data.data(), data.size() * sizeof (float));
    }
    return compressed.getMemoryBlock().toBase64Encoding();
}

std::vector<float> decodeFloats (const juce::String& b64)
{
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (b64)) return {};
    juce::MemoryInputStream in (mb, false);
    juce::GZIPDecompressorInputStream gz (in);
    juce::MemoryBlock out;
    gz.readIntoMemoryBlock (out);
    std::vector<float> v (out.getSize() / sizeof (float));
    std::memcpy (v.data(), out.getData(), v.size() * sizeof (float));
    for (auto& x : v)
        if (! std::isfinite (x)) x = 0.0f;
    return v;
}
} // namespace

WaveDocument::WaveDocument()
{
    loadFactory (0);
    undoStack.clear();
}

// ====================================================================================== whole-table
void WaveDocument::loadFactory (int index)
{
    const auto& names = wavegen::factoryTableNames();
    index = juce::jlimit (0, names.size() - 1, index);
    setFrames (wavegen::factoryTable (index), names[index]);
    factorySource = "factory:" + names[index];
}

void WaveDocument::setFrames (const wavegen::Frames& f, const juce::String& newName, bool undoable)
{
    if (undoable) pushUndo();
    frames.clear();
    for (const auto& src : f)
    {
        Frame fr;
        for (int i = 0; i < N && i < (int) src.size(); ++i)
            fr.samples[(size_t) i] = src[(size_t) i];
        frames.push_back (std::move (fr));
        if ((int) frames.size() >= kMaxFrames) break;
    }
    if (frames.empty()) frames.emplace_back();
    name = newName;
    selected = juce::jlimit (0, getNumFrames() - 1, selected);
    touched();
}

void WaveDocument::generateFromExpression (const juce::String& expr, int numFrames, juce::String& errorOut)
{
    Expression e;
    errorOut = e.compile (expr);
    if (errorOut.isNotEmpty()) return;

    numFrames = juce::jlimit (1, kMaxFrames, numFrames);
    wavegen::Frames out;
    Expression::Vars v;
    v.N = numFrames;
    for (int f = 0; f < numFrames; ++f)
    {
        v.n = f;
        v.f = numFrames > 1 ? (double) f / (numFrames - 1) : 0.0;
        wavegen::Frame fr ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            v.x = (double) i / N;
            fr[(size_t) i] = (float) juce::jlimit (-2.0, 2.0, e.evaluate (v));
        }
        out.push_back (std::move (fr));
    }
    setFrames (out, "Math");
}

void WaveDocument::generateRandom (wavegen::RandomStyle style, uint32_t seed, float complexity, int numFrames, bool currentFrameOnly)
{
    if (currentFrameOnly)
    {
        pushUndo();
        auto cyc = wavegen::randomCycle (style, seed, complexity);
        auto& fr = frames[(size_t) selected];
        fr.samples = cyc;
        fr.pointMode = false;
        fr.points.clear();
        touched();
        return;
    }
    setFrames (wavegen::randomTable (style, seed, complexity, numFrames), "Random " + wavegen::randomStyleNames()[(int) style]);
}

// ===================================================================================== frame editing
void WaveDocument::beginGesture()
{
    pushUndo();
}

std::vector<int> WaveDocument::targetFrames (int frame, bool allFrames) const
{
    std::vector<int> t;
    if (allFrames)
        for (int i = 0; i < getNumFrames(); ++i) t.push_back (i);
    else
        t.push_back (juce::jlimit (0, getNumFrames() - 1, frame));
    return t;
}

void WaveDocument::drawSegment (int frame, float x0, float y0, float x1, float y1, bool allFrames)
{
    if (x1 < x0) { std::swap (x0, x1); std::swap (y0, y1); }
    x0 = juce::jlimit (0.0f, 1.0f, x0);
    x1 = juce::jlimit (0.0f, 1.0f, x1);
    const int i0 = juce::jlimit (0, N - 1, (int) std::floor (x0 * N));
    const int i1 = juce::jlimit (0, N - 1, (int) std::ceil (x1 * N));
    for (int f : targetFrames (frame, allFrames))
    {
        auto& fr = frames[(size_t) f];
        fr.pointMode = false;
        fr.points.clear();
        for (int i = i0; i <= i1; ++i)
        {
            const float t = i1 == i0 ? 1.0f : (float) (i - i0) / (float) (i1 - i0);
            fr.samples[(size_t) i] = juce::jlimit (-2.0f, 2.0f, y0 + (y1 - y0) * t);
        }
    }
    touched();
}

std::vector<float> WaveDocument::renderPoints (const std::vector<ControlPoint>& input)
{
    std::vector<float> out ((size_t) N, 0.0f);
    if (input.empty()) return out;
    auto pts = input;
    std::sort (pts.begin(), pts.end(), [] (const ControlPoint& a, const ControlPoint& b) { return a.x < b.x; });
    const int n = (int) pts.size();
    for (int i = 0; i < N; ++i)
    {
        const float x = (float) i / N;
        // find segment [a, b] containing x (periodic)
        int a = n - 1;
        for (int k = 0; k < n; ++k)
            if (pts[(size_t) k].x <= x) a = k;
        const int b = (a + 1) % n;
        float xa = pts[(size_t) a].x, xb = pts[(size_t) b].x;
        float xx = x;
        if (b <= a) // wrap segment
        {
            xb += 1.0f;
            if (xx < xa) xx += 1.0f;
        }
        const float len = juce::jmax (1.0e-6f, xb - xa);
        const float u = juce::jlimit (0.0f, 1.0f, (xx - xa) / len);
        const float shaped = Envelope::curve (u, pts[(size_t) a].curve);
        out[(size_t) i] = pts[(size_t) a].y + (pts[(size_t) b].y - pts[(size_t) a].y) * shaped;
    }
    return out;
}

void WaveDocument::setPoints (int frame, const std::vector<ControlPoint>& pts)
{
    auto& fr = frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)];
    fr.points = pts;
    for (auto& p : fr.points)
    {
        p.x = juce::jlimit (0.0f, 1.0f, p.x);
        p.y = juce::jlimit (-2.0f, 2.0f, p.y);
        p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
    }
    fr.pointMode = true;
    fr.samples = renderPoints (fr.points);
    touched();
}

void WaveDocument::convertToPoints (int frame, int numPoints)
{
    pushUndo();
    auto& fr = frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)];
    numPoints = juce::jlimit (2, 128, numPoints);
    std::vector<ControlPoint> pts;
    for (int k = 0; k < numPoints; ++k)
    {
        const float x = (float) k / numPoints;
        pts.push_back ({ x, sampleAt (fr.samples, x), 0.0f });
    }
    setPoints (frame, pts);
}

void WaveDocument::leavePointMode (int frame)
{
    auto& fr = frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)];
    fr.pointMode = false;
    fr.points.clear();
    ++version;
}

void WaveDocument::applyTransform (Transform t, float amount, bool allFrames)
{
    pushUndo();
    for (int f : targetFrames (selected, allFrames))
    {
        auto& fr = frames[(size_t) f];
        auto& s = fr.samples;
        std::vector<float> src = s;
        switch (t)
        {
            case Transform::Invert: for (auto& v : s) v = -v; break;
            case Transform::Reverse: std::reverse (s.begin(), s.end()); break;
            case Transform::MirrorSymmetric:
                for (int i = N / 2; i < N; ++i) s[(size_t) i] = src[(size_t) (N - 1 - i)];
                break;
            case Transform::Normalise: wavegen::normalise (s, 1.0f); break;
            case Transform::RemoveDc: wavegen::removeDc (s); break;
            case Transform::Smooth:
            {
                auto spec = analyse (s.data());
                const double H = 4.0 + (1.0 - amount) * (1.0 - amount) * 1020.0;
                for (size_t h = 1; h < spec.bins.size(); ++h)
                    spec.bins[h] *= std::exp (-std::pow ((double) h / H, 2.0));
                synthesise (spec, s.data());
                break;
            }
            case Transform::Rectify:
                for (auto& v : s) v = v + (std::abs (v) - v) * amount;
                break;
            case Transform::Fold:
            {
                const float g = 1.0f + amount * 4.0f;
                for (auto& v : s) v = std::sin (v * g * kPi * 0.5f);
                break;
            }
            case Transform::Quantise:
            {
                const float steps = std::round (64.0f - amount * 62.0f);
                for (auto& v : s) v = std::round (v * steps) / steps;
                break;
            }
            case Transform::Clear: std::fill (s.begin(), s.end(), 0.0f); break;
            case Transform::ShiftPhase:
            {
                const int k = (int) std::round (amount * N);
                for (int i = 0; i < N; ++i) s[(size_t) i] = src[(size_t) (((i + k) % N + N) % N)];
                break;
            }
            case Transform::StretchToCentre:
                for (int i = 0; i < N; ++i)
                    s[(size_t) i] = sampleAt (src, 0.5 + ((double) i / N - 0.5) / (1.0 + amount * 3.0));
                break;
            case Transform::CompressToCentre:
                for (int i = 0; i < N; ++i)
                    s[(size_t) i] = sampleAt (src, 0.5 + ((double) i / N - 0.5) * (1.0 + amount * 3.0));
                break;
            case Transform::Scale: for (auto& v : s) v *= amount * 2.0f; break;
            case Transform::Offset: for (auto& v : s) v = juce::jlimit (-2.0f, 2.0f, v + (amount * 2.0f - 1.0f)); break;
            case Transform::Sharpen:
            {
                auto spec = analyse (s.data());
                for (size_t h = 1; h < spec.bins.size(); ++h)
                    spec.bins[h] *= std::pow ((double) h, (double) amount * 0.8);
                synthesise (spec, s.data());
                wavegen::normalise (s, 1.0f);
                break;
            }
            case Transform::HalfCycle:
                for (int i = 0; i < N; ++i)
                {
                    const double x = (double) i / N;
                    s[(size_t) i] = x < 0.5 ? sampleAt (src, 2.0 * x) : -sampleAt (src, 2.0 * x - 1.0);
                }
                break;
            case Transform::DoubleCycle:
                for (int i = 0; i < N; ++i)
                    s[(size_t) i] = sampleAt (src, 2.0 * (double) i / N);
                break;
        }
        for (auto& v : s) v = std::isfinite (v) ? juce::jlimit (-2.0f, 2.0f, v) : 0.0f;
        if (fr.pointMode && t != Transform::Clear)
        {
            fr.pointMode = false;
            fr.points.clear();
        }
    }
    touched();
}

void WaveDocument::getHarmonics (int frame, std::vector<float>& mags, std::vector<float>& phases, int count) const
{
    const auto spec = analyse (getFrame (frame).samples.data());
    count = juce::jlimit (1, kMaxHarmonics - 1, count);
    mags.resize ((size_t) count);
    phases.resize ((size_t) count);
    for (int h = 1; h <= count; ++h)
    {
        const auto b = spec.bins[(size_t) h];
        mags[(size_t) h - 1] = (float) (std::abs (b) * 2.0 / N);
        phases[(size_t) h - 1] = (float) (std::arg (b) + juce::MathConstants<double>::halfPi);
    }
}

void WaveDocument::setHarmonic (int frame, int harmonic, float magnitude, float phase, bool allFrames)
{
    if (harmonic < 1 || harmonic >= kMaxHarmonics) return;
    for (int f : targetFrames (frame, allFrames))
    {
        auto& fr = frames[(size_t) f];
        auto spec = analyse (fr.samples.data());
        spec.bins[(size_t) harmonic] = std::polar ((double) magnitude * N * 0.5, (double) phase - juce::MathConstants<double>::halfPi);
        synthesise (spec, fr.samples.data());
        fr.pointMode = false;
        fr.points.clear();
    }
    touched();
}

void WaveDocument::setHarmonics (int frame, const std::vector<float>& mags, const std::vector<float>& phases)
{
    auto& fr = frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)];
    auto spec = analyse (fr.samples.data());
    for (size_t h = 1; h <= mags.size() && h < (size_t) kMaxHarmonics; ++h)
    {
        const double ph = h - 1 < phases.size() ? (double) phases[h - 1] : std::arg (spec.bins[h]) + juce::MathConstants<double>::halfPi;
        spec.bins[h] = std::polar ((double) mags[h - 1] * N * 0.5, ph - juce::MathConstants<double>::halfPi);
    }
    synthesise (spec, fr.samples.data());
    fr.pointMode = false;
    fr.points.clear();
    touched();
}

// ======================================================================================= multi-frame
void WaveDocument::addFrame (bool duplicateSelected)
{
    if (getNumFrames() >= kMaxFrames) return;
    pushUndo();
    Frame fr = duplicateSelected ? frames[(size_t) selected] : Frame {};
    frames.insert (frames.begin() + selected + 1, fr);
    ++selected;
    touched();
}

void WaveDocument::deleteFrame (int f)
{
    if (getNumFrames() <= 1) return;
    pushUndo();
    f = juce::jlimit (0, getNumFrames() - 1, f);
    frames.erase (frames.begin() + f);
    selected = juce::jlimit (0, getNumFrames() - 1, selected >= f ? selected - 1 : selected);
    touched();
}

void WaveDocument::moveFrame (int from, int to)
{
    from = juce::jlimit (0, getNumFrames() - 1, from);
    to = juce::jlimit (0, getNumFrames() - 1, to);
    if (from == to) return;
    pushUndo();
    auto fr = frames[(size_t) from];
    frames.erase (frames.begin() + from);
    frames.insert (frames.begin() + to, fr);
    selected = to;
    touched();
}

void WaveDocument::interpolateBetween (int a, int b, bool spectral)
{
    if (a > b) std::swap (a, b);
    a = juce::jlimit (0, getNumFrames() - 1, a);
    b = juce::jlimit (0, getNumFrames() - 1, b);
    if (b - a < 2) return;
    pushUndo();
    const auto fa = frames[(size_t) a].samples, fb = frames[(size_t) b].samples;
    for (int f = a + 1; f < b; ++f)
    {
        const float t = (float) (f - a) / (float) (b - a);
        auto& fr = frames[(size_t) f];
        if (spectral)
            fr.samples = wavegen::spectralMorph (fa, fb, t);
        else
            for (int i = 0; i < N; ++i) fr.samples[(size_t) i] = fa[(size_t) i] + (fb[(size_t) i] - fa[(size_t) i]) * t;
        fr.pointMode = false;
        fr.points.clear();
    }
    touched();
}

void WaveDocument::resampleFrameCount (int newCount, bool spectral)
{
    newCount = juce::jlimit (1, kMaxFrames, newCount);
    if (newCount == getNumFrames()) return;
    pushUndo();
    const auto old = frames;
    const int oc = (int) old.size();
    std::vector<Frame> out ((size_t) newCount);
    for (int f = 0; f < newCount; ++f)
    {
        const double pos = newCount > 1 ? (double) f / (newCount - 1) * (oc - 1) : 0.0;
        const int k = juce::jmin (oc - 1, (int) pos);
        const int k2 = juce::jmin (oc - 1, k + 1);
        const float t = (float) (pos - k);
        if (t < 1.0e-4f || k == k2)
            out[(size_t) f].samples = old[(size_t) k].samples;
        else if (spectral)
            out[(size_t) f].samples = wavegen::spectralMorph (old[(size_t) k].samples, old[(size_t) k2].samples, t);
        else
            for (int i = 0; i < N; ++i)
                out[(size_t) f].samples[(size_t) i] = old[(size_t) k].samples[(size_t) i]
                    + (old[(size_t) k2].samples[(size_t) i] - old[(size_t) k].samples[(size_t) i]) * t;
    }
    frames = std::move (out);
    selected = juce::jlimit (0, newCount - 1, selected);
    touched();
}

// ============================================================================================== undo
void WaveDocument::pushUndo()
{
    undoStack.push_back ({ frames, name, factorySource, selected });
    if (undoStack.size() > kMaxUndo) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void WaveDocument::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back ({ frames, name, factorySource, selected });
    auto s = std::move (undoStack.back());
    undoStack.pop_back();
    frames = std::move (s.frames); name = s.name; factorySource = s.factorySource; selected = s.selected;
    ++version;
}

void WaveDocument::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back ({ frames, name, factorySource, selected });
    auto s = std::move (redoStack.back());
    redoStack.pop_back();
    frames = std::move (s.frames); name = s.name; factorySource = s.factorySource; selected = s.selected;
    ++version;
}

// =============================================================================================== IO
wavegen::Frames WaveDocument::getAllFrames() const
{
    wavegen::Frames f;
    f.reserve (frames.size());
    for (const auto& fr : frames) f.push_back (fr.samples);
    return f;
}

std::unique_ptr<Wavetable> WaveDocument::buildTable() const
{
    return Wavetable::fromFrames (getAllFrames(), name);
}

juce::ValueTree WaveDocument::toValueTree() const
{
    juce::ValueTree v ("WAVE");
    v.setProperty ("name", name, nullptr);
    v.setProperty ("selected", selected, nullptr);
    if (factorySource.isNotEmpty())
    {
        v.setProperty ("factory", factorySource.fromFirstOccurrenceOf ("factory:", false, false), nullptr);
        return v;
    }
    std::vector<float> all;
    all.reserve (frames.size() * (size_t) N);
    for (const auto& fr : frames) all.insert (all.end(), fr.samples.begin(), fr.samples.end());
    v.setProperty ("frames", (int) frames.size(), nullptr);
    v.setProperty ("data", encodeFloats (all), nullptr);

    for (int f = 0; f < (int) frames.size(); ++f)
    {
        const auto& fr = frames[(size_t) f];
        if (! fr.pointMode || fr.points.empty()) continue;
        juce::ValueTree p ("POINTS");
        p.setProperty ("frame", f, nullptr);
        juce::String s;
        for (const auto& cp : fr.points)
            s << juce::String (cp.x, 5) << "," << juce::String (cp.y, 5) << "," << juce::String (cp.curve, 4) << ";";
        p.setProperty ("pts", s, nullptr);
        v.appendChild (p, nullptr);
    }
    return v;
}

void WaveDocument::fromValueTree (const juce::ValueTree& v)
{
    if (! v.isValid()) return;
    undoStack.clear();
    redoStack.clear();

    if (v.hasProperty ("expr")) // compact procedural definition (factory presets)
    {
        juce::String err;
        generateFromExpression (v["expr"].toString(), juce::jlimit (1, kMaxFrames, (int) v.getProperty ("frames", 1)), err);
        if (err.isNotEmpty()) loadFactory (0);
        name = v.getProperty ("name", "Math").toString();
        undoStack.clear();
        return;
    }
    if (v.hasProperty ("rstyle"))
    {
        generateRandom ((wavegen::RandomStyle) juce::jlimit (0, (int) wavegen::RandomStyle::NumStyles - 1, (int) v["rstyle"]),
                        (uint32_t) (juce::int64) v.getProperty ("rseed", 1), (float) v.getProperty ("rcomplex", 0.5),
                        juce::jlimit (1, kMaxFrames, (int) v.getProperty ("frames", 32)), false);
        name = v.getProperty ("name", "Random").toString();
        undoStack.clear();
        return;
    }
    if (v.hasProperty ("factory"))
    {
        const int idx = wavegen::factoryTableIndex (v["factory"].toString());
        loadFactory (juce::jmax (0, idx));
        undoStack.clear();
        selected = juce::jlimit (0, getNumFrames() - 1, (int) v.getProperty ("selected", 0));
        return;
    }

    const int count = juce::jlimit (1, kMaxFrames, (int) v.getProperty ("frames", 1));
    const auto data = decodeFloats (v["data"].toString());
    frames.assign ((size_t) count, Frame {});
    for (int f = 0; f < count; ++f)
        for (int i = 0; i < N; ++i)
        {
            const size_t k = (size_t) f * N + (size_t) i;
            frames[(size_t) f].samples[(size_t) i] = k < data.size() ? data[k] : 0.0f;
        }

    for (const auto& p : v)
    {
        if (! p.hasType ("POINTS")) continue;
        const int f = (int) p["frame"];
        if (f < 0 || f >= count) continue;
        std::vector<ControlPoint> pts;
        for (const auto& tok : juce::StringArray::fromTokens (p["pts"].toString(), ";", ""))
        {
            const auto parts = juce::StringArray::fromTokens (tok, ",", "");
            if (parts.size() == 3)
                pts.push_back ({ parts[0].getFloatValue(), parts[1].getFloatValue(), parts[2].getFloatValue() });
        }
        if (! pts.empty())
        {
            frames[(size_t) f].points = pts;
            frames[(size_t) f].pointMode = true;
        }
    }
    name = v.getProperty ("name", "Custom").toString();
    factorySource = {};
    selected = juce::jlimit (0, count - 1, (int) v.getProperty ("selected", 0));
    ++version;
}
} // namespace dw
