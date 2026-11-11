#pragma once

#include "Widgets.h"
#include "PluginProcessor.h"

namespace grokfx
{

class PickPanel : public juce::Component
{
public:
    explicit PickPanel(GrokFxAudioProcessor& processor);
    void resized() override;
    void refresh();
    bool tick(float dt);

private:
    GrokFxAudioProcessor& audio;
    juce::Label title { {}, "Pick" };
    juce::OwnedArray<GrokButton> banks;
    juce::OwnedArray<SoundPad> pads;
};

class MixPanel : public juce::Component
{
public:
    explicit MixPanel(GrokFxAudioProcessor& processor);
    ~MixPanel() override;
    void resized() override;
    void refresh();

private:
    class Card;
    GrokFxAudioProcessor& audio;
    juce::OwnedArray<Card> cards;
    juce::OwnedArray<GrokButton> layerTabs;
    GrokButton addLayer { "+", "Add another layer and select it." };
    int selectedLayer = 0;
};

class DropPanel : public juce::Component
{
public:
    DropPanel(GrokFxAudioProcessor& processor, std::function<void()> dragFiles);
    ~DropPanel() override;
    void resized() override;
    void refreshTransport(double bpm, bool running);
    WaveformView& waveform() { return wave; }
    OutputMeter& meter() { return levels; }

    GrokToggle preview { "Preview", "Audition the mix through the plugin output." };
    GrokButton newTake { "New take", "Roll a new variation seed on every unlocked layer." };
    GrokKnob strength { "Strength", "How far Randomize moves each unlocked control." };
    GrokChoice bits { "Export bit depth. Drag writes this WAV format." };
    GrokChoice rate { "Export sample rate. Host uses the track rate at the moment you drag." };
    GrokToggle stems { "Stems", "Drag one WAV per audible layer instead of the full mix." };
    GrokKnob tail { "Tail", "How long the clip rings past the drop, in beats." };
    GrokKnob intensity { "Intensity", "Macro for level, drive and resonance." };
    GrokKnob space { "Space", "Adjust the amount and size of the layer reverb." };
    GrokKnob brightness { "Bright", "Macro for brightness and filter cutoff." };
    GrokKnob tension { "Tension", "Macro for sweep depth and roll acceleration." };
    GrokKnob tone { "Tone", "Balance between the darker body and the bright edge." };
    GrokKnob gain { "Gain", "Master output gain." };
    GrokKnob masterWidthKnob { "Width", "Master stereo width." };
    GrokKnob ceiling { "Ceiling", "Limiter ceiling." };
    GrokToggle limiter { "Limit", "Keep the output under the ceiling." };
    GrokChoice filter { "Master filter type." };
    GrokKnob cutoff { "Cutoff", "Master filter cutoff." };
    GrokKnob resonance { "Reso", "Master filter resonance." };
    GrokKnob reverb { "Room", "Master reverb size." };
    GrokKnob delay { "Delay", "Master delay amount." };
    GrokChoice delayTime { "Choose the tempo-synced delay note length." };
    GrokChoice delayRhythm { "Set the delay rhythm to straight, dotted, or triplet." };

private:
    GrokFxAudioProcessor& audio;
    juce::Label title { {}, "Drop" };
    juce::Label bpm { {}, "120 BPM" };
    WaveformView wave;
    OutputMeter levels;
    juce::Component details;
};

} // namespace grokfx
