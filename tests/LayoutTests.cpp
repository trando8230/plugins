#include "Support.h"
#include "ui/Panels.h"

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
