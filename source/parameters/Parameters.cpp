#include "Parameters.h"

namespace grokfx
{
namespace
{

juce::String layerName(int index, const juce::String& label)
{
    return "Layer " + juce::String(index + 1) + " " + label;
}

void addFloat(juce::AudioProcessorParameterGroup& group, const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float value, const char* unit)
{
    group.addChild(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, range, value,
        juce::AudioParameterFloatAttributes().withLabel(unit)));
}

void addChoice(juce::AudioProcessorParameterGroup& group, const juce::String& id, const juce::String& name,
               const juce::StringArray& choices, int value)
{
    group.addChild(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id, 1 }, name, choices, value));
}

void addBool(juce::AudioProcessorParameterGroup& group, const juce::String& id, const juce::String& name, bool value)
{
    group.addChild(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id, 1 }, name, value));
}

void addInt(juce::AudioProcessorParameterGroup& group, const juce::String& id, const juce::String& name,
            int min, int max, int value, const char* unit)
{
    group.addChild(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { id, 1 }, name, min, max, value,
        juce::AudioParameterIntAttributes().withLabel(unit)));
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::StringArray types { "Riser", "Downlifter", "Impact", "Snare roll",
                                    "White-noise sweep", "Reverse cymbal", "Sub drop" };
    const juce::StringArray banks { "Classic", "Analog", "Digital", "Vocal" };
    const juce::StringArray curves { "Linear", "Exponential", "Logarithmic", "S-curve" };
    const juce::StringArray filters { "Off", "Low-pass", "High-pass", "Band-pass" };
    const juce::StringArray divisions { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const int defaultTypes[4] = { 0, 2, 3, 6 };

    for (int layer = 0; layer < 4; ++layer)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(
            "layer" + juce::String(layer + 1), "Layer " + juce::String(layer + 1), "|");

        addBool(*group, layerParam(layer, "enabled"), layerName(layer, "On"), layer == 0);
        addChoice(*group, layerParam(layer, "type"), layerName(layer, "Type"), types, defaultTypes[layer]);
        addChoice(*group, layerParam(layer, "bank"), layerName(layer, "Bank"), banks, 0);
        addFloat(*group, layerParam(layer, "volume"), layerName(layer, "Volume"), Range(-60.0f, 12.0f), -6.0f, "dB");
        addFloat(*group, layerParam(layer, "pan"), layerName(layer, "Pan"), Range(-1.0f, 1.0f), 0.0f, "%");
        addInt(*group, layerParam(layer, "pitch"), layerName(layer, "Pitch"), -24, 24, 0, "st");
        addFloat(*group, layerParam(layer, "fine"), layerName(layer, "Fine"), Range(-100.0f, 100.0f), 0.0f, "ct");
        addFloat(*group, layerParam(layer, "offset"), layerName(layer, "Start"), Range(0.0f, 0.9f), 0.0f, "%");
        addFloat(*group, layerParam(layer, "fadeIn"), layerName(layer, "Fade In"), Range(0.0f, 1.0f), 0.0f, "%");
        addChoice(*group, layerParam(layer, "fadeInCurve"), layerName(layer, "Fade In Curve"), curves, 0);
        addFloat(*group, layerParam(layer, "fadeOut"), layerName(layer, "Fade Out"), Range(0.0f, 1.0f), 0.0f, "%");
        addChoice(*group, layerParam(layer, "fadeOutCurve"), layerName(layer, "Fade Out Curve"), curves, 3);
        addBool(*group, layerParam(layer, "reverse"), layerName(layer, "Reverse"), false);
        addBool(*group, layerParam(layer, "solo"), layerName(layer, "Solo"), false);
        addBool(*group, layerParam(layer, "mute"), layerName(layer, "Mute"), false);
        addChoice(*group, layerParam(layer, "filterType"), layerName(layer, "Filter"), filters, 0);
        addFloat(*group, layerParam(layer, "cutoffStart"), layerName(layer, "Filter Start"),
                 Range(20.0f, 20000.0f, 0.0f, 0.3f), 180.0f, "Hz");
        addFloat(*group, layerParam(layer, "cutoffEnd"), layerName(layer, "Filter End"),
                 Range(20.0f, 20000.0f, 0.0f, 0.3f), 9000.0f, "Hz");
        addFloat(*group, layerParam(layer, "resonance"), layerName(layer, "Resonance"), Range(0.0f, 1.0f), 0.2f, "%");
        addFloat(*group, layerParam(layer, "drive"), layerName(layer, "Drive"), Range(0.0f, 1.0f), 0.15f, "%");
        addFloat(*group, layerParam(layer, "width"), layerName(layer, "Width"), Range(0.0f, 1.0f), 0.5f, "%");
        addFloat(*group, layerParam(layer, "reverbSend"), layerName(layer, "Reverb"), Range(0.0f, 1.0f), 0.18f, "%");
        addFloat(*group, layerParam(layer, "delaySend"), layerName(layer, "Delay"), Range(0.0f, 1.0f), 0.08f, "%");
        addBool(*group, layerParam(layer, "locked"), layerName(layer, "Lock"), false);
        addChoice(*group, layerParam(layer, "shape"), layerName(layer, "Shape"), curves, 1);
        addChoice(*group, layerParam(layer, "rollStart"), layerName(layer, "Roll Start"), divisions, 0);
        addChoice(*group, layerParam(layer, "rollEnd"), layerName(layer, "Roll End"), divisions, 5);
        addChoice(*group, layerParam(layer, "rollAccel"), layerName(layer, "Roll Acceleration"), curves, 1);
        addFloat(*group, layerParam(layer, "swing"), layerName(layer, "Swing"), Range(0.0f, 1.0f), 0.0f, "%");
        addFloat(*group, layerParam(layer, "velStart"), layerName(layer, "Velocity Start"), Range(0.0f, 1.0f), 0.35f, "%");
        addFloat(*group, layerParam(layer, "velEnd"), layerName(layer, "Velocity End"), Range(0.0f, 1.0f), 1.0f, "%");
        addChoice(*group, layerParam(layer, "velCurve"), layerName(layer, "Velocity Curve"), curves, 1);
        addInt(*group, layerParam(layer, "seed"), layerName(layer, "Variation"), 1, 999983, 1000 + layer * 997, "seed");

        layout.add(std::move(group));
    }

    auto song = std::make_unique<juce::AudioProcessorParameterGroup>("song", "Song", "|");
    addChoice(*song, "keyRoot", "Key",
              { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    addChoice(*song, "keyMode", "Scale", { "Major", "Minor" }, 0);
    addChoice(*song, "length", "Length",
              { "1 beat", "2 beats", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 4);
    addFloat(*song, "character", "Tone", Range(0.0f, 1.0f), 0.5f, "%");
    layout.add(std::move(song));

    auto macros = std::make_unique<juce::AudioProcessorParameterGroup>("macros", "Macros", "|");
    addFloat(*macros, "intensity", "Intensity", Range(0.0f, 1.0f), 0.5f, "%");
    addFloat(*macros, "space", "Space", Range(0.0f, 1.0f), 0.5f, "%");
    addFloat(*macros, "brightness", "Brightness", Range(0.0f, 1.0f), 0.5f, "%");
    addFloat(*macros, "tension", "Tension", Range(0.0f, 1.0f), 0.5f, "%");
    addFloat(*macros, "randomStrength", "Randomize Strength", Range(0.0f, 1.0f), 1.0f, "%");
    layout.add(std::move(macros));

    auto master = std::make_unique<juce::AudioProcessorParameterGroup>("master", "Master", "|");
    addFloat(*master, "masterWidth", "Master Width", Range(0.0f, 1.0f), 0.85f, "%");
    addFloat(*master, "reverbAmount", "Reverb Amount", Range(0.0f, 1.0f), 0.35f, "%");
    addFloat(*master, "reverbSize", "Reverb Size", Range(0.0f, 1.0f), 0.45f, "%");
    addFloat(*master, "delayAmount", "Delay Amount", Range(0.0f, 1.0f), 0.22f, "%");
    addFloat(*master, "delayFeedback", "Delay Feedback", Range(0.0f, 0.85f), 0.32f, "%");
    addChoice(*master, "delayDivision", "Delay Time", { "1/2", "1/4", "1/8", "1/8d", "1/16" }, 2);
    addChoice(*master, "delayStyle", "Delay Rhythm", { "Straight", "Dotted", "Triplet" }, 0);
    addChoice(*master, "masterFilter", "Master Filter", filters, 0);
    addFloat(*master, "masterCutoff", "Master Cutoff", Range(20.0f, 20000.0f, 0.0f, 0.3f), 14000.0f, "Hz");
    addFloat(*master, "masterResonance", "Master Resonance", Range(0.0f, 1.0f), 0.12f, "%");
    addBool(*master, "limiterOn", "Limiter", true);
    addFloat(*master, "limiterCeiling", "Limiter Ceiling", Range(-24.0f, 0.0f), -0.3f, "dB");
    addFloat(*master, "masterGain", "Master Gain", Range(-24.0f, 12.0f), 0.0f, "dB");
    addFloat(*master, "tailBeats", "Tail", Range(0.0f, 8.0f), 0.0f, "beats");
    addBool(*master, "followHost", "Follow Host", false);
    addBool(*master, "preview", "Preview", false);
    layout.add(std::move(master));

    auto exportGroup = std::make_unique<juce::AudioProcessorParameterGroup>("export", "Export", "|");
    addChoice(*exportGroup, "exportBits", "Export Bit Depth", { "16-bit", "24-bit", "32-bit float" }, 1);
    addChoice(*exportGroup, "exportRate", "Export Sample Rate", { "Host", "44.1 kHz", "48 kHz", "96 kHz" }, 0);
    addBool(*exportGroup, "exportStems", "Export Stems", false);
    addFloat(*exportGroup, "trimStart", "Export Start", Range(0.0f, 1.0f), 0.0f, "%");
    addFloat(*exportGroup, "trimEnd", "Export End", Range(0.0f, 1.0f), 1.0f, "%");
    layout.add(std::move(exportGroup));

    return layout;
}

} // namespace grokfx
