#pragma once

#include "Theme.h"
#include "dsp/Model.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace grokfx
{

class GrokFxAudioProcessorEditor;

class GlassPanel : public juce::Component
{
public:
    explicit GlassPanel(juce::String titleText);
    void paint(juce::Graphics& g) override;

private:
    juce::String title;
};

class GrokKnob : public juce::Slider
{
public:
    GrokKnob(juce::String labelText, juce::String tip);
    void bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    bool tick(float dt);
    static juce::String formatValue(double value, const juce::String& unit, bool pan);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> dialBounds() const;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    juce::String label;
    bool isPan = false;
    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float shown = 0.0f;
    float hover = 0.0f;
    float hoverTarget = 0.0f;
};

class GrokChoice : public juce::ComboBox
{
public:
    explicit GrokChoice(juce::String tip);
    void bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class GrokToggle : public juce::ToggleButton
{
public:
    GrokToggle(juce::String labelText, juce::String tip);
    void bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class GrokButton : public juce::TextButton
{
public:
    GrokButton(juce::String labelText, juce::String tip);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
};

class DiceButton : public GrokButton
{
public:
    DiceButton();
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
};

class SoundPad : public juce::Component,
                 public juce::FileDragAndDropTarget,
                 public juce::SettableTooltipClient
{
public:
    SoundPad(int typeIndex, std::function<void(int)> onPick);
    void setSelected(bool shouldSelect);
    bool tick(float dt);
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    void drawIcon(juce::Graphics& g, juce::Rectangle<float> area) const;

    int type = 0;
    std::function<void(int)> pick;
    bool selected = false;
    float hover = 0.0f;
    float hoverTarget = 0.0f;
    float notice = 0.0f;
};

class WaveformView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    void setPeaks(const float* mins, const float* maxes, int count, float dropFraction);
    void setPlayhead(float fraction, bool running);
    void setStatus(const juce::String& text);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void paint(juce::Graphics& g) override;
    std::function<void()> onExternalDrag;
    std::function<void(float)> onPreviewClick;
    bool tick(float dt);

private:
    float minimums[waveformColumns] {};
    float maximums[waveformColumns] {};
    float shownMin[waveformColumns] {};
    float shownMax[waveformColumns] {};
    int count = waveformColumns;
    float drop = 1.0f;
    float playhead = 0.0f;
    bool playing = false;
    bool dragArmed = false;
    juce::String status;
};

class OutputMeter : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    void setLevels(float left, float right);
    void paint(juce::Graphics& g) override;
    bool tick(float dt);

private:
    float targetL = 0.0f;
    float targetR = 0.0f;
    float shownL = 0.0f;
    float shownR = 0.0f;
};

void paintSectionTitle(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

} // namespace grokfx
