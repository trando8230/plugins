#include "Panels.h"
#include "parameters/Parameters.h"

namespace grokfx
{
namespace
{

void placeRow(juce::Rectangle<int>& area, const std::vector<juce::Component*>& controls, int height)
{
    if (controls.empty())
        return;
    auto row = area.removeFromTop(height);
    const int width = row.getWidth() / static_cast<int>(controls.size());
    for (auto* control : controls)
    {
        auto cell = row.removeFromLeft(width).reduced(4, 2);
        control->setBounds(cell);
    }
}

} // namespace

PickPanel::PickPanel(GrokFxAudioProcessor& processor)
    : audio(processor)
{
    title.setFont(Theme::font(Theme::typeSection, true));
    title.setColour(juce::Label::textColourId, Theme::text());
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    const char* bankNames[] = { "Classic", "Analog", "Digital", "Vocal" };
    for (int bank = 0; bank < 4; ++bank)
    {
        auto* button = banks.add(new GrokButton(bankNames[bank], juce::String(bankNames[bank]) + " bank for the armed layer."));
        addAndMakeVisible(button);
        button->onClick = [this, bank]()
        {
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(audio.apvts.getParameter(layerParam(audio.getArmedLayer(), "bank"))))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(bank)));
                parameter->endChangeGesture();
            }
            refresh();
        };
    }

    for (int type = 0; type <= static_cast<int>(FxType::count); ++type)
    {
        auto* pad = pads.add(new SoundPad(type, [this](int picked)
        {
            const int layer = audio.getArmedLayer();
            if (picked == static_cast<int>(FxType::count))
            {
                const auto path = audio.apvts.state.getProperty("samplePath" + juce::String(layer + 1)).toString();
                if (path.isEmpty() || !juce::File(path).existsAsFile()) { chooseSample(); return; }
                if (auto* use = audio.apvts.getParameter(layerParam(layer, "useCustom")))
                {
                    use->beginChangeGesture(); use->setValueNotifyingHost(1.0f); use->endChangeGesture();
                }
                if (auto* enabled = audio.apvts.getParameter(layerParam(layer, "enabled")))
                {
                    enabled->beginChangeGesture(); enabled->setValueNotifyingHost(1.0f); enabled->endChangeGesture();
                }
                refresh();
                return;
            }
            if (auto* use = audio.apvts.getParameter(layerParam(layer, "useCustom")))
            {
                use->beginChangeGesture(); use->setValueNotifyingHost(0.0f); use->endChangeGesture();
            }
            if (auto* typeParam = dynamic_cast<juce::RangedAudioParameter*>(audio.apvts.getParameter(layerParam(layer, "type"))))
            {
                typeParam->beginChangeGesture();
                typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(picked)));
                typeParam->endChangeGesture();
            }
            if (auto* enabled = dynamic_cast<juce::RangedAudioParameter*>(audio.apvts.getParameter(layerParam(layer, "enabled"))))
            {
                enabled->beginChangeGesture();
                enabled->setValueNotifyingHost(1.0f);
                enabled->endChangeGesture();
            }
            refresh();
        }, [this](int picked, const juce::File& file) { loadSample(picked, file); }));
        addAndMakeVisible(pad);
    }
    refresh();
}

void PickPanel::chooseSample()
{
    sampleChooser = std::make_unique<juce::FileChooser>("Choose an audio sample", juce::File(), "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<PickPanel> safeThis(this);
    sampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis != nullptr && chooser.getResult() != juce::File())
                safeThis->loadSample(static_cast<int>(FxType::count), chooser.getResult());
        });
}

void PickPanel::loadSample(int type, const juce::File& file)
{
    const int layer = audio.getArmedLayer();
    const auto result = audio.loadSample(layer, file);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Sample could not be loaded", result.getErrorMessage());
        return;
    }
    if (auto* use = audio.apvts.getParameter(layerParam(layer, "useCustom")))
    {
        use->beginChangeGesture(); use->setValueNotifyingHost(1.0f); use->endChangeGesture();
    }
    if (auto* enabled = audio.apvts.getParameter(layerParam(layer, "enabled")))
    {
        enabled->beginChangeGesture(); enabled->setValueNotifyingHost(1.0f); enabled->endChangeGesture();
    }
    if (type < static_cast<int>(FxType::count))
        if (auto* typeParam = audio.apvts.getParameter(layerParam(layer, "type")))
        {
            typeParam->beginChangeGesture();
            typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(type)));
            typeParam->endChangeGesture();
        }
    refresh();
}

void PickPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::space2);
    title.setBounds(area.removeFromTop(Theme::space3));
    area.removeFromTop(Theme::space1);
    auto bankRow = area.removeFromTop(Theme::controlH);
    const int bankW = bankRow.getWidth() / std::max(1, banks.size());
    for (auto* bank : banks)
        bank->setBounds(bankRow.removeFromLeft(bankW).reduced(2, 0));
    area.removeFromTop(Theme::space1);

    const int columns = 2;
    const int rows = (pads.size() + columns - 1) / columns;
    const int gap = Theme::space1;
    const int padW = (area.getWidth() - gap) / columns;
    const int padH = rows > 0 ? (area.getHeight() - gap * (rows - 1)) / rows : Theme::space6;
    for (int i = 0; i < pads.size(); ++i)
    {
        const int column = i % columns;
        const int row = i / columns;
        pads[i]->setBounds(area.getX() + column * (padW + gap), area.getY() + row * (padH + gap), padW, padH);
    }
}

void PickPanel::refresh()
{
    const int layer = audio.getArmedLayer();
    int type = 0;
    int bank = 0;
    if (auto* parameter = audio.apvts.getRawParameterValue(layerParam(layer, "type")))
        type = juce::roundToInt(parameter->load());
    if (auto* custom = audio.apvts.getRawParameterValue(layerParam(layer, "useCustom")); custom != nullptr && custom->load() >= 0.5f)
        type = static_cast<int>(FxType::count);
    if (auto* parameter = audio.apvts.getRawParameterValue(layerParam(layer, "bank")))
        bank = juce::roundToInt(parameter->load());
    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setSelected(i == type);
    for (int i = 0; i < banks.size(); ++i)
        banks[i]->setToggleState(i == bank, juce::dontSendNotification);
}

bool PickPanel::tick(float dt)
{
    bool changed = false;
    for (auto* pad : pads)
        changed = pad->tick(dt) || changed;
    return changed;
}

class MixPanel::Card : public juce::Component
{
public:
    Card(GrokFxAudioProcessor& processor, int layerIndex)
        : audio(processor), index(layerIndex)
    {
        auto bindKnob = [this](GrokKnob& knob, const char* suffix)
        {
            addAndMakeVisible(knob);
            knob.bind(audio.apvts, layerParam(index, suffix));
        };
        auto bindChoice = [this](GrokChoice& choice, const char* suffix)
        {
            addAndMakeVisible(choice);
            choice.bind(audio.apvts, layerParam(index, suffix));
        };
        auto bindToggle = [this](GrokToggle& toggle, const char* suffix)
        {
            addAndMakeVisible(toggle);
            toggle.bind(audio.apvts, layerParam(index, suffix));
        };

        bindChoice(type, "type");
        bindChoice(sampleRoot, "sampleRoot");
        bindToggle(matchKey, "matchKey");
        addAndMakeVisible(detectedRoot);
        detectedRoot.setFont(Theme::font(12.0f));
        detectedRoot.setTooltip("Estimated root of the loaded sample; choose a note to override it, or Unpitched for drums and noise.");
        bindChoice(bank, "bank");
        bindToggle(on, "enabled");
        bindToggle(solo, "solo");
        bindToggle(mute, "mute");
        bindToggle(lock, "locked");
        bindToggle(reverse, "reverse");
        bindKnob(volume, "volume");
        bindKnob(pan, "pan");
        bindKnob(pitch, "pitch");
        bindKnob(fine, "fine");
        bindKnob(offset, "offset");
        bindKnob(fadeIn, "fadeIn");
        bindKnob(fadeOut, "fadeOut");
        bindKnob(drive, "drive");
        bindKnob(width, "width");
        bindKnob(reverb, "reverbSend");
        bindKnob(delay, "delaySend");
        bindKnob(cutStart, "cutoffStart");
        bindKnob(cutEnd, "cutoffEnd");
        bindKnob(resonance, "resonance");
        bindKnob(swing, "swing");
        bindKnob(velStart, "velStart");
        bindKnob(velEnd, "velEnd");
        bindChoice(shape, "shape");
        bindChoice(fadeInCurve, "fadeInCurve");
        bindChoice(fadeOutCurve, "fadeOutCurve");
        bindChoice(filter, "filterType");
        bindChoice(rollStart, "rollStart");
        bindChoice(rollEnd, "rollEnd");
        bindChoice(rollAccel, "rollAccel");
        bindChoice(velCurve, "velCurve");
        addAndMakeVisible(sampleWave);
        sampleWave.setCaption("Drag handles to trim this layer");
        sampleWave.setTooltip("Drag the handles to choose this layer's sample start and end; click to audition.");
        sampleWave.onPreviewClick = [this](float fraction)
        {
            auto* start = audio.apvts.getRawParameterValue(layerParam(index, "sampleStart"));
            auto* end = audio.apvts.getRawParameterValue(layerParam(index, "sampleEnd"));
            auto* reverseParam = audio.apvts.getRawParameterValue(layerParam(index, "reverse"));
            const float first = start != nullptr ? start->load() : 0.0f;
            const float last = end != nullptr ? end->load() : 1.0f;
            float position = std::clamp((fraction - first) / std::max(0.001f, last - first), 0.0f, 1.0f);
            if (reverseParam != nullptr && reverseParam->load() >= 0.5f) position = 1.0f - position;
            audio.togglePreviewAt(position);
        };
        sampleWave.onTrimChange = [this](float start, float end)
        {
            for (const auto& update : { std::pair { "sampleStart", start }, std::pair { "sampleEnd", end } })
                if (auto* parameter = audio.apvts.getParameter(layerParam(index, update.first)))
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(update.second);
                    parameter->endChangeGesture();
                }
        };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const bool armed = audio.getArmedLayer() == index;
        g.setColour(armed ? Theme::accent().withAlpha(0.16f) : Theme::glassTop().withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, Theme::radiusM);
        g.setColour(armed ? Theme::accent() : Theme::accent().withAlpha(0.18f));
        g.drawRoundedRectangle(bounds, Theme::radiusM, armed ? 1.5f : 1.0f);
        g.setColour(Theme::text());
        g.setFont(Theme::font(Theme::typeBody, true));
        const auto* custom = audio.apvts.getRawParameterValue(layerParam(index, "useCustom"));
        const bool usingCustom = custom != nullptr && custom->load() >= 0.5f;
        g.drawFittedText("Layer " + juce::String(index + 1) + (usingCustom ? "  •  Custom sample" : ""), getLocalBounds().reduced(Theme::space2).removeFromTop(24),
                         juce::Justification::centredLeft, 1);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.eventComponent == this)
            audio.setArmedLayer(index);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(Theme::space1);
        area.removeFromTop(24);
        std::vector<juce::Component*> choices { &type, &bank };
        placeRow(area, choices, Theme::controlH);
        std::vector<juce::Component*> toggles { &on, &solo, &mute, &lock, &reverse };
        placeRow(area, toggles, 28);
        std::vector<juce::Component*> row1 { &volume, &pan, &pitch, &fine, &offset, &fadeIn, &fadeOut, &drive };
        placeRow(area, row1, Theme::knobCellH);
        std::vector<juce::Component*> row2 { &width, &reverb, &delay, &cutStart, &cutEnd, &resonance, &swing, &velStart };
        placeRow(area, row2, Theme::knobCellH);
        auto velRow = area.removeFromTop(Theme::knobCellH);
        velEnd.setBounds(velRow.removeFromLeft(Theme::knobCellW).reduced(4, 2));
        auto sampleRow = velRow.reduced(8, 10);
        detectedRoot.setBounds(sampleRow.removeFromTop(22));
        matchKey.setBounds(sampleRow.removeFromRight(100).removeFromTop(28));
        sampleRoot.setBounds(sampleRow.removeFromLeft(150).removeFromTop(28));
        std::vector<juce::Component*> row4 { &shape, &filter, &fadeInCurve, &fadeOutCurve };
        placeRow(area, row4, Theme::controlH);
        area.removeFromTop(Theme::space1);
        std::vector<juce::Component*> row5 { &rollStart, &rollEnd, &rollAccel, &velCurve };
        placeRow(area, row5, Theme::controlH);
        area.removeFromTop(Theme::space1);
        sampleWave.setBounds(area.removeFromTop(std::min(100, area.getHeight())).reduced(4, 0));
    }

    int contentHeight() const { return 24 + Theme::controlH + 28 + Theme::knobCellH * 3 + Theme::controlH * 2 + Theme::space4; }

    void setPeaks(const WavePeaks& peaks)
    {
        sampleWave.setPeaks(peaks.minColumn, peaks.maxColumn, peaks.columns, peaks.dropFraction);
        sampleWave.tick(1.0f);
    }

    void refreshTrim()
    {
        auto* custom = audio.apvts.getRawParameterValue(layerParam(index, "useCustom"));
        const bool usingSample = custom != nullptr && custom->load() >= 0.5f;
        type.setEnabled(!usingSample);
        sampleRoot.setVisible(usingSample);
        matchKey.setVisible(usingSample);
        detectedRoot.setVisible(usingSample);
        if (usingSample)
        {
            GrokFxAudioProcessor::SampleReadGuard guard(audio);
            const auto model = audio.readModel();
            const auto* sample = model.layers[index].sample;
            const juce::String root = sample != nullptr && sample->detectedRoot >= 0
                ? juce::String(noteName(sample->detectedRoot)) : "Unknown / unpitched";
            detectedRoot.setText("Detected root: " + root + "  |  Pitch-preserving fit", juce::dontSendNotification);
            matchKey.setEnabled(model.layers[index].sampleRoot != 1);
        }
        auto* start = audio.apvts.getRawParameterValue(layerParam(index, "sampleStart"));
        auto* end = audio.apvts.getRawParameterValue(layerParam(index, "sampleEnd"));
        if (start != nullptr && end != nullptr)
            sampleWave.setTrim(start->load(), end->load());
    }

private:
    GrokFxAudioProcessor& audio;
    WaveformView sampleWave;
    juce::Label detectedRoot;
    GrokChoice sampleRoot { "Source root: Auto uses detection, a note overrides it, and Unpitched disables key matching." };
    GrokToggle matchKey { "Match key", "Transpose this sample from its source root to the song key without changing its length." };
    int index = 0;
    GrokChoice type { "Choose a stock sound for this layer; select a stock pad to exit custom mode." };
    GrokChoice bank { "Timbre bank for this layer." };
    GrokToggle on { "On", "Enable this layer." };
    GrokToggle solo { "Solo", "Listen to this layer alone." };
    GrokToggle mute { "Mute", "Silence this layer." };
    GrokToggle lock { "Lock", "Keep this layer still when you hit New take or Randomize." };
    GrokToggle reverse { "Rev", "Play this layer's musical section backwards. The drop stays at the end." };
    GrokKnob volume { "Volume", "Layer volume." };
    GrokKnob pan { "Pan", "Layer pan." };
    GrokKnob pitch { "Pitch", "Pitch in semitones." };
    GrokKnob fine { "Fine", "Fine pitch in cents." };
    GrokKnob offset { "Start", "Move the start of the layer later in the clip." };
    GrokKnob fadeIn { "Fade in", "Fade-in length." };
    GrokKnob fadeOut { "Fade out", "Fade-out length." };
    GrokKnob drive { "Drive", "Saturation amount." };
    GrokKnob width { "Width", "Layer stereo width." };
    GrokKnob reverb { "Reverb", "Send into the master reverb." };
    GrokKnob delay { "Delay", "Send into the tempo-synced delay." };
    GrokKnob cutStart { "Cut A", "Filter cutoff at the start of the clip." };
    GrokKnob cutEnd { "Cut B", "Filter cutoff at the drop." };
    GrokKnob resonance { "Reso", "Filter resonance across the sweep." };
    GrokKnob swing { "Swing", "Snare-roll swing. Every other hit lands late." };
    GrokKnob velStart { "Vel A", "Snare-roll velocity at the start." };
    GrokKnob velEnd { "Vel B", "Snare-roll velocity at the drop." };
    GrokChoice shape { "Riser, downlifter and sweep curve." };
    GrokChoice fadeInCurve { "Fade-in curve." };
    GrokChoice fadeOutCurve { "Fade-out curve." };
    GrokChoice filter { "Low-pass, high-pass, band-pass, or off." };
    GrokChoice rollStart { "Snare-roll subdivision at the start." };
    GrokChoice rollEnd { "Snare-roll subdivision at the drop." };
    GrokChoice rollAccel { "How the snare roll speeds up." };
    GrokChoice velCurve { "How snare-roll velocity builds." };
};

MixPanel::MixPanel(GrokFxAudioProcessor& processor)
    : audio(processor)
{
    for (int layer = 0; layer < layerCount; ++layer)
    {
        auto* card = cards.add(new Card(audio, layer));
        addChildComponent(card);
        auto* tab = layerTabs.add(new GrokButton("Layer " + juce::String(layer + 1),
                                                 "Select layer " + juce::String(layer + 1) + "."));
        addAndMakeVisible(tab);
        tab->onClick = [this, layer]() { selectedLayer = layer; audio.setArmedLayer(layer); refresh(); resized(); };
    }
    addAndMakeVisible(addLayer);
    addLayer.onClick = [this]()
    {
        for (int layer = 0; layer < layerCount; ++layer)
        {
            auto* parameter = audio.apvts.getParameter(layerParam(layer, "enabled"));
            if (parameter != nullptr && parameter->getValue() < 0.5f)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(1.0f);
                parameter->endChangeGesture();
                selectedLayer = layer;
                audio.setArmedLayer(layer);
                refresh();
                resized();
                return;
            }
        }
    };
    refresh();
}

MixPanel::~MixPanel() = default;

void MixPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::space1);
    auto tabs = area.removeFromTop(Theme::controlH);
    for (int layer = 0; layer < layerTabs.size(); ++layer)
    {
        if (layerTabs[layer]->isVisible())
            layerTabs[layer]->setBounds(tabs.removeFromLeft(92).reduced(2, 0));
    }
    addLayer.setBounds(tabs.removeFromLeft(40).reduced(2, 0));
    area.removeFromTop(Theme::space1);
    for (auto* card : cards)
        card->setBounds(area);
}

void MixPanel::refresh()
{
    bool layoutChanged = false;
    for (int layer = 0; layer < cards.size(); ++layer)
    {
        auto* enabled = audio.apvts.getRawParameterValue(layerParam(layer, "enabled"));
        const bool active = enabled != nullptr && enabled->load() >= 0.5f;
        const bool visible = active || layer == selectedLayer;
        layoutChanged = layoutChanged || layerTabs[layer]->isVisible() != visible;
        layerTabs[layer]->setVisible(visible);
        layerTabs[layer]->setToggleState(layer == selectedLayer, juce::dontSendNotification);
        cards[layer]->setVisible(layer == selectedLayer);
        if (layer == selectedLayer) cards[layer]->refreshTrim();
        cards[layer]->repaint();
    }
    bool full = true;
    for (int layer = 0; layer < layerCount; ++layer)
        if (auto* enabled = audio.apvts.getRawParameterValue(layerParam(layer, "enabled")))
            full = full && enabled->load() >= 0.5f;
    addLayer.setEnabled(!full);
    if (layoutChanged)
        resized();
}

void MixPanel::setLayerPeaks(const WavePeaks& peaks, int layer)
{
    if (juce::isPositiveAndBelow(layer, cards.size()))
        cards[layer]->setPeaks(peaks);
}

DropPanel::DropPanel(GrokFxAudioProcessor& processor, std::function<void()> dragFiles)
    : audio(processor)
{
    title.setFont(Theme::font(Theme::typeSection, true));
    title.setColour(juce::Label::textColourId, Theme::text());
    bpm.setFont(Theme::font(Theme::typeBody, true));
    bpm.setColour(juce::Label::textColourId, Theme::accent());
    bpm.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(title);
    addAndMakeVisible(bpm);
    addAndMakeVisible(wave);
    addAndMakeVisible(levels);
    wave.onExternalDrag = std::move(dragFiles);
    wave.setTooltip("Drag the handles to trim; click to audition, or drag the audio onto a track.");
    wave.onPreviewClick = [this](float fraction) { audio.togglePreviewAt(fraction); };
    wave.onTrimChange = [this](float start, float end)
    {
        for (const auto& update : { std::pair { "trimStart", start }, std::pair { "trimEnd", end } })
            if (auto* parameter = audio.apvts.getParameter(update.first))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(update.second);
                parameter->endChangeGesture();
            }
    };
    levels.setTooltip("Shows the left and right output levels.");

    auto bindKnob = [this](GrokKnob& knob, const char* id)
    {
        details.addAndMakeVisible(knob);
        knob.bind(audio.apvts, id);
    };
    auto bindChoice = [this](GrokChoice& choice, const char* id)
    {
        details.addAndMakeVisible(choice);
        choice.bind(audio.apvts, id);
    };
    auto bindToggle = [this](GrokToggle& toggle, const char* id)
    {
        details.addAndMakeVisible(toggle);
        toggle.bind(audio.apvts, id);
    };

    addAndMakeVisible(preview);
    addAndMakeVisible(newTake);
    preview.bind(audio.apvts, "preview");
    newTake.onClick = [this]() { audio.newTake(); };

    details.addAndMakeVisible(strength);
    bindKnob(strength, "randomStrength");
    bindChoice(bits, "exportBits");
    bindChoice(rate, "exportRate");
    bindToggle(stems, "exportStems");
    bindKnob(tail, "tailBeats");
    bindKnob(intensity, "intensity");
    bindKnob(space, "space");
    bindKnob(brightness, "brightness");
    bindKnob(tension, "tension");
    bindKnob(tone, "character");
    bindKnob(gain, "masterGain");
    bindKnob(masterWidthKnob, "masterWidth");
    bindKnob(ceiling, "limiterCeiling");
    bindToggle(limiter, "limiterOn");
    bindChoice(filter, "masterFilter");
    bindKnob(cutoff, "masterCutoff");
    bindKnob(resonance, "masterResonance");
    bindKnob(reverb, "reverbSize");
    bindKnob(delay, "delayAmount");
    bindChoice(delayTime, "delayDivision");
    bindChoice(delayRhythm, "delayStyle");

    addAndMakeVisible(details);
}

DropPanel::~DropPanel() = default;

void DropPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::space2);
    auto heading = area.removeFromTop(Theme::space3);
    title.setBounds(heading.removeFromLeft(80));
    bpm.setBounds(heading);
    area.removeFromTop(Theme::space1);
    auto waveRow = area.removeFromTop(120);
    levels.setBounds(waveRow.removeFromRight(24));
    waveRow.removeFromRight(Theme::space1);
    wave.setBounds(waveRow);
    area.removeFromTop(Theme::space1);
    auto buttons = area.removeFromTop(36);
    preview.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(2, 0));
    newTake.setBounds(buttons.reduced(2, 0));
    area.removeFromTop(Theme::space1);
    details.setBounds(area);

    int y = 0;
    const int width = details.getWidth();
    const int gap = 4;
    const int controlHeight = 28;
    const int knobHeight = std::max(40, (details.getHeight() - 4 * controlHeight - 7 * gap) / 4);
    auto nextRow = [&](int height)
    {
        juce::Rectangle<int> row(0, y, width, height);
        y += height + gap;
        return row;
    };
    {
        auto row = nextRow(knobHeight);
        strength.setBounds(row.removeFromLeft(width / 2).reduced(2, 0));
        tail.setBounds(row.reduced(2, 0));
    }
    {
        auto row = nextRow(controlHeight);
        bits.setBounds(row.removeFromLeft(width / 2).reduced(2, 0));
        rate.setBounds(row.reduced(2, 0));
    }
    stems.setBounds(nextRow(controlHeight).reduced(2, 0));
    {
        auto row = nextRow(knobHeight);
        const int cell = width / 4;
        intensity.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        space.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        brightness.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        tension.setBounds(row.reduced(1, 0));
    }
    {
        auto row = nextRow(knobHeight);
        const int cell = width / 4;
        tone.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        gain.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        masterWidthKnob.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        ceiling.setBounds(row.reduced(1, 0));
    }
    {
        auto row = nextRow(controlHeight);
        limiter.setBounds(row.removeFromLeft(width / 2).reduced(2, 0));
        filter.setBounds(row.reduced(2, 0));
    }
    {
        auto row = nextRow(knobHeight);
        const int cell = width / 4;
        cutoff.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        resonance.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        reverb.setBounds(row.removeFromLeft(cell).reduced(1, 0));
        delay.setBounds(row.reduced(1, 0));
    }
    {
        auto row = nextRow(controlHeight);
        delayTime.setBounds(row.removeFromLeft(width / 2).reduced(2, 0));
        delayRhythm.setBounds(row.reduced(2, 0));
    }
}

void DropPanel::refreshTransport(double bpmValue, bool)
{
    bpm.setText(juce::String(juce::roundToInt(bpmValue)) + " BPM", juce::dontSendNotification);
    const auto model = audio.readModel();
    wave.setTrim(model.trimStart, model.trimEnd);
}

} // namespace grokfx
