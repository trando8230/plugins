#pragma once

#include "dsp/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class GrokFxAudioProcessor : public juce::AudioProcessor
{
public:
    GrokFxAudioProcessor();
    ~GrokFxAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    double getDisplayedBpm() const noexcept { return displayedBpm.load(std::memory_order_relaxed); }
    float getUiScale() const noexcept;
    void setUiScale(float scale) noexcept;
    int getArmedLayer() const noexcept { return armedLayer.load(std::memory_order_relaxed); }
    void setArmedLayer(int layer) noexcept;
    bool isShowingB() const noexcept { return showingB.load(std::memory_order_relaxed); }

    void newTake();
    void randomizeLayers(juce::Random& random);
    void toggleAB();

    uint32_t nonFiniteCount() const noexcept { return engine.nonFiniteCount(); }
    void resetNonFiniteCount() noexcept { engine.resetNonFiniteCount(); }

    float getMeterL() const noexcept { return meterL.load(std::memory_order_relaxed); }
    float getMeterR() const noexcept { return meterR.load(std::memory_order_relaxed); }
    float getPlayFraction() const noexcept { return playFraction.load(std::memory_order_relaxed); }
    bool isPreviewRunning() const noexcept { return previewRunning.load(std::memory_order_relaxed); }

    grokfx::RenderModel readModel() const noexcept;
    int64_t renderMix(juce::AudioBuffer<float>& destination, double bpm, double sampleRate) const;
    juce::Array<juce::File> renderExportFiles(const juce::File& directory, double hostSampleRate) const;
    juce::String makeExportFileName(double bpm, int stemLayer) const;

private:
    struct Raw;
    std::unique_ptr<Raw> raw;
    mutable grokfx::Engine engine;
    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> floatScratch;
    bool enginePrepared = false;
    int64_t freeSample = 0;
    int64_t oneshotRemaining = 0;

    std::atomic<int> uiScaleMilli { 10000 };
    std::atomic<int> armedLayer { 0 };
    std::atomic<bool> showingB { false };
    juce::String slotA;
    juce::String slotB;
    juce::CriticalSection stateLock;

    std::atomic<double> displayedBpm { 120.0 };
    std::atomic<float> meterL { 0.0f };
    std::atomic<float> meterR { 0.0f };
    std::atomic<float> playFraction { 0.0f };
    std::atomic<bool> previewRunning { false };

    struct Transport
    {
        double bpm = 120.0;
        bool playing = false;
        bool hasPpq = false;
        double ppq = 0.0;
    };

    Transport readTransport() const noexcept;
    void renderFree(juce::AudioBuffer<float>& buffer, int offset, int count, const grokfx::RenderModel& model,
                    double bpm, int64_t total) noexcept;
    void renderGrid(juce::AudioBuffer<float>& buffer, int offset, int count, const grokfx::RenderModel& model,
                    double bpm, double ppq, int64_t total) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrokFxAudioProcessor)
};
