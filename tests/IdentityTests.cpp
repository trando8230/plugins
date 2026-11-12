#include "Support.h"
#include "PluginIdentity.h"

TEST_CASE("Plugin identity codes stay stable", "[identity]")
{
    REQUIRE(std::string(grokfx::productName) == "Grok FX");
    REQUIRE(std::string(grokfx::manufacturer) == "Grok FX");
    REQUIRE(std::string(grokfx::bundleId) == "com.randoaudio.grokfx");
    REQUIRE(std::string(grokfx::pluginCode) == "Grok");
    REQUIRE(std::string(grokfx::manufacturerCode) == "Raud");
    REQUIRE(grokfx::textLength(grokfx::pluginCode) == 4);
    REQUIRE(grokfx::textLength(grokfx::manufacturerCode) == 4);
    REQUIRE(grokfx::hasUpper(grokfx::pluginCode));
    REQUIRE(grokfx::hasUpper(grokfx::manufacturerCode));

    auto plugin = makePlugin();
    REQUIRE(plugin->getName() == "Grok FX");
    REQUIRE(plugin->acceptsMidi());
    REQUIRE(plugin->hasEditor());
}
