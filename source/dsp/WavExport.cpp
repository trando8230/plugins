#include "WavExport.h"

#include <cstring>

namespace grokfx
{
namespace
{

void writeFour(juce::OutputStream& stream, const char* text)
{
    stream.write(text, 4);
}

void write16(juce::OutputStream& stream, uint16_t value)
{
    stream.writeShort(static_cast<short>(value));
}

void write32(juce::OutputStream& stream, uint32_t value)
{
    stream.writeInt(static_cast<int>(value));
}

} // namespace

WavFormat wavFormatForChoice(int exportBits) noexcept
{
    if (exportBits <= 0)
        return WavFormat::pcm16;
    if (exportBits == 1)
        return WavFormat::pcm24;
    return WavFormat::float32;
}

double sampleRateForChoice(int exportRate, double hostSampleRate) noexcept
{
    switch (exportRate)
    {
        case 1: return 44100.0;
        case 2: return 48000.0;
        case 3: return 96000.0;
        default: return sanitizeSampleRate(hostSampleRate);
    }
}

bool writeStereoWav(const juce::File& file, const juce::AudioBuffer<float>& buffer,
                    double sampleRate, WavFormat format)
{
    file.getParentDirectory().createDirectory();
    if (file.existsAsFile())
        file.deleteFile();

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return false;

    const int frames = std::max(0, buffer.getNumSamples());
    const int bits = format == WavFormat::pcm16 ? 16 : (format == WavFormat::pcm24 ? 24 : 32);
    const int bytesPerFrame = format == WavFormat::pcm16 ? 4 : (format == WavFormat::pcm24 ? 6 : 8);
    const uint16_t audioFormat = format == WavFormat::float32 ? 3 : 1;
    const bool fact = format == WavFormat::float32;
    const uint32_t fmtChunk = 16;
    const uint32_t factChunk = fact ? 12 : 0;
    const uint32_t dataBytes = static_cast<uint32_t>(frames * bytesPerFrame);
    const uint32_t riffSize = 4 + (8 + fmtChunk) + factChunk + 8 + dataBytes;
    const auto rate = static_cast<uint32_t>(std::llround(sanitizeSampleRate(sampleRate)));

    writeFour(stream, "RIFF");
    write32(stream, riffSize);
    writeFour(stream, "WAVE");
    writeFour(stream, "fmt ");
    write32(stream, fmtChunk);
    write16(stream, audioFormat);
    write16(stream, 2);
    write32(stream, rate);
    write32(stream, rate * static_cast<uint32_t>(bytesPerFrame));
    write16(stream, static_cast<uint16_t>(bytesPerFrame));
    write16(stream, static_cast<uint16_t>(bits));

    if (fact)
    {
        writeFour(stream, "fact");
        write32(stream, 4);
        write32(stream, static_cast<uint32_t>(frames));
    }

    writeFour(stream, "data");
    write32(stream, dataBytes);

    auto sampleAt = [&](int channel, int frame) {
        if (channel >= buffer.getNumChannels() || frame >= buffer.getNumSamples())
            return 0.0f;
        const float value = buffer.getSample(channel, frame);
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, -1.0f, 1.0f);
    };

    for (int frame = 0; frame < frames; ++frame)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const float value = sampleAt(channel, frame);
            if (format == WavFormat::pcm16)
            {
                const auto packed = static_cast<int16_t>(std::lrintf(value * 32767.0f));
                stream.writeShort(packed);
            }
            else if (format == WavFormat::pcm24)
            {
                const auto packed = static_cast<int32_t>(std::lrintf(value * 8388607.0f));
                char bytes[3] = {
                    static_cast<char>(packed & 0xff),
                    static_cast<char>((packed >> 8) & 0xff),
                    static_cast<char>((packed >> 16) & 0xff)
                };
                stream.write(bytes, 3);
            }
            else
            {
                static_assert(sizeof(float) == 4, "float WAV expects 32-bit float");
                stream.write(&value, sizeof(float));
            }
        }
    }

    stream.flush();
    return stream.getStatus().wasOk();
}

} // namespace grokfx
