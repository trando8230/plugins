#include "PluginEditor.h"
#include "PluginIdentity.h"

class GrokFxAudioProcessorEditor::PeakThread : public juce::Thread
{
public:
    explicit PeakThread(GrokFxAudioProcessor& processorIn)
        : juce::Thread("GrokFX Peaks"), processor(processorIn) {}

    void request()
    {
        dirty.store(true);
        notify();
    }

    bool consume(grokfx::WavePeaks& destination)
    {
        const juce::ScopedLock lock(mutex);
        if (!ready)
            return false;
        destination = latest;
        ready = false;
        return true;
    }

    juce::Array<juce::File> readyFiles() const
    {
        const juce::ScopedLock lock(mutex);
        return files;
    }

    void run() override
    {
        auto stop = [](void* context) -> bool
        {
            return static_cast<juce::Thread*>(context)->threadShouldExit();
        };

        while (!threadShouldExit())
        {
            if (!dirty.exchange(false))
            {
                wait(40);
                continue;
            }

            const auto model = processor.readModel();
            const double bpm = processor.getDisplayedBpm();
            constexpr double rate = 44100.0;
            grokfx::Engine engine;
            juce::AudioBuffer<float> buffer;
            const auto written = engine.renderOffline(buffer, model, bpm, rate, stop, this);
            if (threadShouldExit())
                return;
            if (written < buffer.getNumSamples() || dirty.load())
                continue;

            auto peaks = grokfx::Engine::peaksFor(buffer, grokfx::musicalSamples(model.length, bpm, rate));
            const double hostRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
            auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("GrokFX");
            auto exported = processor.renderExportFiles(directory, hostRate, stop, this);
            if (threadShouldExit())
                return;
            if (dirty.load())
                continue;

            const juce::ScopedLock lock(mutex);
            latest = peaks;
            ready = true;
            files = exported;
        }
    }

private:
    GrokFxAudioProcessor& processor;
    juce::CriticalSection mutex;
    grokfx::WavePeaks latest;
    juce::Array<juce::File> files;
    std::atomic<bool> dirty { false };
    bool ready = false;
};

GrokFxAudioProcessorEditor::GrokFxAudioProcessorEditor(GrokFxAudioProcessor& processorIn)
    : juce::AudioProcessorEditor(processorIn),
      audio(processorIn),
      pick(processorIn),
      mix(processorIn),
      drop(processorIn, [this]() { dragExport(); })
{
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    setName(grokfx::productName);

    logo.setText(grokfx::productName, juce::dontSendNotification);
    logo.setFont(grokfx::Theme::font(grokfx::Theme::typeLogo, true));
    logo.setColour(juce::Label::textColourId, grokfx::Theme::text());
    company.setText(grokfx::manufacturer, juce::dontSendNotification);
    company.setFont(grokfx::Theme::font(grokfx::Theme::typeCaption));
    company.setColour(juce::Label::textColourId, grokfx::Theme::textMuted());
    bpm.setFont(grokfx::Theme::font(grokfx::Theme::typeBody, true));
    bpm.setColour(juce::Label::textColourId, grokfx::Theme::accent());
    bpm.setJustificationType(juce::Justification::centredRight);
    bpm.setTooltip("Tempo from the host. Standalone and hosts without a playhead stay at 120 BPM.");

    content.addAndMakeVisible(logo);
    content.addAndMakeVisible(company);
    content.addAndMakeVisible(bpm);
    content.addAndMakeVisible(key);
    content.addAndMakeVisible(mode);
    content.addAndMakeVisible(length);
    content.addAndMakeVisible(undo);
    content.addAndMakeVisible(redo);
    content.addAndMakeVisible(compare);
    content.addAndMakeVisible(pick);
    content.addAndMakeVisible(mix);
    content.addAndMakeVisible(drop);
    addAndMakeVisible(content);

    key.bind(audio.apvts, "keyRoot");
    mode.bind(audio.apvts, "keyMode");
    length.bind(audio.apvts, "length");
    audio.apvts.state.addListener(this);
    for (auto* parameter : audio.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
            audio.apvts.addParameterListener(withId->paramID, this);
    undo.onClick = [this]() { audio.undoManager.undo(); };
    redo.onClick = [this]() { audio.undoManager.redo(); };
    compare.onClick = [this]()
    {
        audio.toggleAB();
        compare.setButtonText(audio.isShowingB() ? "B" : "A");
    };
    undo.setTooltip("Undo the last parameter edit.");
    redo.setTooltip("Redo a parameter edit.");
    compare.setTooltip("Switch between mix A and mix B.");

    pick.setComponentID("pick");
    mix.setComponentID("mix");
    drop.setComponentID("drop");

    constrainer.setFixedAspectRatio(static_cast<double>(grokfx::Theme::baseW) / static_cast<double>(grokfx::Theme::baseH));
    constrainer.setMinimumSize(grokfx::Theme::minW, grokfx::Theme::minH);
    constrainer.setMaximumSize(grokfx::Theme::maxW, grokfx::Theme::maxH);
    setConstrainer(&constrainer);
    setResizable(true, true);
    applyScale(audio.getUiScale());

    peaks = std::make_unique<PeakThread>(audio);
    peaks->startThread();
    peaks->request();
    startTimerHz(60);
}

GrokFxAudioProcessorEditor::~GrokFxAudioProcessorEditor()
{
    audio.apvts.state.removeListener(this);
    for (auto* parameter : audio.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
            audio.apvts.removeParameterListener(withId->paramID, this);
    stopTimer();
    if (peaks != nullptr)
    {
        peaks->signalThreadShouldExit();
        peaks->notify();
        peaks->stopThread(-1);
    }
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void GrokFxAudioProcessorEditor::applyScale(float scale)
{
    const float clamped = std::clamp(scale, 0.75f, 2.0f);
    applyingScale = true;
    setSize(juce::roundToInt(static_cast<float>(grokfx::Theme::baseW) * clamped),
            juce::roundToInt(static_cast<float>(grokfx::Theme::baseH) * clamped));
    applyingScale = false;
}

void GrokFxAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient wash(grokfx::Theme::backgroundTop(), 0.0f, 0.0f,
                              grokfx::Theme::backgroundBottom(), 0.0f, static_cast<float>(getHeight()), false);
    wash.addColour(0.45, grokfx::Theme::backgroundMid());
    g.setGradientFill(wash);
    g.fillAll();
    if (blur.isValid())
        g.setOpacity(0.55f), g.drawImage(blur, getLocalBounds().toFloat());
    g.setOpacity(1.0f);
}

void GrokFxAudioProcessorEditor::rebuildBlur()
{
    blurScale = std::max(1.0f, static_cast<float>(getDesktopScaleFactor()));
    const int width = std::max(1, juce::roundToInt(static_cast<float>(getWidth()) * blurScale));
    const int height = std::max(1, juce::roundToInt(static_cast<float>(getHeight()) * blurScale));
    blur = juce::Image(juce::Image::ARGB, width, height, true);
    juce::Graphics imageGraphics(blur);
    imageGraphics.addTransform(juce::AffineTransform::scale(blurScale));
    juce::ColourGradient wash(grokfx::Theme::backgroundTop(), 0.0f, 0.0f,
                              grokfx::Theme::backgroundBottom(), 0.0f, static_cast<float>(getHeight()), false);
    wash.addColour(0.5, grokfx::Theme::backgroundMid().brighter(0.08f));
    imageGraphics.setGradientFill(wash);
    imageGraphics.fillAll();
    imageGraphics.setColour(grokfx::Theme::accent().withAlpha(0.18f));
    imageGraphics.fillEllipse(static_cast<float>(getWidth()) * 0.55f, -40.0f, 420.0f, 280.0f);
    imageGraphics.setColour(juce::Colours::white.withAlpha(0.4f));
    imageGraphics.fillEllipse(-80.0f, static_cast<float>(getHeight()) * 0.42f, 360.0f, 240.0f);

    juce::ImageConvolutionKernel kernel(17);
    kernel.createGaussianBlur(8.0f);
    kernel.applyToImage(blur, blur, blur.getBounds());
}

void GrokFxAudioProcessorEditor::resized()
{
    if (!applyingScale && getWidth() > 0)
        audio.setUiScale(static_cast<float>(getWidth()) / static_cast<float>(grokfx::Theme::baseW));

    const float uiScale = static_cast<float>(getWidth()) / static_cast<float>(grokfx::Theme::baseW);
    content.setBounds(0, 0, grokfx::Theme::baseW, grokfx::Theme::baseH);
    content.setTransform(juce::AffineTransform::scale(std::max(0.01f, uiScale)));

    auto area = content.getLocalBounds().reduced(grokfx::Theme::space2);
    auto header = area.removeFromTop(grokfx::Theme::headerH);
    auto brand = header.removeFromLeft(188);
    logo.setBounds(brand.removeFromTop(36));
    company.setBounds(brand);
    compare.setBounds(header.removeFromRight(56).reduced(4, 16));
    redo.setBounds(header.removeFromRight(72).reduced(4, 16));
    undo.setBounds(header.removeFromRight(72).reduced(4, 16));
    bpm.setBounds(header.removeFromRight(88).reduced(4, 16));
    length.setBounds(header.removeFromRight(128).reduced(4, 16));
    mode.setBounds(header.removeFromRight(104).reduced(4, 16));
    key.setBounds(header.removeFromRight(80).reduced(4, 16));

    area.removeFromTop(grokfx::Theme::space2);
    pick.setBounds(area.removeFromLeft(grokfx::Theme::pickW));
    area.removeFromLeft(grokfx::Theme::space2);
    drop.setBounds(area.removeFromRight(grokfx::Theme::dropW));
    area.removeFromRight(grokfx::Theme::space2);
    mix.setBounds(area);

    rebuildBlur();
}

void GrokFxAudioProcessorEditor::tickTree(juce::Component& component, float dt)
{
    if (auto* knob = dynamic_cast<grokfx::GrokKnob*>(&component))
        knob->tick(dt);
    if (auto* meter = dynamic_cast<grokfx::OutputMeter*>(&component))
        meter->setLevels(audio.getMeterL(), audio.getMeterR()), meter->tick(dt);
    if (auto* wave = dynamic_cast<grokfx::WaveformView*>(&component))
        wave->setPlayhead(audio.getPlayFraction(), audio.isPreviewRunning()), wave->tick(dt);
    for (auto* child : component.getChildren())
        if (child != nullptr)
            tickTree(*child, dt);
}

void GrokFxAudioProcessorEditor::timerCallback()
{
    tickTree(*this, 1.0f / 60.0f);
    pick.tick(1.0f / 60.0f);
    pick.refresh();
    mix.refresh();
    bpm.setText(juce::String(juce::roundToInt(audio.getDisplayedBpm())) + " BPM", juce::dontSendNotification);
    drop.refreshTransport(audio.getDisplayedBpm(), audio.isPreviewRunning());
    compare.setButtonText(audio.isShowingB() ? "B" : "A");
    undo.setEnabled(audio.undoManager.canUndo());
    redo.setEnabled(audio.undoManager.canRedo());

    if (peaks != nullptr)
    {
        grokfx::WavePeaks latest;
        if (peaks->consume(latest))
            drop.waveform().setPeaks(latest.minColumn, latest.maxColumn, latest.columns, latest.dropFraction);
    }

    const auto exportError = audio.lastExportError();
    drop.waveform().setStatus(exportError);
    if (exportError.isNotEmpty())
        drop.waveform().setTooltip(exportError);
}

void GrokFxAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    if (peaks != nullptr)
        peaks->request();
}

void GrokFxAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)
{
    if (peaks != nullptr)
        peaks->request();
}

void GrokFxAudioProcessorEditor::renderWaveformSync()
{
    const auto model = audio.readModel();
    const double bpmValue = audio.getDisplayedBpm();
    constexpr double rate = 44100.0;
    grokfx::Engine engine;
    juce::AudioBuffer<float> buffer;
    engine.renderOffline(buffer, model, bpmValue, rate);
    const auto peaksNow = grokfx::Engine::peaksFor(buffer, grokfx::musicalSamples(model.length, bpmValue, rate));
    drop.waveform().setPeaks(peaksNow.minColumn, peaksNow.maxColumn, peaksNow.columns, peaksNow.dropFraction);
    drop.waveform().tick(1.0f);
}

juce::Array<juce::File> GrokFxAudioProcessorEditor::readyDragFiles() const
{
    return peaks != nullptr ? peaks->readyFiles() : juce::Array<juce::File>{};
}

void GrokFxAudioProcessorEditor::dragExport()
{
    const auto files = readyDragFiles();
    juce::StringArray paths;
    for (const auto& file : files)
        if (file.existsAsFile())
            paths.add(file.getFullPathName());
    if (paths.isEmpty())
        return;
    performExternalDragDropOfFiles(paths, false, this);
}
