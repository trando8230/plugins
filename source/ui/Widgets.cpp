#include "Widgets.h"
#include "dsp/Model.h"

namespace grokfx
{
namespace
{

float approach(float current, float target, float dt) noexcept
{
    const float blend = 1.0f - std::pow(0.001f, dt / Theme::motionSeconds);
    const float next = current + (target - current) * blend;
    return std::abs(next - target) < 0.001f ? target : next;
}

void fillGlass(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour(Theme::shadow());
    g.fillRoundedRectangle(bounds.translated(0.0f, 6.0f), radius);
    juce::ColourGradient wash(Theme::glassTop(), bounds.getX(), bounds.getY(),
                              Theme::glassBottom(), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(wash);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(Theme::highlight());
    g.drawLine(bounds.getX() + radius, bounds.getY() + 1.0f, bounds.getRight() - radius, bounds.getY() + 1.0f, 1.0f);
    g.setColour(Theme::border());
    g.drawRoundedRectangle(bounds, radius, 1.0f);
}

} // namespace

void paintSectionTitle(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour(Theme::text());
    g.setFont(Theme::font(Theme::typeSection, true));
    g.drawFittedText(text, area, juce::Justification::centredLeft, 1);
}

GlassPanel::GlassPanel(juce::String titleText)
    : title(std::move(titleText))
{
    setInterceptsMouseClicks(false, true);
}

void GlassPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    fillGlass(g, bounds, Theme::radiusL);
    if (title.isNotEmpty())
        paintSectionTitle(g, getLocalBounds().reduced(Theme::space2).removeFromTop(Theme::space3), title);
}

GrokKnob::GrokKnob(juce::String labelText, juce::String tip)
    : label(std::move(labelText))
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setScrollWheelEnabled(true);
    setWantsKeyboardFocus(false);
    setTooltip(tip);
    setMouseDragSensitivity(220);
}

void GrokKnob::bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    isPan = paramId.endsWith("_pan");
    parameter = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(paramId));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramId, *this);
    if (parameter != nullptr)
        setDoubleClickReturnValue(true, parameter->getDefaultValue(), juce::ModifierKeys::noModifiers);
    shown = static_cast<float>(valueToProportionOfLength(getValue()));
    if (getTooltip().isEmpty() && parameter != nullptr)
        setTooltip(parameter->getName(64));
}

juce::String GrokKnob::formatValue(double value, const juce::String& unit, bool pan)
{
    if (pan)
    {
        const int amount = juce::roundToInt(std::abs(value) * 100.0);
        return amount == 0 ? "C" : juce::String(amount) + (value < 0.0 ? " L" : " R");
    }
    if (unit == "%") return juce::String(juce::roundToInt(value * 100.0)) + "%";
    if (unit == "st") return juce::String(juce::roundToInt(value)) + " st";
    if (unit == "ct") return juce::String(juce::roundToInt(value)) + " ct";
    if (unit == "Hz") return juce::String(juce::roundToInt(value)) + " Hz";
    return juce::String(value, 1) + (unit.isEmpty() ? "" : " " + unit);
}

juce::Rectangle<float> GrokKnob::dialBounds() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom(16.0f);
    const float side = std::max(0.0f, std::min(area.getWidth(), area.getHeight()) - 4.0f);
    return juce::Rectangle<float>(side, side).withCentre(area.getCentre());
}

void GrokKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto dial = dialBounds();
    if (event.position.getDistanceFrom(dial.getCentre()) <= dial.getWidth() * 0.5f)
        juce::Slider::mouseWheelMove(event, wheel);
}

bool GrokKnob::tick(float dt)
{
    const float target = static_cast<float>(valueToProportionOfLength(getValue()));
    const float next = approach(shown, target, dt);
    const float nextHover = approach(hover, hoverTarget, dt);
    const bool changed = next != shown || nextHover != hover;
    shown = next;
    hover = nextHover;
    if (changed)
        repaint();
    return changed;
}

void GrokKnob::mouseDown(const juce::MouseEvent& event)
{
    const bool fine = event.mods.isShiftDown() || event.mods.isCommandDown();
    setMouseDragSensitivity(fine ? 980 : 220);
    juce::Slider::mouseDown(event);
}

void GrokKnob::mouseDrag(const juce::MouseEvent& event)
{
    const bool fine = event.mods.isShiftDown() || event.mods.isCommandDown();
    setMouseDragSensitivity(fine ? 980 : 220);
    juce::Slider::mouseDrag(event);
}

void GrokKnob::mouseEnter(const juce::MouseEvent& event)
{
    hoverTarget = 1.0f;
    juce::Slider::mouseEnter(event);
}

void GrokKnob::mouseExit(const juce::MouseEvent& event)
{
    hoverTarget = 0.0f;
    juce::Slider::mouseExit(event);
}

void GrokKnob::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto labelArea = area.removeFromBottom(16.0f);
    auto dial = dialBounds();
    const float radius = dial.getWidth() * 0.5f;
    const auto centre = dial.getCentre();
    g.setColour(Theme::meterTrack());
    g.fillEllipse(dial);
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                        juce::MathConstants<float>::pi * 1.2f,
                        juce::MathConstants<float>::pi * 2.8f, true);
    g.setColour(Theme::accent().withAlpha(0.18f + hover * 0.12f));
    g.strokePath(track, juce::PathStrokeType(3.0f));
    const float angle = juce::MathConstants<float>::pi * 1.2f
                      + shown * juce::MathConstants<float>::pi * 1.6f;
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                        juce::MathConstants<float>::pi * 1.2f, angle, true);
    g.setColour(Theme::accent());
    g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(Theme::text());
    g.setFont(Theme::font(Theme::typeCaption, true));
    const auto text = formatValue(getValue(), parameter != nullptr ? parameter->getLabel() : juce::String(), isPan);
    g.drawFittedText(text, dial.reduced(5.0f).toNearestInt(), juce::Justification::centred, 1);

    g.setColour(Theme::textMuted());
    g.setFont(Theme::font(Theme::typeCaption));
    g.drawFittedText(label, labelArea.toNearestInt(), juce::Justification::centred, 1);

}

GrokChoice::GrokChoice(juce::String tip)
{
    setTooltip(std::move(tip));
    setWantsKeyboardFocus(false);
}

void GrokChoice::bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(paramId)))
    {
        clear(juce::dontSendNotification);
        addItemList(choice->choices, 1);
        if (getTooltip().isEmpty())
            setTooltip(choice->getName(80));
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, paramId, *this);
}

GrokToggle::GrokToggle(juce::String labelText, juce::String tip)
    : juce::ToggleButton(std::move(labelText))
{
    setTooltip(std::move(tip));
    setWantsKeyboardFocus(false);
    setClickingTogglesState(true);
}

void GrokToggle::bind(juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    if (getTooltip().isEmpty())
        if (auto* parameter = state.getParameter(paramId))
            setTooltip(parameter->getName(80));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, paramId, *this);
}

void GrokToggle::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();
    juce::Colour fill = on ? Theme::accent() : Theme::glassTop();
    if (down)
        fill = on ? Theme::accentPressed() : Theme::accentSoft();
    else if (highlighted)
        fill = on ? Theme::accent().brighter(0.06f) : Theme::accentSoft();
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, Theme::radiusS);
    g.setColour(on ? Theme::accentPressed() : Theme::accent().withAlpha(0.25f));
    g.drawRoundedRectangle(bounds, Theme::radiusS, 1.0f);
    g.setColour(on ? Theme::onAccent() : Theme::text());
    g.setFont(Theme::font(Theme::typeCaption, true));
    g.drawFittedText(getButtonText(), getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
}

GrokButton::GrokButton(juce::String labelText, juce::String tip)
    : juce::TextButton(std::move(labelText))
{
    setTooltip(std::move(tip));
    setWantsKeyboardFocus(false);
}

void GrokButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();
    juce::Colour fill = down ? Theme::accentPressed() : (on || highlighted ? Theme::accent() : Theme::accentSoft());
    if (on && highlighted)
        fill = Theme::accent().brighter(0.05f);
    g.setColour(Theme::shadow().withAlpha(0.16f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), Theme::radiusS);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, Theme::radiusS);
    g.setColour(on || down || highlighted ? Theme::onAccent() : Theme::text());
    g.setFont(Theme::font(Theme::typeBody, true));
    g.drawFittedText(getButtonText(), getLocalBounds(), juce::Justification::centred, 1);
}

DiceButton::DiceButton() : GrokButton({}, "Randomize unlocked layers by the strength amount.") {}

void DiceButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    GrokButton::paintButton(g, highlighted, down);
    const auto icon = getLocalBounds().toFloat().reduced(8.0f);
    const float side = std::min(icon.getWidth(), icon.getHeight());
    const auto die = juce::Rectangle<float>(side, side).withCentre(icon.getCentre()).reduced(1.0f);
    g.setColour(highlighted || down ? Theme::onAccent() : Theme::text());
    juce::Path outline;
    outline.addRoundedRectangle(die, 3.0f);
    g.strokePath(outline, juce::PathStrokeType(1.5f));
    juce::Path dots;
    const float dot = std::max(1.5f, side * 0.1f);
    for (int i = 0; i < 3; ++i)
        dots.addEllipse(die.getX() + die.getWidth() * (0.25f + 0.25f * i) - dot * 0.5f,
                        die.getY() + die.getHeight() * (0.25f + 0.25f * i) - dot * 0.5f, dot, dot);
    g.fillPath(dots);
}

SoundPad::SoundPad(int typeIndex, std::function<void(int)> onPick)
    : type(typeIndex), pick(std::move(onPick))
{
    setTooltip("Add " + juce::String(fxTypeName(type))
               + ". The strip along the bottom is a drop zone for your own sample. Custom samples are coming soon.");
    setRepaintsOnMouseActivity(false);
}

void SoundPad::setSelected(bool shouldSelect)
{
    if (selected != shouldSelect)
    {
        selected = shouldSelect;
        repaint();
    }
}

bool SoundPad::tick(float dt)
{
    const float nextHover = approach(hover, hoverTarget, dt);
    const float nextNotice = approach(notice, 0.0f, dt);
    const bool changed = nextHover != hover || nextNotice != notice;
    hover = nextHover;
    notice = nextNotice;
    if (changed)
        repaint();
    return changed;
}

void SoundPad::drawIcon(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto ink = selected ? Theme::onAccent() : Theme::accent();
    const float middle = area.getCentreY();
    const float maxHeight = area.getHeight() * 0.46f;
    g.setColour(ink.withAlpha(0.2f));
    g.drawHorizontalLine(juce::roundToInt(middle), area.getX(), area.getRight());

    juce::Path waveform;
    constexpr int bars = 32;
    for (int i = 0; i < bars; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(bars - 1);
        const float variation = 0.32f + 0.68f * std::abs(std::sin(t * 31.0f + std::sin(t * 13.0f)));
        float envelope = 0.65f;
        switch (type)
        {
            case 0: envelope = 0.12f + 0.88f * t; break;
            case 1: envelope = 1.0f - 0.88f * t; break;
            case 2: envelope = std::exp(-t * 5.0f); break;
            case 3: envelope = (0.25f + 0.75f * t) * (0.25f + 0.75f * std::abs(std::sin(t * (12.0f + t * 30.0f)))); break;
            case 4: envelope = 0.48f + 0.35f * variation; break;
            case 5: envelope = 0.1f + 0.9f * t * t; break;
            case 6: envelope = std::exp(-t * 2.5f) * (0.5f + 0.5f * std::abs(std::sin(t * (27.0f - 13.0f * t)))); break;
            default: break;
        }
        const float x = area.getX() + t * area.getWidth();
        const float height = std::max(0.8f, maxHeight * envelope * variation);
        waveform.startNewSubPath(x, middle - height);
        waveform.lineTo(x, middle + height);
    }
    g.setColour(ink);
    g.strokePath(waveform, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void SoundPad::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    juce::Colour fill = selected ? Theme::accent() : Theme::glassTop().withAlpha(0.88f);
    if (!selected && hover > 0.0f)
        fill = Theme::accentSoft().interpolatedWith(Theme::glassTop(), 1.0f - hover);
    g.setColour(Theme::shadow().withAlpha(0.12f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), Theme::radiusM);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, Theme::radiusM);
    g.setColour(selected ? Theme::accentPressed() : Theme::accent().withAlpha(0.25f + notice));
    g.drawRoundedRectangle(bounds, Theme::radiusM, notice > 0.05f ? 2.0f : 1.0f);

    auto content = getLocalBounds().reduced(Theme::space1);
    auto drop = content.removeFromBottom(28);
    drawIcon(g, content.removeFromTop(28).reduced(8, 4).toFloat());
    g.setColour(selected ? Theme::onAccent() : Theme::text());
    g.setFont(Theme::font(Theme::typeLabel, true));
    g.drawFittedText(fxTypeName(type), content, juce::Justification::centred, 2);

    g.setColour(selected ? Theme::onAccent().withAlpha(0.14f) : Theme::accentSoft());
    g.fillRoundedRectangle(drop.toFloat().reduced(2.0f, 1.0f), Theme::radiusS);
    g.setColour(selected ? Theme::onAccent() : Theme::textMuted());
    g.setFont(Theme::font(Theme::typeCaption));
    g.drawFittedText(notice > 0.4f ? "Coming soon" : "Drop a sample here", drop, juce::Justification::centred, 1);
}

void SoundPad::mouseEnter(const juce::MouseEvent&) { hoverTarget = 1.0f; }
void SoundPad::mouseExit(const juce::MouseEvent&) { hoverTarget = 0.0f; }
void SoundPad::mouseDown(const juce::MouseEvent&)
{
    if (pick != nullptr)
        pick(type);
}

void SoundPad::filesDropped(const juce::StringArray&, int, int)
{
    notice = 1.0f;
    repaint();
}

void WaveformView::setPeaks(const float* mins, const float* maxes, int peakCount, float dropFraction)
{
    count = std::clamp(peakCount, 0, waveformColumns);
    for (int i = 0; i < count; ++i)
    {
        minimums[i] = mins[i];
        maximums[i] = maxes[i];
    }
    drop = dropFraction;
}

void WaveformView::setPlayhead(float fraction, bool running)
{
    playhead = fraction;
    playing = running;
}

void WaveformView::setStatus(const juce::String& text)
{
    if (status == text)
        return;
    status = text;
    repaint();
}

bool WaveformView::tick(float dt)
{
    bool changed = playing;
    const float blend = 1.0f - std::pow(0.001f, dt / Theme::motionSeconds);
    for (int i = 0; i < count; ++i)
    {
        shownMin[i] += (minimums[i] - shownMin[i]) * blend;
        shownMax[i] += (maximums[i] - shownMax[i]) * blend;
    }
    repaint();
    return changed;
}

void WaveformView::mouseDown(const juce::MouseEvent&) { dragArmed = true; }

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (dragArmed && event.getDistanceFromDragStart() > 8 && onExternalDrag != nullptr)
    {
        dragArmed = false;
        onExternalDrag();
    }
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    if (dragArmed && onPreviewClick != nullptr)
    {
        const auto plot = getLocalBounds().toFloat().reduced(10.0f, 12.0f);
        if (plot.getWidth() > 0.0f)
            onPreviewClick(std::clamp((event.position.x - plot.getX()) / plot.getWidth(), 0.0f, 1.0f));
    }
    dragArmed = false;
}

void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::backgroundBottom());
    g.fillRoundedRectangle(bounds, Theme::radiusM);
    auto plot = bounds.reduced(10.0f, 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawLine(plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 1.0f);

    juce::Path wave;
    const float mid = plot.getCentreY();
    const float half = plot.getHeight() * 0.46f;
    for (int i = 0; i < count; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(std::max(1, count - 1));
        const float y = mid - shownMax[i] * half;
        if (i == 0)
            wave.startNewSubPath(x, y);
        else
            wave.lineTo(x, y);
    }
    for (int i = count - 1; i >= 0; --i)
    {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(std::max(1, count - 1));
        wave.lineTo(x, mid - shownMin[i] * half);
    }
    wave.closeSubPath();
    g.setColour(Theme::accent().withAlpha(0.85f));
    g.fillPath(wave);

    const float dropX = plot.getX() + plot.getWidth() * drop;
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawLine(dropX, plot.getY(), dropX, plot.getBottom(), 1.0f);
    if (playing || playhead > 0.0f)
    {
        const float playX = plot.getX() + plot.getWidth() * std::clamp(playhead, 0.0f, 1.0f);
        g.setColour(Theme::accentSoft());
        g.drawLine(playX, plot.getY(), playX, plot.getBottom(), 1.5f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.78f));
    g.setFont(Theme::font(Theme::typeCaption, true));
    auto caption = getLocalBounds().removeFromBottom(status.isNotEmpty() ? 32 : 18);
    g.drawFittedText(status.isNotEmpty() ? status : "Drag onto a track", caption, juce::Justification::centred, 2);
}

void OutputMeter::setLevels(float left, float right)
{
    targetL = std::clamp(left, 0.0f, 1.0f);
    targetR = std::clamp(right, 0.0f, 1.0f);
}

bool OutputMeter::tick(float dt)
{
    const float nextL = approach(shownL, targetL, dt);
    const float nextR = approach(shownR, targetR, dt);
    const bool changed = nextL != shownL || nextR != shownR;
    shownL = nextL;
    shownR = nextR;
    if (changed)
        repaint();
    return changed;
}

void OutputMeter::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto left = area.removeFromLeft(area.getWidth() * 0.5f).reduced(2.0f, 0.0f);
    auto right = area.reduced(2.0f, 0.0f);
    auto draw = [&](juce::Rectangle<float> lane, float level)
    {
        g.setColour(Theme::meterTrack());
        g.fillRoundedRectangle(lane, Theme::radiusS);
        auto fill = lane;
        fill = fill.removeFromBottom(lane.getHeight() * level);
        g.setColour(Theme::accent());
        g.fillRoundedRectangle(fill, Theme::radiusS);
    };
    draw(left, shownL);
    draw(right, shownR);
}

} // namespace grokfx
