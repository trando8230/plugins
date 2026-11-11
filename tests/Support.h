#pragma once

#include "PluginProcessor.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

inline std::unique_ptr<GrokFxAudioProcessor> makePlugin()
{
    auto plugin = std::unique_ptr<GrokFxAudioProcessor>(
        dynamic_cast<GrokFxAudioProcessor*>(createPluginFilter()));
    REQUIRE(plugin != nullptr);
    return plugin;
}

inline void setReal(GrokFxAudioProcessor& plugin, const juce::String& id, float value)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(plugin.apvts.getParameter(id));
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

inline float getReal(GrokFxAudioProcessor& plugin, const juce::String& id)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(plugin.apvts.getParameter(id));
    REQUIRE(parameter != nullptr);
    return parameter->convertFrom0to1(parameter->getValue());
}

inline void setNormalized(GrokFxAudioProcessor& plugin, const juce::String& id, float normalized)
{
    auto* parameter = plugin.apvts.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalized));
}

struct HostPlayHead : juce::AudioPlayHead
{
    juce::AudioPlayHead::PositionInfo info;
    bool report = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        if (!report)
            return {};
        return info;
    }
};

inline void expectFiniteBuffer(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float value = buffer.getSample(channel, sample);
            REQUIRE(std::isfinite(value));
            REQUIRE(std::abs(value) <= 1.0f);
        }
}

inline float maxAbs(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
    return peak;
}
