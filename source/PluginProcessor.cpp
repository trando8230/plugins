#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginIdentity.h"
#include "parameters/Parameters.h"
#include "dsp/WavExport.h"

#include <algorithm>

namespace
{

float loadRaw(const std::atomic<float>* value) noexcept
{
    return value != nullptr ? value->load(std::memory_order_relaxed) : 0.0f;
}

int loadInt(const std::atomic<float>* value, int fallback) noexcept
{
    return value != nullptr ? juce::roundToInt(value->load(std::memory_order_relaxed)) : fallback;
}

bool loadBool(const std::atomic<float>* value) noexcept
{
    return value != nullptr && value->load(std::memory_order_relaxed) >= 0.5f;
}

} // namespace

struct GrokFxAudioProcessor::Raw
{
    struct Layer
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* bank = nullptr;
        std::atomic<float>* volume = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* pitch = nullptr;
        std::atomic<float>* fine = nullptr;
        std::atomic<float>* offset = nullptr;
        std::atomic<float>* fadeIn = nullptr;
        std::atomic<float>* fadeInCurve = nullptr;
        std::atomic<float>* fadeOut = nullptr;
        std::atomic<float>* fadeOutCurve = nullptr;
        std::atomic<float>* reverse = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* filterType = nullptr;
        std::atomic<float>* cutoffStart = nullptr;
        std::atomic<float>* cutoffEnd = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* reverbSend = nullptr;
        std::atomic<float>* delaySend = nullptr;
        std::atomic<float>* locked = nullptr;
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* rollStart = nullptr;
        std::atomic<float>* rollEnd = nullptr;
        std::atomic<float>* rollAccel = nullptr;
        std::atomic<float>* swing = nullptr;
        std::atomic<float>* velStart = nullptr;
        std::atomic<float>* velEnd = nullptr;
        std::atomic<float>* velCurve = nullptr;
        std::atomic<float>* seed = nullptr;
    };

    Layer layers[grokfx::layerCount];
    std::atomic<float>* keyRoot = nullptr;
    std::atomic<float>* keyMode = nullptr;
    std::atomic<float>* length = nullptr;
    std::atomic<float>* character = nullptr;
    std::atomic<float>* intensity = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* brightness = nullptr;
    std::atomic<float>* tension = nullptr;
    std::atomic<float>* randomStrength = nullptr;
    std::atomic<float>* masterWidth = nullptr;
    std::atomic<float>* reverbAmount = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* delayAmount = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayDivision = nullptr;
    std::atomic<float>* masterFilter = nullptr;
    std::atomic<float>* masterCutoff = nullptr;
    std::atomic<float>* masterResonance = nullptr;
    std::atomic<float>* limiterOn = nullptr;
    std::atomic<float>* limiterCeiling = nullptr;
    std::atomic<float>* masterGain = nullptr;
    std::atomic<float>* tailBeats = nullptr;
    std::atomic<float>* followHost = nullptr;
    std::atomic<float>* preview = nullptr;
    std::atomic<float>* exportBits = nullptr;
    std::atomic<float>* exportRate = nullptr;
    std::atomic<float>* exportStems = nullptr;
};

GrokFxAudioProcessor::GrokFxAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "PARAMS", grokfx::createParameterLayout()),
      raw(std::make_unique<Raw>())
{
    auto bind = [this](const juce::String& id) -> std::atomic<float>*
    {
        auto* parameter = apvts.getRawParameterValue(id);
        jassert(parameter != nullptr);
        return parameter;
    };

    for (int layer = 0; layer < grokfx::layerCount; ++layer)
    {
        auto& slot = raw->layers[layer];
        slot.enabled = bind(grokfx::layerParam(layer, "enabled"));
        slot.type = bind(grokfx::layerParam(layer, "type"));
        slot.bank = bind(grokfx::layerParam(layer, "bank"));
        slot.volume = bind(grokfx::layerParam(layer, "volume"));
        slot.pan = bind(grokfx::layerParam(layer, "pan"));
        slot.pitch = bind(grokfx::layerParam(layer, "pitch"));
        slot.fine = bind(grokfx::layerParam(layer, "fine"));
        slot.offset = bind(grokfx::layerParam(layer, "offset"));
        slot.fadeIn = bind(grokfx::layerParam(layer, "fadeIn"));
        slot.fadeInCurve = bind(grokfx::layerParam(layer, "fadeInCurve"));
        slot.fadeOut = bind(grokfx::layerParam(layer, "fadeOut"));
        slot.fadeOutCurve = bind(grokfx::layerParam(layer, "fadeOutCurve"));
        slot.reverse = bind(grokfx::layerParam(layer, "reverse"));
        slot.solo = bind(grokfx::layerParam(layer, "solo"));
        slot.mute = bind(grokfx::layerParam(layer, "mute"));
        slot.filterType = bind(grokfx::layerParam(layer, "filterType"));
        slot.cutoffStart = bind(grokfx::layerParam(layer, "cutoffStart"));
        slot.cutoffEnd = bind(grokfx::layerParam(layer, "cutoffEnd"));
        slot.resonance = bind(grokfx::layerParam(layer, "resonance"));
        slot.drive = bind(grokfx::layerParam(layer, "drive"));
        slot.width = bind(grokfx::layerParam(layer, "width"));
        slot.reverbSend = bind(grokfx::layerParam(layer, "reverbSend"));
        slot.delaySend = bind(grokfx::layerParam(layer, "delaySend"));
        slot.locked = bind(grokfx::layerParam(layer, "locked"));
        slot.shape = bind(grokfx::layerParam(layer, "shape"));
        slot.rollStart = bind(grokfx::layerParam(layer, "rollStart"));
        slot.rollEnd = bind(grokfx::layerParam(layer, "rollEnd"));
        slot.rollAccel = bind(grokfx::layerParam(layer, "rollAccel"));
        slot.swing = bind(grokfx::layerParam(layer, "swing"));
        slot.velStart = bind(grokfx::layerParam(layer, "velStart"));
        slot.velEnd = bind(grokfx::layerParam(layer, "velEnd"));
        slot.velCurve = bind(grokfx::layerParam(layer, "velCurve"));
        slot.seed = bind(grokfx::layerParam(layer, "seed"));
    }

    raw->keyRoot = bind("keyRoot");
    raw->keyMode = bind("keyMode");
    raw->length = bind("length");
    raw->character = bind("character");
    raw->intensity = bind("intensity");
    raw->space = bind("space");
    raw->brightness = bind("brightness");
    raw->tension = bind("tension");
    raw->randomStrength = bind("randomStrength");
    raw->masterWidth = bind("masterWidth");
    raw->reverbAmount = bind("reverbAmount");
    raw->reverbSize = bind("reverbSize");
    raw->delayAmount = bind("delayAmount");
    raw->delayFeedback = bind("delayFeedback");
    raw->delayDivision = bind("delayDivision");
    raw->masterFilter = bind("masterFilter");
    raw->masterCutoff = bind("masterCutoff");
    raw->masterResonance = bind("masterResonance");
    raw->limiterOn = bind("limiterOn");
    raw->limiterCeiling = bind("limiterCeiling");
    raw->masterGain = bind("masterGain");
    raw->tailBeats = bind("tailBeats");
    raw->followHost = bind("followHost");
    raw->preview = bind("preview");
    raw->exportBits = bind("exportBits");
    raw->exportRate = bind("exportRate");
    raw->exportStems = bind("exportStems");
}

GrokFxAudioProcessor::~GrokFxAudioProcessor() = default;

void GrokFxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = grokfx::sanitizeSampleRate(sampleRate);
    engine.prepare(currentSampleRate);
    floatScratch.setSize(2, std::max(samplesPerBlock, 8192), false, true, true);
    enginePrepared = true;
    freeSample = 0;
    oneshotRemaining = 0;
}

void GrokFxAudioProcessor::releaseResources()
{
    engine.reset();
}

bool GrokFxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

grokfx::RenderModel GrokFxAudioProcessor::readModel() const noexcept
{
    grokfx::RenderModel model;
    for (int i = 0; i < grokfx::layerCount; ++i)
    {
        const auto& slot = raw->layers[i];
        auto& layer = model.layers[i];
        layer.enabled = loadBool(slot.enabled);
        layer.type = loadInt(slot.type, 0);
        layer.bank = loadInt(slot.bank, 0);
        layer.volumeDb = loadRaw(slot.volume);
        layer.pan = loadRaw(slot.pan);
        layer.pitchSemis = loadInt(slot.pitch, 0);
        layer.fineCents = loadRaw(slot.fine);
        layer.offset = loadRaw(slot.offset);
        layer.fadeIn = loadRaw(slot.fadeIn);
        layer.fadeInCurve = loadInt(slot.fadeInCurve, 0);
        layer.fadeOut = loadRaw(slot.fadeOut);
        layer.fadeOutCurve = loadInt(slot.fadeOutCurve, 0);
        layer.reverse = loadBool(slot.reverse);
        layer.solo = loadBool(slot.solo);
        layer.mute = loadBool(slot.mute);
        layer.filterType = loadInt(slot.filterType, 0);
        layer.cutoffStart = loadRaw(slot.cutoffStart);
        layer.cutoffEnd = loadRaw(slot.cutoffEnd);
        layer.resonance = loadRaw(slot.resonance);
        layer.drive = loadRaw(slot.drive);
        layer.width = loadRaw(slot.width);
        layer.reverbSend = loadRaw(slot.reverbSend);
        layer.delaySend = loadRaw(slot.delaySend);
        layer.locked = loadBool(slot.locked);
        layer.shape = loadInt(slot.shape, 0);
        layer.rollStart = loadInt(slot.rollStart, 0);
        layer.rollEnd = loadInt(slot.rollEnd, 5);
        layer.rollAccel = loadInt(slot.rollAccel, 0);
        layer.swing = loadRaw(slot.swing);
        layer.velStart = loadRaw(slot.velStart);
        layer.velEnd = loadRaw(slot.velEnd);
        layer.rollVelCurve = loadInt(slot.velCurve, 0);
        layer.seed = std::max(1, loadInt(slot.seed, 1));
    }

    model.keyRoot = loadInt(raw->keyRoot, 0);
    model.keyMode = loadInt(raw->keyMode, 0);
    model.length = loadInt(raw->length, static_cast<int>(grokfx::Length::fourBars));
    model.character = loadRaw(raw->character);
    model.intensity = loadRaw(raw->intensity);
    model.space = loadRaw(raw->space);
    model.brightness = loadRaw(raw->brightness);
    model.tension = loadRaw(raw->tension);
    model.randomStrength = loadRaw(raw->randomStrength);
    model.masterWidth = loadRaw(raw->masterWidth);
    model.reverbAmount = loadRaw(raw->reverbAmount);
    model.reverbSize = loadRaw(raw->reverbSize);
    model.delayAmount = loadRaw(raw->delayAmount);
    model.delayFeedback = loadRaw(raw->delayFeedback);
    model.delayDivision = loadInt(raw->delayDivision, 2);
    model.masterFilter = loadInt(raw->masterFilter, 0);
    model.masterCutoff = loadRaw(raw->masterCutoff);
    model.masterResonance = loadRaw(raw->masterResonance);
    model.limiterOn = loadBool(raw->limiterOn);
    model.limiterCeilingDb = loadRaw(raw->limiterCeiling);
    model.masterGainDb = loadRaw(raw->masterGain);
    model.tailBeats = loadRaw(raw->tailBeats);
    model.followHost = loadBool(raw->followHost);
    model.preview = loadBool(raw->preview);
    model.exportBits = loadInt(raw->exportBits, 1);
    model.exportRate = loadInt(raw->exportRate, 0);
    model.exportStems = loadBool(raw->exportStems);
    return model;
}

GrokFxAudioProcessor::Transport GrokFxAudioProcessor::readTransport() const noexcept
{
    Transport transport;
    auto* head = getPlayHead();
    if (head == nullptr)
        return transport;

    const auto position = head->getPosition();
    if (!position.hasValue())
        return transport;

    if (const auto bpm = position->getBpm())
        transport.bpm = grokfx::sanitizeBpm(*bpm);
    transport.playing = position->getIsPlaying();
    if (const auto ppq = position->getPpqPosition())
    {
        if (std::isfinite(*ppq))
        {
            transport.hasPpq = true;
            transport.ppq = *ppq;
        }
    }
    return transport;
}

void GrokFxAudioProcessor::renderFree(juce::AudioBuffer<float>& buffer, int offset, int count,
                                      const grokfx::RenderModel& model, double bpm, int64_t total) noexcept
{
    if (count <= 0 || total <= 0)
        return;
    int rendered = 0;
    while (rendered < count)
    {
        if (freeSample >= total)
            freeSample %= total;
        if (freeSample < 0)
            freeSample = 0;
        const int slice = static_cast<int>(std::min<int64_t>(count - rendered, total - freeSample));
        float* channels[2] = {
            buffer.getWritePointer(0) + offset + rendered,
            buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) + offset + rendered : nullptr
        };
        engine.process(channels, buffer.getNumChannels(), slice, freeSample, model, bpm);
        freeSample += slice;
        rendered += slice;
        playFraction.store(static_cast<float>(static_cast<double>(freeSample % total) / static_cast<double>(total)),
                           std::memory_order_relaxed);
    }
}

void GrokFxAudioProcessor::renderGrid(juce::AudioBuffer<float>& buffer, int offset, int count,
                                      const grokfx::RenderModel& model, double bpm, double ppq, int64_t total) noexcept
{
    if (count <= 0 || total <= 0)
        return;
    const double samplesPerBeat = currentSampleRate * 60.0 / grokfx::sanitizeBpm(bpm);
    const double cycleBeats = std::max(0.01, static_cast<double>(grokfx::lengthBeats(model.length)) + std::max(0.0f, model.tailBeats));
    double rel = std::fmod(ppq, cycleBeats);
    if (rel < 0.0)
        rel += cycleBeats;
    int64_t index = static_cast<int64_t>(std::llround(rel * samplesPerBeat));
    if (index < 0)
        index = 0;
    index %= total;

    int rendered = 0;
    while (rendered < count)
    {
        const int slice = static_cast<int>(std::min<int64_t>(count - rendered, total - index));
        float* channels[2] = {
            buffer.getWritePointer(0) + offset + rendered,
            buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) + offset + rendered : nullptr
        };
        engine.process(channels, buffer.getNumChannels(), slice, index, model, bpm);
        index = (index + slice) % total;
        rendered += slice;
        playFraction.store(static_cast<float>(static_cast<double>(index) / static_cast<double>(total)),
                           std::memory_order_relaxed);
    }
}

void GrokFxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    const Transport transport = readTransport();
    displayedBpm.store(transport.bpm, std::memory_order_relaxed);

    if (!enginePrepared || raw == nullptr)
    {
        meterL.store(0.0f, std::memory_order_relaxed);
        meterR.store(0.0f, std::memory_order_relaxed);
        previewRunning.store(false, std::memory_order_relaxed);
        return;
    }

    const auto model = readModel();
    const int64_t total = std::max<int64_t>(1, grokfx::renderedSamples(model, transport.bpm, currentSampleRate));
    const bool grid = model.followHost && transport.playing && transport.hasPpq;

    int noteAt[64];
    int noteCount = 0;
    for (const auto metadata : midi)
    {
        if (metadata.getMessage().isNoteOn(true) && noteCount < 64)
        {
            const int at = juce::jlimit(0, numSamples - 1, metadata.samplePosition);
            noteAt[noteCount++] = at;
        }
    }
    std::sort(noteAt, noteAt + noteCount);

    int cursor = 0;
    int noteIndex = 0;
    bool sounding = false;
    while (cursor < numSamples)
    {
        while (noteIndex < noteCount && noteAt[noteIndex] < cursor)
            ++noteIndex;
        if (noteIndex < noteCount && noteAt[noteIndex] == cursor)
        {
            freeSample = 0;
            oneshotRemaining = total;
            ++noteIndex;
        }

        int boundary = numSamples;
        if (noteIndex < noteCount)
            boundary = noteAt[noteIndex];
        const int count = boundary - cursor;
        if (count <= 0)
        {
            cursor = std::min(numSamples, cursor + 1);
            continue;
        }
        const bool freeRun = model.preview || oneshotRemaining > 0;
        sounding = grid || freeRun;

        if (count > 0 && sounding)
        {
            if (grid)
            {
                const double ppq = transport.ppq + (static_cast<double>(cursor) * transport.bpm) / (currentSampleRate * 60.0);
                renderGrid(buffer, cursor, count, model, transport.bpm, ppq, total);
            }
            else
            {
                const int64_t before = freeSample;
                renderFree(buffer, cursor, count, model, transport.bpm, total);
                if (!model.preview)
                {
                    const int64_t consumed = (freeSample - before + total) % total;
                    oneshotRemaining = std::max<int64_t>(0, oneshotRemaining - (consumed == 0 ? count : consumed));
                    if (oneshotRemaining == 0)
                        freeSample = 0;
                }
            }
        }
        cursor = boundary;
    }

    previewRunning.store(sounding, std::memory_order_relaxed);

    float peakL = 0.0f;
    float peakR = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        peakL = std::max(peakL, std::abs(buffer.getSample(0, sample)));
        if (buffer.getNumChannels() > 1)
            peakR = std::max(peakR, std::abs(buffer.getSample(1, sample)));
    }
    if (buffer.getNumChannels() < 2)
        peakR = peakL;
    const float previousL = meterL.load(std::memory_order_relaxed);
    const float previousR = meterR.load(std::memory_order_relaxed);
    meterL.store(std::max(peakL, previousL * 0.92f), std::memory_order_relaxed);
    meterR.store(std::max(peakR, previousR * 0.92f), std::memory_order_relaxed);
}

void GrokFxAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    if (floatScratch.getNumChannels() < 2 || floatScratch.getNumSamples() <= 0)
        return;

    int offset = 0;
    while (offset < buffer.getNumSamples())
    {
        const int count = std::min(floatScratch.getNumSamples(), buffer.getNumSamples() - offset);
        juce::MidiBuffer slice;
        for (const auto metadata : midi)
        {
            const int at = metadata.samplePosition - offset;
            if (at >= 0 && at < count)
                slice.addEvent(metadata.getMessage(), at);
        }

        float* channels[] = { floatScratch.getWritePointer(0), floatScratch.getWritePointer(1) };
        juce::AudioBuffer<float> view(channels, 2, count);
        for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
            for (int sample = 0; sample < count; ++sample)
                view.setSample(channel, sample, static_cast<float>(buffer.getSample(channel, offset + sample)));

        processBlock(view, slice);

        for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
            for (int sample = 0; sample < count; ++sample)
                buffer.setSample(channel, offset + sample, view.getSample(channel, sample));
        offset += count;
    }
}

const juce::String GrokFxAudioProcessor::getName() const
{
    return grokfx::productName;
}

double GrokFxAudioProcessor::getTailLengthSeconds() const
{
    const float beats = raw != nullptr ? loadRaw(raw->tailBeats) : 0.0f;
    return static_cast<double>(std::max(0.0f, beats)) * 60.0 / getDisplayedBpm();
}

float GrokFxAudioProcessor::getUiScale() const noexcept
{
    return std::clamp(static_cast<float>(uiScaleMilli.load(std::memory_order_relaxed)) / 10000.0f, 0.75f, 2.0f);
}

void GrokFxAudioProcessor::setUiScale(float scale) noexcept
{
    const float clamped = std::clamp(scale, 0.75f, 2.0f);
    uiScaleMilli.store(juce::roundToInt(clamped * 10000.0f), std::memory_order_relaxed);
}

void GrokFxAudioProcessor::setArmedLayer(int layer) noexcept
{
    armedLayer.store(juce::jlimit(0, grokfx::layerCount - 1, layer), std::memory_order_relaxed);
}

void GrokFxAudioProcessor::newTake()
{
    juce::Random random;
    random.setSeedRandomly();
    juce::ignoreUnused(apvts.copyState());
    const auto model = readModel();
    undoManager.beginNewTransaction("New take");
    for (int layer = 0; layer < grokfx::layerCount; ++layer)
    {
        if (model.layers[layer].locked)
            continue;
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(grokfx::layerParam(layer, "seed")));
        if (parameter == nullptr)
            continue;
        int next = 1 + random.nextInt(999982);
        if (next == model.layers[layer].seed)
            next = next % 999982 + 1;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(next)));
        parameter->endChangeGesture();
    }
    juce::ignoreUnused(apvts.copyState());
}

void GrokFxAudioProcessor::randomizeLayers(juce::Random& random)
{
    juce::ignoreUnused(apvts.copyState());
    const float strength = std::clamp(readModel().randomStrength, 0.0f, 1.0f);
    const auto before = readModel();
    undoManager.beginNewTransaction("Randomize");
    for (int layer = 0; layer < grokfx::layerCount; ++layer)
    {
        if (before.layers[layer].locked)
            continue;
        for (const char* suffix : grokfx::randomizableSuffixes)
        {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(grokfx::layerParam(layer, suffix)));
            if (parameter == nullptr)
                continue;
            const float mixed = grokfx::blendNormalized(parameter->getValue(), random.nextFloat(), strength);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, mixed));
            parameter->endChangeGesture();
        }

        if (strength > 0.0f)
        {
            auto* seed = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(grokfx::layerParam(layer, "seed")));
            if (seed != nullptr)
            {
                const int delta = 1 + static_cast<int>(std::floor(strength * static_cast<float>(1 + random.nextInt(5000))));
                const int next = 1 + (before.layers[layer].seed - 1 + delta) % 999982;
                seed->beginChangeGesture();
                seed->setValueNotifyingHost(seed->convertTo0to1(static_cast<float>(next)));
                seed->endChangeGesture();
            }
        }
    }
    juce::ignoreUnused(apvts.copyState());
}

void GrokFxAudioProcessor::toggleAB()
{
    const juce::ScopedLock lock(stateLock);
    auto current = apvts.copyState();
    if (!showingB.load())
    {
        slotA = current.toXmlString();
        if (slotB.isEmpty())
            slotB = slotA;
        auto restored = juce::ValueTree::fromXml(slotB);
        if (restored.isValid())
            apvts.replaceState(restored);
        showingB.store(true);
    }
    else
    {
        slotB = current.toXmlString();
        if (slotA.isEmpty())
            slotA = slotB;
        auto restored = juce::ValueTree::fromXml(slotA);
        if (restored.isValid())
            apvts.replaceState(restored);
        showingB.store(false);
    }
}

juce::String GrokFxAudioProcessor::makeExportFileName(double bpm, int stemLayer) const
{
    return grokfx::exportFileName(readModel(), bpm, stemLayer);
}

int64_t GrokFxAudioProcessor::renderMix(juce::AudioBuffer<float>& destination, double bpm, double sampleRate) const
{
    grokfx::Engine local;
    local.renderOffline(destination, readModel(), bpm, sampleRate);
    return destination.getNumSamples();
}

juce::Array<juce::File> GrokFxAudioProcessor::renderExportFiles(const juce::File& directory, double hostSampleRate) const
{
    juce::Array<juce::File> files;
    const auto model = readModel();
    const double bpm = getDisplayedBpm();
    const double rate = grokfx::sampleRateForChoice(model.exportRate, hostSampleRate);
    const auto format = grokfx::wavFormatForChoice(model.exportBits);
    directory.createDirectory();

    auto writeOne = [&](const grokfx::RenderModel& renderModel, int stemLayer)
    {
        grokfx::Engine local;
        juce::AudioBuffer<float> buffer;
        local.renderOffline(buffer, renderModel, bpm, rate);
        const auto file = directory.getChildFile(grokfx::exportFileName(model, bpm, stemLayer));
        if (grokfx::writeStereoWav(file, buffer, rate, format))
            files.add(file);
    };

    if (model.exportStems)
    {
        for (int layer = 0; layer < grokfx::layerCount; ++layer)
        {
            if (!grokfx::layerAudible(model, layer))
                continue;
            auto stem = model;
            for (int other = 0; other < grokfx::layerCount; ++other)
            {
                stem.layers[other].solo = false;
                stem.layers[other].mute = other != layer;
                stem.layers[other].enabled = other == layer ? model.layers[other].enabled : false;
            }
            stem.layers[layer].enabled = true;
            stem.layers[layer].mute = false;
            writeOne(stem, layer);
        }
    }
    else
    {
        writeOne(model, -1);
    }
    return files;
}

void GrokFxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(stateLock);
    auto state = apvts.copyState();
    state.setProperty("uiScaleMilli", uiScaleMilli.load(), nullptr);
    state.setProperty("armedLayer", armedLayer.load(), nullptr);
    state.setProperty("showingB", showingB.load() ? 1 : 0, nullptr);
    state.setProperty("slotA", slotA, nullptr);
    state.setProperty("slotB", slotB, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void GrokFxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::ScopedLock lock(stateLock);
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid() || tree.getType() != apvts.state.getType())
        return;

    uiScaleMilli.store(static_cast<int>(tree.getProperty("uiScaleMilli", 10000)), std::memory_order_relaxed);
    armedLayer.store(juce::jlimit(0, grokfx::layerCount - 1, static_cast<int>(tree.getProperty("armedLayer", 0))),
                     std::memory_order_relaxed);
    showingB.store(static_cast<int>(tree.getProperty("showingB", 0)) != 0, std::memory_order_relaxed);
    slotA = tree.getProperty("slotA").toString();
    slotB = tree.getProperty("slotB").toString();
    apvts.replaceState(tree);
    freeSample = 0;
    oneshotRemaining = 0;
}

juce::AudioProcessorEditor* GrokFxAudioProcessor::createEditor()
{
    return new GrokFxAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrokFxAudioProcessor();
}
