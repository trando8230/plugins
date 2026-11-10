#pragma once

#include "Model.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace grokfx
{

enum class WavFormat
{
    pcm16,
    pcm24,
    float32
};

bool writeStereoWav(const juce::File& file, const juce::AudioBuffer<float>& buffer,
                    double sampleRate, WavFormat format);

WavFormat wavFormatForChoice(int exportBits) noexcept;
double sampleRateForChoice(int exportRate, double hostSampleRate) noexcept;

} // namespace grokfx
