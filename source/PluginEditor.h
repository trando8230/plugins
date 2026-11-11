#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"

class GrokFxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer,
                                   private juce::ValueTree::Listener,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   public juce::DragAndDropContainer
{
public:
    explicit GrokFxAudioProcessorEditor(GrokFxAudioProcessor&);
    ~GrokFxAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void renderWaveformSync();
    juce::Array<juce::File> readyDragFiles() const;

    juce::Image blurredBackground() const { return blur; }
    float backgroundScale() const { return blurScale; }

private:
    class PeakThread;

    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged(const juce::String&, float) override;
    void rebuildBlur();
    void tickTree(juce::Component& component, float dt);
    void dragExport();
    void applyScale(float scale);

    GrokFxAudioProcessor& audio;
    grokfx::GrokLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::Component content;
    juce::Label logo;
    juce::Label company;
    juce::Label bpm;
    grokfx::DiceButton randomize;
    grokfx::GrokToggle follow { "Follow", "Start the phrase in time with the host bar while the transport is playing." };
    grokfx::GrokChoice key { "Song key. Every layer is tuned to this root." };
    grokfx::GrokChoice mode { "Major or minor. This sets the harmony interval." };
    grokfx::GrokChoice length { "Phrase length. The drop lands on the last sample before the tail." };
    grokfx::GrokButton undo { "Undo", "Undo the last parameter change." };
    grokfx::GrokButton redo { "Redo", "Redo a parameter change." };
    grokfx::GrokButton compare { "A", "Compare two mixes. Edits stay on the side you are hearing." };
    grokfx::PickPanel pick;
    grokfx::MixPanel mix;
    grokfx::DropPanel drop;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<PeakThread> peaks;
    juce::Image blur;
    float blurScale = 1.0f;
    bool applyingScale = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrokFxAudioProcessorEditor)
};
