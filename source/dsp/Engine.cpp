#include "Engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace grokfx
{
namespace
{

constexpr float twoPi = 6.28318530717958647692f;
constexpr float pi = 3.14159265358979323846f;

uint32_t hashU(uint32_t x) noexcept
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float hashSigned(uint32_t x) noexcept
{
    return static_cast<float>(hashU(x) & 0x00ffffffu) / 8388607.0f - 1.0f;
}

float polyBlep(float t, float dt) noexcept
{
    if (dt <= 1.0e-6f)
        return 0.0f;
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float sawSample(float phase, float dt) noexcept
{
    const float saw = 2.0f * phase - 1.0f;
    return saw - polyBlep(phase, dt);
}

float wrapPhase(double phase) noexcept
{
    phase -= std::floor(phase);
    if (phase < 0.0)
        phase += 1.0;
    return static_cast<float>(phase);
}

float midiToHz(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

int clampIndex(int value, int count) noexcept
{
    if (value < 0)
        return 0;
    if (value >= count)
        return count - 1;
    return value;
}

float filterSample(float& ic1, float& ic2, float input, float cutoff, float resonance,
                   float sampleRate, int mode) noexcept
{
    if (mode == static_cast<int>(FilterType::off))
        return input;

    const float nyquist = std::max(50.0f, sampleRate * 0.45f);
    const float hz = std::clamp(cutoff, 20.0f, nyquist);
    const float res = std::clamp(resonance, 0.0f, 0.96f);
    const float g = std::tan(pi * hz / sampleRate);
    const float k = 2.0f - 2.0f * res;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = input - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = flushDenormal(2.0f * v1 - ic1);
    ic2 = flushDenormal(2.0f * v2 - ic2);
    if (!std::isfinite(ic1) || !std::isfinite(ic2) || std::abs(ic1) > 8.0f || std::abs(ic2) > 8.0f)
    {
        ic1 = 0.0f;
        ic2 = 0.0f;
        return 0.0f;
    }

    if (mode == static_cast<int>(FilterType::lowPass))
        return v2;
    if (mode == static_cast<int>(FilterType::highPass))
        return input - k * v1 - v2;
    return v1;
}

float resonator(float& y1, float& y2, float input, float freq, float sampleRate) noexcept
{
    const float hz = std::clamp(freq, 40.0f, sampleRate * 0.45f);
    const float w = twoPi * hz / sampleRate;
    constexpr float q = 8.0f;
    const float r = std::exp(-w / (2.0f * q));
    const float a = 2.0f * r * std::cos(w);
    float y = input * (1.0f - r) + a * y1 - r * r * y2;
    if (!std::isfinite(y) || std::abs(y) > 4.0f)
        y = 0.0f;
    y2 = flushDenormal(y1);
    y1 = flushDenormal(y);
    return y;
}

float vowelFormant(int index, float position) noexcept
{
    // a, e, i, o, u. Original formant path, not a sampled choir.
    static constexpr float table[5][3] = {
        { 800.0f, 1200.0f, 2600.0f },
        { 450.0f, 2100.0f, 2800.0f },
        { 320.0f, 2400.0f, 3200.0f },
        { 520.0f, 900.0f, 2500.0f },
        { 360.0f, 760.0f, 2400.0f }
    };
    const float scaled = std::clamp(position, 0.0f, 1.0f) * 4.0f;
    const int a = std::clamp(static_cast<int>(scaled), 0, 3);
    const int b = a + 1;
    const float frac = scaled - static_cast<float>(a);
    return table[a][index] + (table[b][index] - table[a][index]) * frac;
}

} // namespace

float Engine::Comb::process(float input, int tap, float feedback, float damp) noexcept
{
    if (buffer.empty())
        return 0.0f;
    const int size = static_cast<int>(buffer.size());
    const int safeTap = std::clamp(tap, 1, size - 1);
    const int read = (index - safeTap + size) % size;
    const float delayed = buffer[static_cast<size_t>(read)];
    dampState = flushDenormal(delayed * (1.0f - damp) + dampState * damp);
    buffer[static_cast<size_t>(index)] = flushDenormal(input + dampState * feedback);
    index = (index + 1) % size;
    return delayed;
}

void Engine::prepare(double newSampleRate)
{
    sampleRate = sanitizeSampleRate(newSampleRate);
    const int reverbMax = static_cast<int>(sampleRate * 0.12) + 8;
    const int baseTaps[8] = { 29, 37, 41, 47, 53, 59, 67, 73 };
    for (int i = 0; i < 8; ++i)
    {
        combs[i].buffer.assign(static_cast<size_t>(reverbMax), 0.0f);
        combs[i].index = 0;
        combs[i].dampState = 0.0f;
        juce::ignoreUnused(baseTaps);
    }
    const int delayMax = static_cast<int>(sampleRate * 8.0) + 8;
    delayL.assign(static_cast<size_t>(delayMax), 0.0f);
    delayR.assign(static_cast<size_t>(delayMax), 0.0f);
    delayIndex = 0;
    prepared = true;
    reset();
}

void Engine::reset() noexcept
{
    for (auto& voice : voices)
        voice = {};
    for (auto& comb : combs)
    {
        std::fill(comb.buffer.begin(), comb.buffer.end(), 0.0f);
        comb.index = 0;
        comb.dampState = 0.0f;
    }
    std::fill(delayL.begin(), delayL.end(), 0.0f);
    std::fill(delayR.begin(), delayR.end(), 0.0f);
    delayIndex = 0;
    delayFeedbackStateL = 0.0f;
    delayFeedbackStateR = 0.0f;
    masterSvfL1 = masterSvfL2 = 0.0f;
    masterSvfR1 = masterSvfR2 = 0.0f;
    limiterEnv = 0.0f;
}

float Engine::finishSample(float value) noexcept
{
    if (!std::isfinite(value))
    {
        nonFinite.fetch_add(1, std::memory_order_relaxed);
        return 0.0f;
    }
    value = flushDenormal(value);
    return std::clamp(value, -1.0f, 1.0f);
}

void Engine::rebuildHits(Voice& voice, const LayerModel& layer, const RenderModel& model, double bpm) noexcept
{
    const double totalBeats = static_cast<double>(lengthBeats(model.length));
    const double startStep = divisionBeats(clampIndex(layer.rollStart, static_cast<int>(Division::count)));
    double endStep = divisionBeats(clampIndex(layer.rollEnd, static_cast<int>(Division::count)));
    endStep *= 1.15 - 0.7 * std::clamp(model.tension, 0.0f, 1.0f);
    endStep = std::max(0.03, endStep);

    double cursor = 0.0;
    int count = 0;
    while (cursor < totalBeats - 1.0e-4 && count < 510)
    {
        const float u = static_cast<float>(cursor / totalBeats);
        const double spacing = std::max(0.03, static_cast<double>(
            startStep + (endStep - startStep) * curveAt(u, layer.rollAccel)));
        double hit = cursor;
        if ((count % 2) == 1)
            hit = std::min(totalBeats, cursor + spacing * 0.5 * std::clamp(layer.swing, 0.0f, 1.0f));
        voice.hitBeats[count] = static_cast<float>(hit);
        voice.hitVelocity[count] = std::clamp(
            layer.velStart + (layer.velEnd - layer.velStart) * curveAt(u, layer.rollVelCurve),
            0.0f, 1.0f);
        ++count;
        cursor += spacing;
        juce::ignoreUnused(bpm);
    }
    voice.hitBeats[count] = static_cast<float>(totalBeats);
    voice.hitVelocity[count] = std::clamp(layer.velEnd, 0.0f, 1.0f);
    voice.hitCount = count + 1;
}

float Engine::renderLayer(Voice& voice, const RenderModel& model, int index, int64_t sampleIndex,
                          int64_t musical, int64_t total, double bpm) noexcept
{
    const auto& layer = model.layers[index];
    if (!layerAudible(model, index) || sampleIndex < 0 || sampleIndex >= total || musical < 2)
        return 0.0f;

    int64_t source = sampleIndex;
    if (layer.reverse && sampleIndex < musical)
        source = musical - 1 - sampleIndex;

    const float u = std::clamp(static_cast<float>(source) / static_cast<float>(musical - 1), 0.0f, 1.0f);
    const float offset = std::clamp(layer.offset, 0.0f, 0.9f);
    if (source < musical && u < offset)
        return 0.0f;

    const float span = std::max(0.05f, 1.0f - offset);
    const float shapedPosition = source < musical ? std::clamp((u - offset) / span, 0.0f, 1.0f) : 1.0f;
    const float motion = curveAt(shapedPosition, layer.shape);
    const float sr = static_cast<float>(sampleRate);
    const float intensity = unityMacro(model.intensity);
    const float brightness = std::pow(2.0f, (std::clamp(model.brightness, 0.0f, 1.0f) - 0.5f) * 2.0f);
    const float tension = unityMacro(model.tension);
    const int seed = std::max(1, layer.seed);
    const float interval = model.keyMode == 0 ? 4.0f : 3.0f;
    const float rootMidi = 60.0f + static_cast<float>(model.keyRoot)
                         + static_cast<float>(layer.pitchSemis)
                         + layer.fineCents / 100.0f;
    const float sweep = 24.0f * tension * (0.55f + 0.45f * model.character);
    float semis = rootMidi;
    const int type = clampIndex(layer.type, static_cast<int>(FxType::count));

    if (type == static_cast<int>(FxType::riser) || type == static_cast<int>(FxType::noiseSweep)
        || type == static_cast<int>(FxType::reverseCymbal))
        semis += -sweep * 0.25f + sweep * motion;
    else if (type == static_cast<int>(FxType::downlifter))
        semis += sweep * 0.75f - sweep * motion;
    else if (type == static_cast<int>(FxType::subDrop) && source >= musical - 1)
    {
        const float seconds = static_cast<float>(source - (musical - 1)) / sr;
        semis -= seconds * (10.0f + 18.0f * model.tension) * 2.0f;
    }

    const float variation = (static_cast<float>(seed % 97) - 48.0f) * 0.00015f;
    const float hz = std::clamp(midiToHz(semis) * (1.0f + variation), 20.0f, sr * 0.45f);
    const float dt = hz / sr;

    if (sampleIndex != voice.lastSample + 1)
    {
        for (double& phase : voice.phase)
            phase = 0.0;
        voice.svf1 = voice.svf2 = voice.tilt = 0.0f;
        for (int i = 0; i < 3; ++i)
            voice.formantY1[i] = voice.formantY2[i] = 0.0f;
    }
    voice.lastSample = sampleIndex;

    auto advance = [&](int slot, float frequency) noexcept {
        voice.phase[slot] += static_cast<double>(frequency / sr);
        return wrapPhase(voice.phase[slot]);
    };

    const float fundamental = advance(0, hz);
    const float detune = advance(1, hz * (1.007f + variation));
    const float harmony = advance(2, hz * std::pow(2.0f, interval / 12.0f));
    const float noise = hashSigned(hashU(static_cast<uint32_t>(source + 1) ^ static_cast<uint32_t>(seed * 131u) ^ static_cast<uint32_t>(index * 17)));

    float tone = 0.0f;
    const int bank = clampIndex(layer.bank, static_cast<int>(Bank::count));
    if (bank == static_cast<int>(Bank::analog))
    {
        const float tri = 1.0f - 4.0f * std::abs(fundamental - 0.5f);
        const float drift = 0.5f + 0.5f * std::sin(twoPi * (fundamental * 0.25f + variation * 40.0f));
        tone = 0.75f * tri + 0.25f * drift * sawSample(detune, dt);
    }
    else if (bank == static_cast<int>(Bank::digital))
    {
        const float modulator = std::sin(twoPi * detune);
        const float indexFm = (1.5f + 8.0f * motion) * brightness;
        tone = std::sin(twoPi * fundamental + modulator * indexFm);
    }
    else if (bank == static_cast<int>(Bank::vocal))
    {
        const float excite = 0.6f * sawSample(fundamental, dt) + 0.15f * noise;
        float sum = 0.0f;
        for (int formant = 0; formant < 3; ++formant)
        {
            const float gain = formant == 0 ? 1.0f : (formant == 1 ? 0.62f : 0.38f);
            sum += gain * resonator(voice.formantY1[formant], voice.formantY2[formant], excite,
                                    vowelFormant(formant, motion) * brightness, sr);
        }
        tone = sum * 0.45f;
    }
    else
    {
        tone = 0.48f * sawSample(fundamental, dt)
             + 0.28f * sawSample(detune, dt * 1.007f)
             + 0.18f * std::sin(twoPi * harmony);
    }

    const float noiseMix = std::clamp(0.08f + 0.35f * model.brightness * (0.35f + 0.65f * motion), 0.0f, 0.85f);
    if (bank != static_cast<int>(Bank::vocal))
        tone = tone * (1.0f - noiseMix) + noise * noiseMix;

    if (type == static_cast<int>(FxType::reverseCymbal))
    {
        float metal = 0.0f;
        constexpr float ratios[5] = { 1.0f, 1.47f, 2.11f, 2.93f, 4.18f };
        for (int partial = 0; partial < 5; ++partial)
        {
            const float phase = advance(3 + partial, hz * ratios[partial] * (0.5f + brightness));
            metal += (1.0f / static_cast<float>(partial + 1)) * sawSample(phase, dt * ratios[partial]);
        }
        tone = metal * 0.35f + noise * (0.25f + 0.45f * motion);
    }
    else if (type == static_cast<int>(FxType::noiseSweep) || type == static_cast<int>(FxType::impact))
    {
        tone = tone * 0.35f + noise * 0.85f;
    }
    else if (type == static_cast<int>(FxType::snareRoll))
    {
        tone = noise * 0.85f + std::sin(twoPi * fundamental) * 0.35f;
    }
    else if (type == static_cast<int>(FxType::subDrop))
    {
        tone = std::sin(twoPi * advance(0, hz * 0.5f)) * 0.9f + noise * 0.08f;
    }

    float envelope = motion;
    if (type == static_cast<int>(FxType::impact))
    {
        const float attackSamples = sr * 0.035f;
        if (source >= musical - 1)
        {
            const float seconds = static_cast<float>(source - (musical - 1)) / sr;
            envelope = std::exp(-seconds * 8.0f);
        }
        else
        {
            const float distance = static_cast<float>((musical - 1) - source);
            envelope = distance < attackSamples ? curveAt(1.0f - distance / attackSamples, layer.shape) : 0.0f;
        }
    }
    else if (type == static_cast<int>(FxType::subDrop))
    {
        if (source >= musical - 1)
        {
            const float seconds = static_cast<float>(source - (musical - 1)) / sr;
            envelope = std::exp(-seconds * 1.6f);
        }
        else
            envelope = 0.25f + 0.75f * motion;
    }
    else if (type == static_cast<int>(FxType::snareRoll))
    {
        const int hash = seed ^ (layer.rollStart << 8) ^ (layer.rollEnd << 12) ^ (layer.rollAccel << 16)
                       ^ model.length ^ static_cast<int>(bpm * 10.0);
        if (voice.hitHash != hash || voice.hitCount == 0)
        {
            rebuildHits(voice, layer, model, bpm);
            voice.hitHash = hash;
        }
        const double beats = static_cast<double>(source) * sanitizeBpm(bpm) / (sampleRate * 60.0);
        int hit = 0;
        while (hit + 1 < voice.hitCount && voice.hitBeats[hit + 1] <= beats)
            ++hit;
        const double ageBeats = beats - voice.hitBeats[hit];
        const double ageSeconds = ageBeats * 60.0 / sanitizeBpm(bpm);
        const float decay = std::exp(static_cast<float>(-ageSeconds * 18.0f));
        envelope = voice.hitVelocity[hit] * decay;
    }

    if (source < musical)
    {
        const float fadeIn = std::clamp(layer.fadeIn, 0.0f, 1.0f);
        const float fadeOut = std::clamp(layer.fadeOut, 0.0f, 1.0f);
        if (fadeIn > 0.001f && shapedPosition < fadeIn)
            envelope *= curveAt(shapedPosition / fadeIn, layer.fadeInCurve);
        if (fadeOut > 0.001f && shapedPosition > 1.0f - fadeOut)
            envelope *= curveAt((1.0f - shapedPosition) / fadeOut, layer.fadeOutCurve);
    }

    const float high = tone - voice.tilt;
    voice.tilt = flushDenormal(voice.tilt + high * 0.04f);
    tone += (high - voice.tilt) * (std::clamp(model.character, 0.0f, 1.0f) - 0.5f) * 1.4f;

    const float cutoff = (layer.cutoffStart + (layer.cutoffEnd - layer.cutoffStart) * motion) * brightness;
    const float resonance = std::clamp(layer.resonance * intensity, 0.0f, 0.96f);
    if (layer.filterType != static_cast<int>(FilterType::off))
        tone = filterSample(voice.svf1, voice.svf2, tone, cutoff, resonance, sr, layer.filterType);
    else if (type == static_cast<int>(FxType::noiseSweep))
        tone = filterSample(voice.svf1, voice.svf2, tone, 180.0f + 9000.0f * motion * brightness, 0.25f, sr,
                            static_cast<int>(FilterType::bandPass));

    const float drive = std::clamp(layer.drive * intensity, 0.0f, 1.0f);
    const float driveGain = 1.0f + drive * 10.0f;
    tone = std::tanh(tone * driveGain) / std::tanh(driveGain);

    const float gain = dbToGain(layer.volumeDb) * (0.7f + 0.6f * std::clamp(model.intensity, 0.0f, 1.0f)) * 0.65f;
    return flushDenormal(tone * envelope * gain);
}

void Engine::renderSample(const RenderModel& model, double bpm, int64_t sampleIndex,
                          float& left, float& right) noexcept
{
    const int64_t musical = musicalSamples(model.length, bpm, sampleRate);
    const int64_t total = musical + tailSamples(model.tailBeats, bpm, sampleRate);
    float dryL = 0.0f;
    float dryR = 0.0f;
    float sendR = 0.0f;
    float sendD = 0.0f;
    const float space = unityMacro(model.space);

    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const float mono = renderLayer(voices[layerIndex], model, layerIndex, sampleIndex, musical, total, bpm);
        if (mono == 0.0f)
            continue;
        const auto& layer = model.layers[layerIndex];
        const float pan = std::clamp(layer.pan, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * 0.25f * pi;
        float l = mono * std::cos(angle);
        float r = mono * std::sin(angle);
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * (std::clamp(layer.width, 0.0f, 1.0f) * 2.0f);
        l = mid + side;
        r = mid - side;
        dryL += l;
        dryR += r;
        const float send = 0.5f * (l + r);
        sendR += send * std::clamp(layer.reverbSend, 0.0f, 1.0f);
        sendD += send * std::clamp(layer.delaySend, 0.0f, 1.0f);
    }

    const float sr = static_cast<float>(sampleRate);
    if (model.masterFilter != static_cast<int>(FilterType::off))
    {
        const float cutoff = model.masterCutoff * std::pow(2.0f, (model.brightness - 0.5f) * 2.0f);
        dryL = filterSample(masterSvfL1, masterSvfL2, dryL, cutoff, model.masterResonance, sr, model.masterFilter);
        dryR = filterSample(masterSvfR1, masterSvfR2, dryR, cutoff, model.masterResonance, sr, model.masterFilter);
    }

    const float masterWidth = std::clamp(model.masterWidth, 0.0f, 1.0f) * space;
    const float mid = 0.5f * (dryL + dryR);
    const float side = 0.5f * (dryL - dryR) * (masterWidth * 2.0f);
    dryL = mid + side;
    dryR = mid - side;

    float reverbL = 0.0f;
    float reverbR = 0.0f;
    if (prepared && !combs[0].buffer.empty())
    {
        const float size = std::clamp(model.reverbSize * space, 0.05f, 1.5f);
        const float feedback = std::clamp(0.25f + size * 0.45f, 0.0f, 0.86f);
        const float damp = 0.25f + (1.0f - model.brightness) * 0.4f;
        const int base[8] = { 1103, 1381, 1601, 1861, 2017, 2281, 2531, 2791 };
        const float excited = sendR * std::clamp(model.reverbAmount, 0.0f, 1.0f) * space;
        for (int i = 0; i < 8; ++i)
        {
            const int tap = std::clamp(static_cast<int>(base[i] * size * sampleRate / 44100.0), 8,
                                       static_cast<int>(combs[i].buffer.size()) - 2);
            const float wet = combs[i].process(excited, tap, feedback, damp);
            if ((i & 1) == 0)
                reverbL += wet;
            else
                reverbR += wet;
        }
        reverbL = flushDenormal(reverbL * 0.18f);
        reverbR = flushDenormal(reverbR * 0.18f);
    }

    float delayOutL = 0.0f;
    float delayOutR = 0.0f;
    if (prepared && delayL.size() > 8)
    {
        const double beats = delayBeats(model.delayDivision);
        int tap = static_cast<int>(std::llround(beats * sampleRate * 60.0 / sanitizeBpm(bpm)));
        tap = std::clamp(tap, 1, static_cast<int>(delayL.size()) - 2);
        const int size = static_cast<int>(delayL.size());
        const int read = (delayIndex - tap + size) % size;
        const int readR = (delayIndex - std::max(1, tap - 17) + size) % size;
        delayOutL = delayL[static_cast<size_t>(read)];
        delayOutR = delayR[static_cast<size_t>(readR)];
        const float feedback = std::clamp(model.delayFeedback, 0.0f, 0.85f);
        const float excited = sendD * std::clamp(model.delayAmount, 0.0f, 1.0f) * space;
        delayL[static_cast<size_t>(delayIndex)] = flushDenormal(excited + delayOutL * feedback);
        delayR[static_cast<size_t>(delayIndex)] = flushDenormal(excited + delayOutR * feedback);
        delayIndex = (delayIndex + 1) % size;
        delayOutL = flushDenormal(delayOutL);
        delayOutR = flushDenormal(delayOutR);
    }

    float l = dryL + reverbL + delayOutL;
    float r = dryR + reverbR + delayOutR;
    l *= dbToGain(model.masterGainDb);
    r *= dbToGain(model.masterGainDb);

    if (model.limiterOn)
    {
        const float ceiling = std::clamp(dbToGain(model.limiterCeilingDb), 0.05f, 1.0f);
        const float peak = std::max(std::abs(l), std::abs(r));
        const float coeff = peak > limiterEnv ? 0.35f : 0.0025f;
        limiterEnv = flushDenormal(limiterEnv + (peak - limiterEnv) * coeff);
        const float gain = limiterEnv > ceiling && limiterEnv > 1.0e-8f ? ceiling / limiterEnv : 1.0f;
        l *= gain;
        r *= gain;
    }

    if (sampleIndex >= musical && total > musical)
    {
        const float tailU = static_cast<float>(sampleIndex - musical) / static_cast<float>(std::max<int64_t>(1, total - musical));
        const float tailFade = 1.0f - curveAt(std::clamp(tailU, 0.0f, 1.0f), static_cast<int>(Curve::sCurve));
        // Keep the drop sample intact, then let the ring fade to silence.
        if (sampleIndex > musical)
        {
            l *= tailFade;
            r *= tailFade;
        }
    }

    left = l;
    right = r;
}

void Engine::process(float* const* channels, int numChannels, int numSamples, int64_t startSample,
                     const RenderModel& model, double bpm) noexcept
{
    if (channels == nullptr || numSamples <= 0)
        return;
    const double safeBpm = sanitizeBpm(bpm);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;
        if (prepared)
            renderSample(model, safeBpm, startSample + sample, left, right);
        left = finishSample(left);
        right = finishSample(right);
        if (numChannels > 0 && channels[0] != nullptr)
            channels[0][sample] = left;
        if (numChannels > 1 && channels[1] != nullptr)
            channels[1][sample] = right;
    }
}

void Engine::renderOffline(juce::AudioBuffer<float>& destination, const RenderModel& model,
                           double bpm, double sampleRateIn)
{
    const double rate = sanitizeSampleRate(sampleRateIn);
    const double safeBpm = sanitizeBpm(bpm);
    const int64_t total64 = renderedSamples(model, safeBpm, rate);
    const int total = static_cast<int>(std::min<int64_t>(total64, 8'000'000));
    destination.setSize(2, std::max(1, total), false, true, true);
    destination.clear();
    prepare(rate);
    reset();
    constexpr int chunk = 1024;
    int64_t rendered = 0;
    while (rendered < destination.getNumSamples())
    {
        const int count = static_cast<int>(std::min<int64_t>(chunk, destination.getNumSamples() - rendered));
        float* pointers[2] = {
            destination.getWritePointer(0) + rendered,
            destination.getWritePointer(1) + rendered
        };
        process(pointers, 2, count, rendered, model, safeBpm);
        rendered += count;
    }
}

WavePeaks Engine::peaksFor(const juce::AudioBuffer<float>& buffer, int64_t musicalLength)
{
    WavePeaks peaks;
    const int samples = buffer.getNumSamples();
    peaks.dropFraction = samples > 0
        ? std::clamp(static_cast<float>(musicalLength) / static_cast<float>(samples), 0.0f, 1.0f)
        : 1.0f;
    if (samples <= 0)
        return peaks;

    for (int column = 0; column < waveformColumns; ++column)
    {
        const int start = static_cast<int>((static_cast<int64_t>(column) * samples) / waveformColumns);
        const int end = std::max(start + 1, static_cast<int>((static_cast<int64_t>(column + 1) * samples) / waveformColumns));
        float minValue = 0.0f;
        float maxValue = 0.0f;
        for (int sample = start; sample < end && sample < samples; ++sample)
        {
            const float value = 0.5f * (buffer.getSample(0, sample)
                                      + (buffer.getNumChannels() > 1 ? buffer.getSample(1, sample) : buffer.getSample(0, sample)));
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
        peaks.minColumn[column] = minValue;
        peaks.maxColumn[column] = maxValue;
    }
    return peaks;
}

} // namespace grokfx
