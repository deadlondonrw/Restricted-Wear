#include "StateIO.h"

namespace dw::stateio
{
juce::String encodeFloats (const float* data, size_t count)
{
    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream gz (compressed, 9);
        gz.write (data, count * sizeof (float));
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
    if (! v.empty()) std::memcpy (v.data(), out.getData(), v.size() * sizeof (float));
    for (auto& x : v)
        if (! std::isfinite (x)) x = 0.0f;
    return v;
}

juce::ValueTree sampleToTree (const SampleAsset& s)
{
    juce::ValueTree v ("SAMPLE");
    v.setProperty ("name", s.name, nullptr);
    v.setProperty ("rate", s.sourceRate, nullptr);
    v.setProperty ("root", s.rootNote, nullptr);
    v.setProperty ("channels", s.numChannels, nullptr);
    v.setProperty ("length", s.length, nullptr);
    v.setProperty ("loopStart", s.loopStart, nullptr);
    v.setProperty ("loopEnd", s.loopEnd, nullptr);
    v.setProperty ("loop", s.loop, nullptr);
    for (int c = 0; c < s.numChannels; ++c)
        v.setProperty ("ch" + juce::String (c), encodeFloats (s.data[0][c].data(), (size_t) s.length), nullptr);
    return v;
}

std::unique_ptr<SampleAsset> sampleFromTree (const juce::ValueTree& v)
{
    if (! v.isValid() || ! v.hasType ("SAMPLE")) return nullptr;
    auto s = std::make_unique<SampleAsset>();
    s->name = v["name"].toString();
    s->sourceRate = juce::jlimit (1000.0, 384000.0, (double) v.getProperty ("rate", 44100.0));
    s->rootNote = (float) v.getProperty ("root", 60.0);
    s->numChannels = juce::jlimit (1, 2, (int) v.getProperty ("channels", 1));
    s->loop = (bool) v.getProperty ("loop", false);
    int len = 0;
    for (int c = 0; c < s->numChannels; ++c)
    {
        auto d = decodeFloats (v["ch" + juce::String (c)].toString());
        len = c == 0 ? (int) d.size() : juce::jmin (len, (int) d.size());
        d.resize (d.size() + 4, 0.0f);
        s->data[0][c] = std::move (d);
    }
    if (len < 2) return nullptr;
    s->length = len;
    s->loopStart = juce::jlimit (0, len - 1, (int) v.getProperty ("loopStart", 0));
    s->loopEnd = juce::jlimit (s->loopStart, len, (int) v.getProperty ("loopEnd", len));
    s->buildLevels();
    return s;
}

LfoPoints defaultLfoPoints()
{
    return { { 0.0f, 0.0f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.5f, 0.35f, 0.0f }, { 0.75f, 0.8f, 0.0f } };
}

std::unique_ptr<LfoShapeTable> renderLfo (const LfoPoints& pts)
{
    auto t = std::make_unique<LfoShapeTable>();
    if (pts.empty()) return t;
    auto sorted = pts;
    std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
    const int n = (int) sorted.size();
    for (int i = 0; i <= LfoShapeTable::kSize; ++i)
    {
        const float x = (float) (i % LfoShapeTable::kSize) / (float) LfoShapeTable::kSize;
        int a = n - 1;
        for (int k = 0; k < n; ++k)
            if (sorted[(size_t) k].x <= x) a = k;
        const int b = (a + 1) % n;
        float xa = sorted[(size_t) a].x, xb = sorted[(size_t) b].x, xx = x;
        if (b <= a) { xb += 1.0f; if (xx < xa) xx += 1.0f; }
        const float u = juce::jlimit (0.0f, 1.0f, (xx - xa) / juce::jmax (1.0e-6f, xb - xa));
        const float y = sorted[(size_t) a].y + (sorted[(size_t) b].y - sorted[(size_t) a].y) * Envelope::curve (u, sorted[(size_t) a].curve);
        t->v[(size_t) i] = juce::jlimit (0.0f, 1.0f, y);
    }
    return t;
}

juce::String lfoPointsToString (const LfoPoints& pts)
{
    juce::String s;
    for (const auto& p : pts)
        s << juce::String (p.x, 5) << "," << juce::String (p.y, 5) << "," << juce::String (p.curve, 4) << ";";
    return s;
}

LfoPoints lfoPointsFromString (const juce::String& s)
{
    LfoPoints pts;
    for (const auto& tok : juce::StringArray::fromTokens (s, ";", ""))
    {
        const auto parts = juce::StringArray::fromTokens (tok, ",", "");
        if (parts.size() == 3)
            pts.push_back ({ juce::jlimit (0.0f, 1.0f, parts[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, parts[1].getFloatValue()),
                             juce::jlimit (-1.0f, 1.0f, parts[2].getFloatValue()) });
    }
    if (pts.size() < 2) pts = defaultLfoPoints();
    return pts;
}

// ====================================================================================== wavetable WAV
bool writeWavetableWav (const juce::File& file, const wavegen::Frames& frames)
{
    if (frames.empty()) return false;
    file.deleteFile();
    const int frameSize = kWaveSize;
    const int total = frameSize * (int) frames.size();

    juce::MemoryOutputStream data;
    for (const auto& f : frames)
        for (int i = 0; i < frameSize; ++i)
            data.writeFloat (i < (int) f.size() ? f[(size_t) i] : 0.0f);

    const juce::String clmText = "<!>" + juce::String (frameSize) + " 10000000 wavetable (DEADWAVE)";
    juce::MemoryBlock clm (clmText.toRawUTF8(), (size_t) clmText.getNumBytesAsUTF8());
    if (clm.getSize() % 2) clm.append ("\0", 1);

    juce::FileOutputStream out (file);
    if (! out.openedOk()) return false;
    const uint32_t fmtSize = 16;
    const uint32_t dataSize = (uint32_t) total * 4;
    const uint32_t riffSize = 4 + (8 + fmtSize) + (8 + (uint32_t) clm.getSize()) + (8 + dataSize);
    out.write ("RIFF", 4); out.writeInt ((int) riffSize); out.write ("WAVE", 4);
    out.write ("fmt ", 4); out.writeInt ((int) fmtSize);
    out.writeShort (3);          // IEEE float
    out.writeShort (1);          // mono
    out.writeInt (44100);
    out.writeInt (44100 * 4);
    out.writeShort (4);
    out.writeShort (32);
    out.write ("clm ", 4); out.writeInt ((int) clm.getSize()); out.write (clm.getData(), clm.getSize());
    out.write ("data", 4); out.writeInt ((int) dataSize); out.write (data.getData(), data.getDataSize());
    out.flush();
    return out.getStatus().wasOk();
}

bool readWavetableWav (const juce::File& file, wavegen::Frames& framesOut, juce::String& error)
{
    framesOut.clear();
    // Look for a clm chunk to learn the frame size.
    int frameSize = kWaveSize;
    {
        juce::FileInputStream in (file);
        if (! in.openedOk()) { error = "Cannot open file"; return false; }
        char id[5] {};
        in.read (id, 4);
        if (juce::String (id) != "RIFF") { error = "Not a WAV file"; return false; }
        in.readInt();
        in.read (id, 4);
        while (! in.isExhausted())
        {
            if (in.read (id, 4) != 4) break;
            const int size = in.readInt();
            if (size < 0) break;
            if (juce::String (id, 4) == "clm ")
            {
                juce::MemoryBlock mb;
                in.readIntoMemoryBlock (mb, size);
                const auto text = mb.toString();
                const auto num = text.fromFirstOccurrenceOf ("<!>", false, false).upToFirstOccurrenceOf (" ", false, false).getIntValue();
                if (num >= 64 && num <= 8192) frameSize = num;
                break;
            }
            in.skipNextBytes (size + (size & 1));
        }
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported audio file"; return false; }
    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) frameSize * kMaxFrames);
    juce::AudioBuffer<float> buf (1, juce::jmax (1, len));
    reader->read (&buf, 0, len, 0, true, false);
    const int numFrames = juce::jmax (1, len / frameSize);
    for (int f = 0; f < numFrames; ++f)
    {
        wavegen::Frame fr ((size_t) kWaveSize);
        for (int i = 0; i < kWaveSize; ++i)
        {
            const double pos = (double) i * frameSize / kWaveSize;
            const int k = (int) pos;
            const float t = (float) (pos - k);
            const int a = f * frameSize + juce::jmin (k, frameSize - 1);
            const int b = f * frameSize + (k + 1) % frameSize;
            const float va = a < len ? buf.getSample (0, a) : 0.0f;
            const float vb = b < len ? buf.getSample (0, b) : 0.0f;
            fr[(size_t) i] = va + (vb - va) * t;
        }
        framesOut.push_back (std::move (fr));
    }
    return true;
}
} // namespace dw::stateio
