#include "LookAndFeel.h"
#include "BinaryData.h"

namespace grokfx
{

GrokLookAndFeel::GrokLookAndFeel()
{
    regular = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    medium = juce::Typeface::createSystemTypefaceFor(BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    semibold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);

    setColour(juce::ComboBox::backgroundColourId, Theme::glassTop());
    setColour(juce::ComboBox::textColourId, Theme::text());
    setColour(juce::ComboBox::outlineColourId, Theme::border());
    setColour(juce::ComboBox::arrowColourId, Theme::accent());
    setColour(juce::PopupMenu::backgroundColourId, Theme::backgroundTop());
    setColour(juce::PopupMenu::textColourId, Theme::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent());
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::onAccent());
    setColour(juce::Label::textColourId, Theme::text());
    setColour(juce::ScrollBar::thumbColourId, Theme::accent().withAlpha(0.45f));
    setColour(juce::TooltipWindow::backgroundColourId, Theme::backgroundTop());
    setColour(juce::TooltipWindow::textColourId, Theme::text());
    setColour(juce::TooltipWindow::outlineColourId, Theme::accent().withAlpha(0.35f));
    setColour(juce::TextButton::textColourOffId, Theme::text());
    setColour(juce::TextButton::textColourOnId, Theme::onAccent());
}

juce::Typeface::Ptr GrokLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    const auto style = font.getTypefaceStyle();
    if (style.containsIgnoreCase("Bold") || font.getHeight() >= Theme::typeTitle)
        return semibold != nullptr ? semibold : regular;
    if (font.getHeight() >= Theme::typeSection)
        return medium != nullptr ? medium : regular;
    return regular;
}

juce::Font GrokLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return Theme::font(Theme::typeBody);
}

juce::Font GrokLookAndFeel::getPopupMenuFont()
{
    return Theme::font(Theme::typeBody);
}

juce::Font GrokLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return Theme::font(Theme::typeBody, true);
}

void GrokLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(box.isEnabled() ? Theme::glassTop() : Theme::glassBottom());
    g.fillRoundedRectangle(bounds, Theme::radiusS);
    g.setColour(box.hasKeyboardFocus(true) ? Theme::accent() : Theme::accent().withAlpha(0.28f));
    g.drawRoundedRectangle(bounds, Theme::radiusS, 1.0f);
    juce::Path arrow;
    const float cx = static_cast<float>(width) - 16.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(Theme::accent());
    g.fillPath(arrow);
}

void GrokLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(Theme::backgroundTop());
    g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), Theme::radiusM);
    g.setColour(Theme::accent().withAlpha(0.25f));
    g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f, Theme::radiusM, 1.0f);
}

void GrokLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                       bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                       const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(Theme::accent().withAlpha(0.15f));
        g.fillRect(area.reduced(Theme::space1, 0).withHeight(1).withY(area.getCentreY()));
        return;
    }

    auto row = area.reduced(4, 2).toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour(Theme::accent());
        g.fillRoundedRectangle(row, Theme::radiusS);
    }
    g.setColour(isHighlighted && isActive ? Theme::onAccent() : (isActive ? Theme::text() : Theme::textMuted()));
    g.setFont(Theme::font(Theme::typeBody, isTicked));
    g.drawFittedText(text, area.reduced(Theme::space2, 0), juce::Justification::centredLeft, 1);
    if (hasSubMenu)
        juce::ignoreUnused(hasSubMenu);
}

void GrokLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    juce::Colour fill = on ? Theme::accent() : Theme::glassTop();
    if (down)
        fill = on ? Theme::accentPressed() : Theme::accentSoft();
    else if (highlighted)
        fill = on ? Theme::accent().brighter(0.08f) : Theme::accentSoft();
    g.setColour(Theme::shadow().withAlpha(0.18f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), Theme::radiusS);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, Theme::radiusS);
    g.setColour(on ? Theme::accentPressed() : Theme::border());
    g.drawRoundedRectangle(bounds, Theme::radiusS, 1.0f);
}

void GrokLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                   bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<float> track(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
    g.setColour(Theme::meterTrack());
    g.fillRoundedRectangle(track.reduced(2.0f), Theme::radiusS);
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { track.getX() + 2.0f, static_cast<float>(thumbStartPosition), track.getWidth() - 4.0f, static_cast<float>(thumbSize) };
    else
        thumb = { static_cast<float>(thumbStartPosition), track.getY() + 2.0f, static_cast<float>(thumbSize), track.getHeight() - 4.0f };
    g.setColour(Theme::accent().withAlpha(isMouseDown ? 0.9f : (isMouseOver ? 0.7f : 0.45f)));
    g.fillRoundedRectangle(thumb, Theme::radiusS);
}

} // namespace grokfx
