// DEADNOISE - learned background noise profile.
#include "NoiseProfile.h"
#include "DspMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace deadnoise
{

namespace
{
    constexpr uint32_t kProfileMagic = 0x31504E44; // "DNP1"

    template <typename T> void put (std::vector<uint8_t>& v, const T& value)
    {
        const auto* p = reinterpret_cast<const uint8_t*> (&value);
        v.insert (v.end(), p, p + sizeof (T));
    }

    template <typename T> bool get (const uint8_t*& p, const uint8_t* end, T& value)
    {
        if ((size_t) (end - p) < sizeof (T)) return false;
        std::memcpy (&value, p, sizeof (T));
        p += sizeof (T);
        return true;
    }
}

std::vector<uint8_t> NoiseProfile::serialise() const
{
    std::vector<uint8_t> out;
    out.reserve (32 + psd.size() * sizeof (float));
    put (out, kProfileMagic);
    put (out, sampleRate);
    put (out, (int32_t) fftSize);
    put (out, acceptedFraction);
    put (out, levelDb);
    put (out, (int32_t) psd.size());
    for (float f : psd) put (out, f);
    return out;
}

bool NoiseProfile::deserialise (const uint8_t* data, size_t size, NoiseProfile& out)
{
    const uint8_t* p = data;
    const uint8_t* end = data + size;
    uint32_t magic = 0;
    int32_t fft = 0, count = 0;
    NoiseProfile r;
    if (! get (p, end, magic) || magic != kProfileMagic) return false;
    if (! get (p, end, r.sampleRate)) return false;
    if (! get (p, end, fft)) return false;
    if (! get (p, end, r.acceptedFraction)) return false;
    if (! get (p, end, r.levelDb)) return false;
    if (! get (p, end, count)) return false;
    if (fft < 64 || fft > 65536 || count != fft / 2 + 1) return false;
    if (! (r.sampleRate >= 8000.0 && r.sampleRate <= 768000.0)) return false;
    r.fftSize = fft;
    r.psd.resize ((size_t) count);
    for (auto& v : r.psd)
    {
        if (! get (p, end, v)) return false;
        if (! std::isfinite (v) || v < 0.0f) return false;
    }
    out = std::move (r);
    return true;
}

void mapProfileToGrid (const NoiseProfile& src, double dstRate, int dstFft, float* dst) noexcept
{
    const int dstBins = dstFft / 2 + 1;
    if (! src.isValid())
    {
        for (int k = 0; k < dstBins; ++k) dst[k] = 0.0f;
        return;
    }

    const int srcBins = (int) src.psd.size();
    const double srcBinHz = src.sampleRate / src.fftSize;
    const double dstBinHz = dstRate / dstFft;
    // Per-sample noise variance of a band-limited analog noise scales with the
    // sampling bandwidth; this keeps the level right when only the rate changes.
    const float rateScale = (float) (dstRate / src.sampleRate);
    const float tiny = 1.0e-30f;

    for (int k = 0; k < dstBins; ++k)
    {
        const double f = k * dstBinHz;
        double value;
        if (dstBinHz > srcBinHz * 1.01)
        {
            // destination bins are wider: average the source bins they cover
            const double lo = std::max (0.0, (f - 0.5 * dstBinHz) / srcBinHz);
            const double hi = (f + 0.5 * dstBinHz) / srcBinHz;
            int i0 = (int) std::ceil (lo);
            int i1 = (int) std::floor (hi);
            i0 = clampv (i0, 0, srcBins - 1);
            i1 = clampv (i1, i0, srcBins - 1);
            double s = 0.0;
            for (int i = i0; i <= i1; ++i) s += src.psd[(size_t) i];
            value = s / (i1 - i0 + 1);
        }
        else
        {
            const double pos = f / srcBinHz;
            if (pos >= srcBins - 1)
                value = src.psd[(size_t) srcBins - 1];
            else
            {
                const int i = (int) pos;
                const double fr = pos - i;
                const double a = std::log (std::max (src.psd[(size_t) i], tiny));
                const double b = std::log (std::max (src.psd[(size_t) i + 1], tiny));
                value = std::exp (a + (b - a) * fr);
            }
        }
        dst[k] = (float) value * rateScale;
    }
}

LearnAnalysis analyseNoiseCapture (const float* frames, int numFrames, int numBins,
                                   double sampleRate, int fftSize, std::vector<float>& psdOut)
{
    LearnAnalysis result;
    psdOut.assign ((size_t) numBins, 0.0f);
    if (numFrames <= 0 || numBins <= 0) return result;

    const double binHz = sampleRate / fftSize;

    // ---- 1. Detection sub-bands: log spaced from ~90 Hz, at least 8 bins wide
    std::vector<int> edges;
    {
        const int minWidth = 8;
        int start = std::max (1, (int) std::round (90.0 / binHz));
        edges.push_back (start);
        const double top = std::min (sampleRate * 0.5 * 0.95, 20000.0);
        const int topBin = std::min (numBins - 1, (int) std::round (top / binHz));
        const int bandsWanted = 10;
        const double ratio = std::pow ((double) topBin / start, 1.0 / bandsWanted);
        double edge = start;
        while (edges.back() < topBin)
        {
            edge *= ratio;
            int next = std::max (edges.back() + minWidth, (int) std::round (edge));
            next = std::min (next, topBin);
            if (topBin - next < minWidth / 2) next = topBin;
            edges.push_back (next);
        }
    }
    const int numBands = (int) edges.size() - 1;
    if (numBands <= 0) return result;

    std::vector<double> bandEnergy ((size_t) numFrames * numBands, 0.0);
    double totalPower = 0.0;
    for (int f = 0; f < numFrames; ++f)
    {
        const float* row = frames + (size_t) f * numBins;
        for (int k = 0; k < numBins; ++k) totalPower += row[k];
        for (int b = 0; b < numBands; ++b)
        {
            double s = 0.0;
            for (int k = edges[(size_t) b]; k < edges[(size_t) b + 1]; ++k) s += row[k];
            bandEnergy[(size_t) f * numBands + b] = s;
        }
    }

    // digital silence / muted microphone?  (mean per-bin power below -150 dB)
    if (totalPower / ((double) numFrames * numBins) < 1.0e-15)
    {
        result.outcome = LearnOutcome::noSignal;
        return result;
    }

    // ---- 2. Per band: minimum-statistics reference and a Gamma-model threshold.
    // The band power of stationary noise is ~Gamma distributed with
    // d = bins/2 degrees of freedom (Hann leakage correlates neighbouring bins).
    // The reference is the 10th percentile, so the threshold stays anchored to
    // the noise floor even when someone talks through most of the capture.
    // Quantiles via the Wilson-Hilferty approximation.
    auto gammaQuantile = [] (double d, double z)
    {
        const double a = 1.0 / (9.0 * d);
        const double t = 1.0 - a + z * std::sqrt (a);
        return std::max (1.0e-6, t * t * t);
    };
    std::vector<char> active ((size_t) numFrames, 0);
    {
        std::vector<double> tmp ((size_t) numFrames);
        for (int b = 0; b < numBands; ++b)
        {
            for (int f = 0; f < numFrames; ++f) tmp[(size_t) f] = bandEnergy[(size_t) f * numBands + b];
            const size_t q10 = (size_t) (0.10 * (numFrames - 1));
            std::nth_element (tmp.begin(), tmp.begin() + (long) q10, tmp.end());
            const double ref = tmp[q10];
            const double d = std::max (1.0, 0.5 * (edges[(size_t) b + 1] - edges[(size_t) b]));
            const double ratio = gammaQuantile (d, 3.2905) / gammaQuantile (d, -1.2816);   // Q(0.9995) / Q(0.10)
            const double thr = ref * ratio;
            for (int f = 0; f < numFrames; ++f)
                if (bandEnergy[(size_t) f * numBands + b] > thr) active[(size_t) f] = 1;
        }
    }

    // ---- 3. Dilate activity by +-3 frames (onsets, decays, breath tails)
    std::vector<char> reject ((size_t) numFrames, 0);
    const int dilate = 3;
    for (int f = 0; f < numFrames; ++f)
        if (active[(size_t) f])
            for (int d = -dilate; d <= dilate; ++d)
            {
                const int g = f + d;
                if (g >= 0 && g < numFrames) reject[(size_t) g] = 1;
            }

    int accepted = 0;
    std::vector<double> acc ((size_t) numBins, 0.0);
    for (int f = 0; f < numFrames; ++f)
    {
        if (reject[(size_t) f]) continue;
        ++accepted;
        const float* row = frames + (size_t) f * numBins;
        for (int k = 0; k < numBins; ++k) acc[(size_t) k] += row[k];
    }

    result.acceptedFrames = accepted;
    result.acceptedFraction = (float) accepted / (float) numFrames;

    if (result.acceptedFraction < 0.35f || accepted < 20)
    {
        result.outcome = LearnOutcome::tooMuchActivity;
        return result;
    }

    for (int k = 0; k < numBins; ++k)
        psdOut[(size_t) k] = (float) (acc[(size_t) k] / accepted);

    result.outcome = LearnOutcome::ok;
    return result;
}

} // namespace deadnoise
