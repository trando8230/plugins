#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace grokfx
{

inline juce::String layerParam(int index, const char* suffix)
{
    return "l" + juce::String(index + 1) + "_" + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

inline constexpr const char* randomizableSuffixes[] = {
    "type", "bank", "volume", "pan", "pitch", "fine", "offset",
    "fadeIn", "fadeInCurve", "fadeOut", "fadeOutCurve", "reverse",
    "filterType", "cutoffStart", "cutoffEnd", "resonance", "drive",
    "width", "reverbSend", "delaySend", "shape", "rollStart", "rollEnd",
    "rollAccel", "swing", "velStart", "velEnd", "velCurve"
};

} // namespace grokfx
