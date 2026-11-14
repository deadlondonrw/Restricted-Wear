#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>

namespace dw
{
// ============================================================================================ math
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }
inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }
inline float semitonesToRatio (float st) noexcept { return std::exp2 (st * (1.0f / 12.0f)); }

/** Pade-style tanh approximation, accurate to ~1e-4 in [-3, 3] and clamped beyond. */
inline float fastTanh (float x) noexcept
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

inline float softClip (float x) noexcept { return fastTanh (x); }

inline float wrap01 (float x) noexcept { return x - std::floor (x); }
inline double wrap01 (double x) noexcept { return x - std::floor (x); }

inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Hermite 4-point, 3rd order interpolation. */
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float bNeg = w + a;
    return ((((a * t) - bNeg) * t + c) * t + x0);
}

/** Flush tiny values to zero (denormal protection for feedback paths). */
inline float undenormalise (float x) noexcept { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

// ============================================================================================ rng
/** Fast, allocation-free xorshift RNG usable on the audio thread. */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) noexcept : s (seed ? seed : 1u) {}
    void seed (uint32_t v) noexcept { s = v ? v : 1u; }
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0, 1)
    float bi() noexcept { return uni() * 2.0f - 1.0f; }                             // [-1, 1)
};

// ================================================================================== smoothing
/** One-pole parameter smoother with configurable time constant. */
struct Smoother
{
    float value = 0.0f, coeff = 0.0f;
    void setTime (double sampleRate, float seconds) noexcept
    {
        coeff = seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (sampleRate * (double) seconds));
    }
    void reset (float v) noexcept { value = v; }
    float process (float target) noexcept { value = target + (value - target) * coeff; return value; }
};

// ====================================================================================== DC block
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void prepare (double sampleRate, float cutoffHz = 5.0f) noexcept
    {
        r = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * (double) cutoffHz / sampleRate);
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = undenormalise (y);
        return y1;
    }
};

// ============================================================================ lock-free exchange
/**
    Single-producer (message thread) / single-consumer (audio thread) object exchange.

    The message thread publishes a new heap object; the audio thread picks it up at the start of a
    block without allocating or freeing. Retired objects are handed back through a fixed-size ring and
    deleted on the message thread by collectGarbage().
*/
template <typename T>
class ObjectExchange
{
public:
    ObjectExchange() = default;
    ~ObjectExchange()
    {
        delete pending.exchange (nullptr);
        delete current;
        collectGarbageLocked();
    }

    /** Message thread. Takes ownership. Returns false if the retire ring is full (try again later). */
    void publish (std::unique_ptr<T> obj)
    {
        std::lock_guard<std::mutex> lock (producerLock);
        collectGarbageLocked();
        delete pending.exchange (obj.release(), std::memory_order_acq_rel);
    }

    /** Audio thread. Returns the active object (may be null). */
    T* acquire() noexcept
    {
        if (auto* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (current != nullptr)
                retire (current);
            current = p;
        }
        return current;
    }

    /** Audio thread (or when the audio thread is stopped). */
    T* get() const noexcept { return current; }

    /** Message thread (any non-audio thread): frees objects the audio thread has retired. */
    void collectGarbage()
    {
        std::lock_guard<std::mutex> lock (producerLock);
        collectGarbageLocked();
    }

    /** Message thread, only when the audio thread is guaranteed not to run (e.g. before prepareToPlay). */
    void setImmediately (std::unique_ptr<T> obj)
    {
        std::lock_guard<std::mutex> lock (producerLock);
        delete pending.exchange (nullptr);
        delete current;
        current = obj.release();
    }

private:
    void collectGarbageLocked()
    {
        for (;;)
        {
            const auto r = readPos.load (std::memory_order_relaxed);
            if (r == writePos.load (std::memory_order_acquire))
                break;
            delete ring[r % kRing];
            ring[r % kRing] = nullptr;
            readPos.store (r + 1, std::memory_order_release);
        }
    }

    void retire (T* p) noexcept
    {
        const auto w = writePos.load (std::memory_order_relaxed);
        if (w - readPos.load (std::memory_order_acquire) >= kRing)
        {
            // Ring full: leak deliberately rather than free on the audio thread. Practically
            // unreachable because the message thread drains the ring at 30 Hz.
            return;
        }
        ring[w % kRing] = p;
        writePos.store (w + 1, std::memory_order_release);
    }

    static constexpr uint32_t kRing = 64;
    std::atomic<T*> pending { nullptr };
    T* current = nullptr;
    T* ring[kRing] {};
    std::atomic<uint32_t> writePos { 0 }, readPos { 0 };
    std::mutex producerLock;
};
} // namespace dw
