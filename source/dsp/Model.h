#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace grokfx
{

inline constexpr int layerCount = 4;
inline constexpr int waveformColumns = 512;

enum class FxType : int
{
    riser = 0,
    downlifter,
    impact,
    snareRoll,
    noiseSweep,
    reverseCymbal,
    subDrop,
    count
};

enum class Bank : int
{
    classic = 0,
    analog,
    digital,
    vocal,
    count
};

enum class Length : int
{
    oneBeat = 0,
    twoBeats,
    oneBar,
    twoBars,
    fourBars,
    eightBars,
    sixteenBars,
    count
};

enum class Curve : int
{
    linear = 0,
    exponential,
    logarithmic,
    sCurve,
    count
};

enum class FilterType : int
{
    off = 0,
    lowPass,
    highPass,
    bandPass,
    count
};

enum class Division : int
{
    quarter = 0,
    eighth,
    eighthTriplet,
    sixteenth,
    sixteenthTriplet,
    thirtySecond,
    count
};

enum class DelayDivision : int
{
    half = 0,
    quarter,
    eighth,
    eighthDotted,
    sixteenth,
    count
};

inline constexpr const char* fxTypeName(int type)
{
    switch (type)
    {
        case 0: return "Riser";
        case 1: return "Downlifter";
        case 2: return "Impact";
        case 3: return "Snare roll";
        case 4: return "White-noise sweep";
        case 5: return "Reverse cymbal";
        case 6: return "Sub drop";
        default: return "Riser";
    }
}

inline constexpr const char* fxTypeToken(int type)
{
    switch (type)
    {
        case 0: return "Riser";
        case 1: return "Downlifter";
        case 2: return "Impact";
        case 3: return "SnareRoll";
        case 4: return "NoiseSweep";
        case 5: return "ReverseCymbal";
        case 6: return "SubDrop";
        default: return "Riser";
    }
}

inline constexpr const char* bankName(int bank)
{
    switch (bank)
    {
        case 0: return "Classic";
        case 1: return "Analog";
        case 2: return "Digital";
        case 3: return "Vocal";
        default: return "Classic";
    }
}

// Shared by the engine and the pitch tests. MIDI 69 is A4 at 440 Hz.
inline float noteToHz(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

inline constexpr const char* noteName(int root)
{
    switch (root & 11)
    {
        case 0: return "C";
        case 1: return "C#";
        case 2: return "D";
        case 3: return "D#";
        case 4: return "E";
        case 5: return "F";
        case 6: return "F#";
        case 7: return "G";
        case 8: return "G#";
        case 9: return "A";
        case 10: return "A#";
        default: return "B";
    }
}

inline constexpr const char* lengthToken(int length)
{
    switch (length)
    {
        case 0: return "1beat";
        case 1: return "2beats";
        case 2: return "1bar";
        case 3: return "2bars";
        case 4: return "4bars";
        case 5: return "8bars";
        default: return "16bars";
    }
}

inline int lengthBeats(int length) noexcept
{
    switch (length)
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        case 3: return 8;
        case 4: return 16;
        case 5: return 32;
        case 6: return 64;
        default: return 16;
    }
}

inline double divisionBeats(int division) noexcept
{
    switch (division)
    {
        case 0: return 1.0;
        case 1: return 0.5;
        case 2: return 1.0 / 3.0;
        case 3: return 0.25;
        case 4: return 1.0 / 6.0;
        default: return 0.125;
    }
}

inline double delayBeats(int division, int style = 0) noexcept
{
    double beats = 0.25;
    switch (division)
    {
        case 0: beats = 2.0; break;
        case 1: beats = 1.0; break;
        case 2: beats = 0.5; break;
        case 3: beats = 0.75; break;
        default: break;
    }
    return beats * (style == 1 ? 1.5 : style == 2 ? 2.0 / 3.0 : 1.0);
}

// 120 BPM when the host has no usable tempo. Non-positive and non-finite
// values are treated as missing so a stopped validator cannot divide by zero.
inline double sanitizeBpm(double bpm) noexcept
{
    if (!std::isfinite(bpm) || bpm <= 0.0)
        return 120.0;
    if (bpm < 20.0)
        return 20.0;
    if (bpm > 400.0)
        return 400.0;
    return bpm;
}

inline double sanitizeSampleRate(double sampleRate) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
        return 44100.0;
    if (sampleRate > 384000.0)
        return 384000.0;
    return sampleRate;
}

inline int64_t musicalSamples(int length, double bpm, double sampleRate) noexcept
{
    const double safeBpm = sanitizeBpm(bpm);
    const double safeRate = sanitizeSampleRate(sampleRate);
    const double seconds = static_cast<double>(lengthBeats(length)) * 60.0 / safeBpm;
    const auto samples = static_cast<int64_t>(std::llround(seconds * safeRate));
    return samples > 1 ? samples : 1;
}

inline int64_t tailSamples(float tailBeats, double bpm, double sampleRate) noexcept
{
    if (!std::isfinite(tailBeats) || tailBeats <= 0.0f)
        return 0;
    const double safeBpm = sanitizeBpm(bpm);
    const double safeRate = sanitizeSampleRate(sampleRate);
    const double seconds = static_cast<double>(tailBeats) * 60.0 / safeBpm;
    const auto samples = static_cast<int64_t>(std::llround(seconds * safeRate));
    return samples > 0 ? samples : 0;
}

inline float curveAt(float x, int curve) noexcept
{
    const float u = std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f;
    switch (curve)
    {
        case 1: // exponential
            return std::pow(2.0f, u) - 1.0f;
        case 2: // logarithmic
            return std::log(1.0f + u * 9.0f) / std::log(10.0f);
        case 3: // smoothstep S-curve
            return u * u * (3.0f - 2.0f * u);
        default:
            return u;
    }
}

inline float flushDenormal(float value) noexcept
{
    return std::abs(value) < 1.0e-15f ? 0.0f : value;
}

inline float dbToGain(float db) noexcept
{
    if (!std::isfinite(db) || db <= -80.0f)
        return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

inline float unityMacro(float macro) noexcept
{
    const float m = std::isfinite(macro) ? std::clamp(macro, 0.0f, 1.0f) : 0.5f;
    return 0.35f + 1.3f * m; // 0.5 -> 1.0
}

struct LayerModel
{
    int type = 0;
    int bank = 0;
    int shape = 0;
    int fadeInCurve = 0;
    int fadeOutCurve = 0;
    int filterType = 0;
    int pitchSemis = 0;
    int seed = 1;
    int rollStart = 0;
    int rollEnd = 5;
    int rollAccel = 1;
    int rollVelCurve = 1;
    float volumeDb = -6.0f;
    float pan = 0.0f;
    float fineCents = 0.0f;
    float offset = 0.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    float cutoffStart = 200.0f;
    float cutoffEnd = 8000.0f;
    float resonance = 0.2f;
    float drive = 0.15f;
    float width = 0.5f;
    float reverbSend = 0.2f;
    float delaySend = 0.1f;
    float swing = 0.0f;
    float velStart = 0.35f;
    float velEnd = 1.0f;
    bool enabled = false;
    bool reverse = false;
    bool solo = false;
    bool mute = false;
    bool locked = false;
};

struct RenderModel
{
    LayerModel layers[layerCount];
    int keyRoot = 0;
    int keyMode = 0; // 0 major, 1 minor
    int length = static_cast<int>(Length::fourBars);
    int delayDivision = static_cast<int>(DelayDivision::eighth);
    int delayStyle = 0;
    int masterFilter = 0;
    int exportBits = 1; // 0=16, 1=24, 2=32f
    int exportRate = 0; // 0=host, 1=44.1, 2=48, 3=96
    float character = 0.5f;
    float intensity = 0.5f;
    float space = 0.5f;
    float brightness = 0.5f;
    float tension = 0.5f;
    float masterWidth = 1.0f;
    float reverbAmount = 0.35f;
    float reverbSize = 0.45f;
    float delayAmount = 0.25f;
    float delayFeedback = 0.35f;
    float masterCutoff = 12000.0f;
    float masterResonance = 0.15f;
    float limiterCeilingDb = -0.3f;
    float masterGainDb = 0.0f;
    float tailBeats = 0.0f;
    float randomStrength = 1.0f;
    bool limiterOn = true;
    bool followHost = false;
    bool preview = false;
    bool exportStems = false;
};

inline int64_t renderedSamples(const RenderModel& model, double bpm, double sampleRate) noexcept
{
    return musicalSamples(model.length, bpm, sampleRate)
         + tailSamples(model.tailBeats, bpm, sampleRate);
}

inline bool layerAudible(const RenderModel& model, int index) noexcept
{
    const auto& layer = model.layers[index];
    if (!layer.enabled || layer.mute)
        return false;
    bool anySolo = false;
    for (const auto& candidate : model.layers)
        if (candidate.enabled && candidate.solo && !candidate.mute)
            anySolo = true;
    if (anySolo && !layer.solo)
        return false;
    return true;
}

inline float blendNormalized(float current, float target, float strength) noexcept
{
    const float amount = std::clamp(std::isfinite(strength) ? strength : 0.0f, 0.0f, 1.0f);
    return current + (target - current) * amount;
}

inline juce::String exportFileName(const RenderModel& model, double bpm, int stemLayer)
{
    juce::String types;
    if (stemLayer >= 0 && stemLayer < layerCount)
    {
        types = "L" + juce::String(stemLayer + 1) + "-" + fxTypeToken(model.layers[stemLayer].type);
    }
    else
    {
        for (int i = 0; i < layerCount; ++i)
        {
            if (!layerAudible(model, i))
                continue;
            if (types.isNotEmpty())
                types << "-";
            types << fxTypeToken(model.layers[i].type);
        }
        if (types.isEmpty())
            types = "Empty";
    }

    const juce::String key = juce::String(noteName(model.keyRoot)) + (model.keyMode == 0 ? "maj" : "min");
    const int bpmText = juce::roundToInt(sanitizeBpm(bpm));
    return "GrokFX_" + types + "_" + key + "_" + juce::String(bpmText) + "bpm_" + lengthToken(model.length) + ".wav";
}

} // namespace grokfx
