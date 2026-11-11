#include "PluginEditor.h"
#include "ui/Theme.h"

namespace
{

juce::Image snapshotOf(juce::Component& component)
{
    const int width = std::max(1, component.getWidth());
    const int height = std::max(1, component.getHeight());
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics graphics(image);
    component.paintEntireComponent(graphics, true);
    return image;
}

bool imageHasDetail(const juce::Image& image)
{
    if (!image.isValid() || image.getWidth() < 8 || image.getHeight() < 8)
        return false;
    juce::int64 total = 0;
    juce::int64 totalSq = 0;
    const int stepX = std::max(1, image.getWidth() / 48);
    const int stepY = std::max(1, image.getHeight() / 48);
    int count = 0;
    for (int y = 0; y < image.getHeight(); y += stepY)
        for (int x = 0; x < image.getWidth(); x += stepX)
        {
            const auto pixel = image.getPixelAt(x, y);
            const int luma = pixel.getRed() + pixel.getGreen() + pixel.getBlue();
            total += luma;
            totalSq += static_cast<juce::int64>(luma) * luma;
            ++count;
        }
    if (count == 0)
        return false;
    const double mean = static_cast<double>(total) / static_cast<double>(count);
    const double variance = static_cast<double>(totalSq) / static_cast<double>(count) - mean * mean;
    return variance > 20.0;
}

juce::Component* findById(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* found = findById(*child, id))
                return found;
    return nullptr;
}

juce::Image cropPanel(const juce::Image& full, juce::Component& panel, float scale)
{
    auto bounds = panel.getBounds();
    if (auto* parent = panel.getParentComponent())
        bounds = parent->getLocalArea(&panel, panel.getLocalBounds());
    const int x = juce::roundToInt(static_cast<float>(bounds.getX()) * scale);
    const int y = juce::roundToInt(static_cast<float>(bounds.getY()) * scale);
    const int w = std::max(1, juce::roundToInt(static_cast<float>(bounds.getWidth()) * scale));
    const int h = std::max(1, juce::roundToInt(static_cast<float>(bounds.getHeight()) * scale));
    return full.getClippedImage(juce::Rectangle<int>(x, y, w, h).getIntersection(full.getBounds()));
}

bool writePng(const juce::File& file, const juce::Image& image)
{
    file.getParentDirectory().createDirectory();
    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return false;
    juce::PNGImageFormat format;
    return format.writeImageToStream(image, stream);
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File output = argc > 1
        ? juce::File(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("screenshots");
    output.createDirectory();

    auto plugin = std::unique_ptr<GrokFxAudioProcessor>(
        dynamic_cast<GrokFxAudioProcessor*>(createPluginFilter()));
    if (plugin == nullptr)
        return 1;

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
    auto* grok = dynamic_cast<GrokFxAudioProcessorEditor*>(editor.get());
    if (grok == nullptr)
        return 1;

    const struct Scale { const char* name; int width; int height; float factor; } scales[] = {
        { "100", grokfx::Theme::baseW, grokfx::Theme::baseH, 1.0f },
        { "200", grokfx::Theme::baseW * 2, grokfx::Theme::baseH * 2, 2.0f }
    };

    for (const auto& scale : scales)
    {
        editor->setSize(scale.width, scale.height);
        grok->renderWaveformSync();
        const auto full = snapshotOf(*editor);
        if (!writePng(output.getChildFile("grokfx-" + juce::String(scale.name) + "-full.png"), full) || !imageHasDetail(full))
            return 2;

        if (auto* pick = findById(*editor, "pick"))
        {
            const auto image = cropPanel(full, *pick, scale.factor);
            if (!writePng(output.getChildFile("grokfx-" + juce::String(scale.name) + "-pick.png"), image) || !imageHasDetail(image))
                return 3;
        }
        if (auto* mix = findById(*editor, "mix"))
        {
            const auto image = cropPanel(full, *mix, scale.factor);
            if (!writePng(output.getChildFile("grokfx-" + juce::String(scale.name) + "-mix.png"), image) || !imageHasDetail(image))
                return 4;
        }
        if (auto* drop = findById(*editor, "drop"))
        {
            const auto image = cropPanel(full, *drop, scale.factor);
            if (!writePng(output.getChildFile("grokfx-" + juce::String(scale.name) + "-drop.png"), image) || !imageHasDetail(image))
                return 5;
        }
    }

    return 0;
}
