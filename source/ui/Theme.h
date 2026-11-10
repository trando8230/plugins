#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace grokfx
{

// 8 px grid, one accent, Inter via the LookAndFeel. Components read sizes and
// colours from here instead of hard-coding them.
struct Theme
{
    static constexpr int grid = 8;
    static constexpr int baseW = 1200;
    static constexpr int baseH = 744;
    static constexpr int minW = 900;
    static constexpr int minH = 558;
    static constexpr int maxW = 2400;
    static constexpr int maxH = 1488;
    static constexpr int headerH = 64;
    static constexpr int pickW = 248;
    static constexpr int dropW = 312;
    static constexpr int space1 = 8;
    static constexpr int space2 = 16;
    static constexpr int space3 = 24;
    static constexpr int space4 = 32;
    static constexpr int space5 = 40;
    static constexpr int space6 = 48;
    static constexpr float radiusS = 8.0f;
    static constexpr float radiusM = 12.0f;
    static constexpr float radiusL = 16.0f;
    static constexpr float radiusXL = 24.0f;
    static constexpr float typeCaption = 11.0f;
    static constexpr float typeLabel = 12.0f;
    static constexpr float typeBody = 13.0f;
    static constexpr float typeSection = 16.0f;
    static constexpr float typeTitle = 22.0f;
    static constexpr float typeLogo = 28.0f;
    static constexpr float motionSeconds = 0.16f;
    static constexpr int knob = 56;
    static constexpr int knobCellW = 72;
    static constexpr int knobCellH = 80;
    static constexpr int controlH = 32;

    static juce::Colour backgroundTop() { return juce::Colour::fromRGB(244, 250, 255); }
    static juce::Colour backgroundMid() { return juce::Colour::fromRGB(215, 236, 248); }
    static juce::Colour backgroundBottom() { return juce::Colour::fromRGB(185, 212, 234); }
    static juce::Colour glassTop() { return juce::Colour::fromRGBA(255, 255, 255, 196); }
    static juce::Colour glassBottom() { return juce::Colour::fromRGBA(232, 244, 252, 168); }
    static juce::Colour border() { return juce::Colour::fromRGBA(255, 255, 255, 226); }
    static juce::Colour highlight() { return juce::Colour::fromRGBA(255, 255, 255, 242); }
    static juce::Colour shadow() { return juce::Colour::fromRGBA(22, 48, 68, 38); }
    static juce::Colour text() { return juce::Colour::fromRGB(22, 48, 68); }
    static juce::Colour textMuted() { return juce::Colour::fromRGB(62, 98, 120); }
    static juce::Colour accent() { return juce::Colour::fromRGB(31, 122, 224); }
    static juce::Colour accentSoft() { return juce::Colour::fromRGB(214, 233, 250); }
    static juce::Colour meterTrack() { return juce::Colour::fromRGBA(22, 48, 68, 32); }

    static juce::Colour accentPressed() { return accent().darker(0.28f); }

    static juce::Font font(float size, bool emphasis = false)
    {
        auto options = juce::FontOptions().withHeight(size);
        if (emphasis)
            options = options.withStyle("Bold");
        return juce::Font(options);
    }

    static float ease(float amount) noexcept
    {
        const float t = std::clamp(amount, 0.0f, 1.0f);
        return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    }
};

} // namespace grokfx
