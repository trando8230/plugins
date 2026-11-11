#pragma once

#include "Model.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstdint>

namespace grokfx
{

struct WavePeaks
{
    float minColumn[waveformColumns] {};
    float maxColumn[waveformColumns] {};
    int columns = waveformColumns;
    float dropFraction = 1.0f;
};

class Engine
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // Allocation-free. Caller must have called prepare for the same sample rate
    // before a sounding block. A larger block than prepare is safe.
    void process(float* const* channels, int numChannels, int numSamples, int64_t startSample,
                 const RenderModel& model, double bpm) noexcept;

    // Returns samples actually rendered. shouldStop is polled between chunks
    // so the editor can close without killing the peak thread.
    int64_t renderOffline(juce::AudioBuffer<float>& destination, const RenderModel& model,
                          double bpm, double sampleRate,
                          bool (*shouldStop)(void*) = nullptr, void* stopContext = nullptr);

    static WavePeaks peaksFor(const juce::AudioBuffer<float>& buffer, int64_t musicalLength);

    uint32_t nonFiniteCount() const noexcept { return nonFinite.load(std::memory_order_relaxed); }
    void resetNonFiniteCount() noexcept { nonFinite.store(0, std::memory_order_relaxed); }

private:
    struct Voice
    {
        double phase[8] {};
        float svf1 = 0.0f;
        float svf2 = 0.0f;
        float tilt = 0.0f;
        float formantY1[3] {};
        float formantY2[3] {};
        float hitBeats[512] {};
        float hitVelocity[512] {};
        int hitCount = 0;
        int hitHash = 0;
        int64_t lastSample = -1000000;
    };

    struct Comb
    {
        std::vector<float> buffer;
        int index = 0;
        float dampState = 0.0f;
        float process(float input, int tap, float feedback, float damp) noexcept;
    };

    void renderSample(const RenderModel& model, double bpm, int64_t sampleIndex,
                      float& left, float& right) noexcept;
    float renderLayer(Voice& voice, const RenderModel& model, int index, int64_t sampleIndex,
                      int64_t musical, int64_t total, double bpm) noexcept;
    void rebuildHits(Voice& voice, const LayerModel& layer, const RenderModel& model, double bpm) noexcept;
    float finishSample(float value) noexcept;

    double sampleRate = 44100.0;
    bool prepared = false;
    Voice voices[layerCount];
    Comb combs[8];
    std::vector<float> delayL;
    std::vector<float> delayR;
    int delayIndex = 0;
    float delayFeedbackStateL = 0.0f;
    float delayFeedbackStateR = 0.0f;
    float masterSvfL1 = 0.0f;
    float masterSvfL2 = 0.0f;
    float masterSvfR1 = 0.0f;
    float masterSvfR2 = 0.0f;
    float limiterEnv = 0.0f;
    std::atomic<uint32_t> nonFinite { 0 };
};

} // namespace grokfx
