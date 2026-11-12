#include "Support.h"
#include "ui/Panels.h"
#include "dsp/WavExport.h"

TEST_CASE("Layer tabs add and select enabled layers", "[layout]")
{
    auto plugin = makePlugin();
    grokfx::MixPanel panel(*plugin);
    panel.setBounds(0, 0, 580, 600);
    auto findButton = [&](const juce::String& text) -> grokfx::GrokButton*
    {
        for (auto* child : panel.getChildren())
            if (auto* button = dynamic_cast<grokfx::GrokButton*>(child))
                if (button->getButtonText() == text)
                    return button;
        return nullptr;
    };
    auto* add = findButton("+");
    auto* first = findButton("Layer 1");
    auto* second = findButton("Layer 2");
    REQUIRE(add != nullptr);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first->isVisible());
    REQUIRE_FALSE(second->isVisible());
    REQUIRE(add->onClick != nullptr);
    add->onClick();
    REQUIRE(getReal(*plugin, "l2_enabled") == Catch::Approx(1.0f));
    REQUIRE(second->isVisible());
    REQUIRE(plugin->getArmedLayer() == 1);
    first->onClick();
    REQUIRE(plugin->getArmedLayer() == 0);
}

TEST_CASE("Mix and drop controls fit without scrolling", "[layout]")
{
    auto plugin = makePlugin();
    grokfx::MixPanel mix(*plugin);
    grokfx::DropPanel drop(*plugin, [] {});
    mix.setBounds(0, 0, 568, 632);
    drop.setBounds(0, 0, 312, 632);
    auto check = [&](auto&& self, const juce::Component& component) -> void
    {
        REQUIRE(dynamic_cast<const juce::Viewport*>(&component) == nullptr);
        for (const auto* child : component.getChildren())
            if (child->isVisible())
            {
                REQUIRE(component.getLocalBounds().contains(child->getBounds()));
                self(self, *child);
            }
    };
    check(check, mix);
    check(check, drop);
}

TEST_CASE("Waveform handles change trim without starting a drag", "[layout][trim]")
{
    grokfx::WaveformView wave;
    wave.setBounds(0, 0, 220, 100);
    float start = 0.0f, end = 1.0f;
    bool externalDrag = false;
    wave.onTrimChange = [&](float a, float b) { start = a; end = b; };
    wave.onExternalDrag = [&]() { externalDrag = true; };
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto time = juce::Time::getCurrentTime();
    const juce::Point<float> down(10.0f, 50.0f), target(60.0f, 50.0f);
    const juce::MouseEvent downEvent(source, down, juce::ModifierKeys::leftButtonModifier,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &wave, &wave, time, down, time, 1, false);
    const juce::MouseEvent dragEvent(source, target, juce::ModifierKeys::leftButtonModifier,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &wave, &wave, time, down, time, 1, true);
    wave.mouseDown(downEvent);
    wave.mouseDrag(dragEvent);
    wave.mouseUp(dragEvent);
    REQUIRE(start == Catch::Approx(0.25f));
    REQUIRE(end == Catch::Approx(1.0f));
    REQUIRE_FALSE(externalDrag);
}

TEST_CASE("Dropping audio on the custom pad loads the armed layer", "[sample][layout]")
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("grokfx-pad", ".wav", false);
    juce::AudioBuffer<float> input(2, 1024);
    for (int i = 0; i < input.getNumSamples(); ++i)
        input.setSample(0, i, std::sin(static_cast<float>(i) * 0.1f) * 0.3f);
    input.copyFrom(1, 0, input, 0, 0, input.getNumSamples());
    REQUIRE(grokfx::writeStereoWav(file, input, 48000.0, grokfx::WavFormat::pcm24));
    auto plugin = makePlugin();
    grokfx::PickPanel panel(*plugin);
    grokfx::SoundPad* custom = nullptr;
    for (auto* child : panel.getChildren())
        if (auto* pad = dynamic_cast<grokfx::SoundPad*>(child)) custom = pad;
    REQUIRE(custom != nullptr);
    custom->filesDropped({ file.getFullPathName() }, 0, 0);
    REQUIRE(getReal(*plugin, "l1_useCustom") == Catch::Approx(1.0f));
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    juce::AudioBuffer<float> rendered;
    plugin->renderMix(rendered, 120.0, 48000.0);
    REQUIRE(maxAbs(rendered) > 0.01f);
    REQUIRE(file.deleteFile());
}
