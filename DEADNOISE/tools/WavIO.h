// Minimal WAV reader/writer for the DEADNOISE test tools (PCM16/24/32, float32).
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wavio
{

struct Audio
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> ch;   // ch[c][n]
    size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

inline bool read (const std::string& path, Audio& out)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) return false;
    std::vector<uint8_t> d;
    {
        uint8_t buf[65536];
        size_t r;
        while ((r = std::fread (buf, 1, sizeof (buf), f)) > 0) d.insert (d.end(), buf, buf + r);
        std::fclose (f);
    }
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;
    size_t pos = 12;
    int fmtTag = 0, nch = 0, bits = 0;
    uint32_t sr = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;
    while (pos + 8 <= d.size())
    {
        uint32_t sz;
        std::memcpy (&sz, d.data() + pos + 4, 4);
        const uint8_t* body = d.data() + pos + 8;
        if (! std::memcmp (d.data() + pos, "fmt ", 4))
        {
            uint16_t t, c, b;
            std::memcpy (&t, body, 2); std::memcpy (&c, body + 2, 2);
            std::memcpy (&sr, body + 4, 4); std::memcpy (&b, body + 14, 2);
            fmtTag = t; nch = c; bits = b;
            if (fmtTag == 0xFFFE && sz >= 26) { uint16_t sub; std::memcpy (&sub, body + 24, 2); fmtTag = sub; }
        }
        else if (! std::memcmp (d.data() + pos, "data", 4))
        {
            data = body;
            dataSize = std::min ((size_t) sz, d.size() - (pos + 8));
        }
        pos += 8 + sz + (sz & 1);
    }
    if (! data || nch <= 0) return false;
    const int bps = bits / 8;
    const size_t frames = dataSize / (size_t) (bps * nch);
    out.sampleRate = sr;
    out.ch.assign ((size_t) nch, std::vector<float> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < nch; ++c)
        {
            const uint8_t* s = data + (i * (size_t) nch + (size_t) c) * (size_t) bps;
            float v = 0.0f;
            if (fmtTag == 3 && bits == 32) std::memcpy (&v, s, 4);
            else if (bits == 16) { int16_t x; std::memcpy (&x, s, 2); v = x / 32768.0f; }
            else if (bits == 24) { int32_t x = (s[0] << 8) | (s[1] << 16) | (s[2] << 24); v = (float) (x / 2147483648.0); }
            else if (bits == 32) { int32_t x; std::memcpy (&x, s, 4); v = (float) (x / 2147483648.0); }
            out.ch[(size_t) c][i] = v;
        }
    return true;
}

inline bool writeFloat (const std::string& path, const Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint16_t nch = (uint16_t) a.ch.size();
    const uint32_t frames = (uint32_t) a.frames();
    const uint32_t sr = (uint32_t) a.sampleRate;
    const uint32_t dataSize = frames * nch * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataSize); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (nch); w32 (sr); w32 (sr * nch * 4); w16 ((uint16_t) (nch * 4)); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataSize);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint16_t c = 0; c < nch; ++c) std::fwrite (&a.ch[c][i], 4, 1, f);
    std::fclose (f);
    return true;
}

} // namespace wavio
