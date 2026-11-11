#include "Support.h"
#include "ui/Widgets.h"

TEST_CASE("Knob values show meaningful units", "[knob]")
{
    using grokfx::GrokKnob;
    REQUIRE(GrokKnob::formatValue(0.35, "%", false) == "35%");
    REQUIRE(GrokKnob::formatValue(-12, "st", false) == "-12 st");
    REQUIRE(GrokKnob::formatValue(25, "ct", false) == "25 ct");
    REQUIRE(GrokKnob::formatValue(0, "%", true) == "C");
    REQUIRE(GrokKnob::formatValue(-0.5, "%", true) == "50 L");
    REQUIRE(GrokKnob::formatValue(1, "%", true) == "100 R");
}

TEST_CASE("Knob wheel responds only inside its circular dial", "[knob]")
{
    grokfx::GrokKnob knob("Test", "Adjust test value.");
    knob.setBounds(0, 0, 80, 80);
    knob.setRange(0.0, 100.0, 1.0);
    knob.setValue(50.0);
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto time = juce::Time::getCurrentTime();
    auto eventAt = [&](juce::Point<float> point)
    {
        return juce::MouseEvent(source, point, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                &knob, &knob, time, point, time, 0, false);
    };
    juce::MouseWheelDetails wheel;
    wheel.deltaY = 1.0f;
    knob.mouseWheelMove(eventAt({ 10.0f, 2.0f }), wheel);
    REQUIRE(knob.getValue() == 50.0);
    knob.mouseWheelMove(eventAt({ 40.0f, 32.0f }), wheel);
    REQUIRE(knob.getValue() != 50.0);
}
