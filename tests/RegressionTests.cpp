#include "Support.h"
#include "PluginEditor.h"
#include "dsp/Engine.h"
#include "parameters/Parameters.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace
{

juce::File repoRoot()
{
    auto dir = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 8; ++i)
    {
        if (dir.getChildFile("CMakeLists.txt").existsAsFile()
            && dir.getChildFile("source").getChildFile("dsp").getChildFile("Model.h").existsAsFile())
            return dir;
        dir = dir.getParentDirectory();
    }
    return {};
}

int countBytes(const juce::MemoryBlock& block, const char* needle)
{
    const auto length = std::strlen(needle);
    int count = 0;
    const auto* data = static_cast<const char*>(block.getData());
    const auto size = block.getSize();
    if (length == 0 || size < length)
        return 0;
    for (size_t i = 0; i + length <= size; ++i)
        if (std::memcmp(data + i, needle, length) == 0)
            ++count;
    return count;
}

double goertzelPower(const float* samples, int count, double frequency, double sampleRate)
{
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double coeff = 2.0 * std::cos(omega);
    double s1 = 0.0;
    double s2 = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double s0 = static_cast<double>(samples[i]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

double windowedPower(const std::vector<float>& windowed, double frequency, double sampleRate)
{
    return goertzelPower(windowed.data(), static_cast<int>(windowed.size()), frequency, sampleRate);
}

std::vector<float> hannWindow(const float* samples, int count)
{
    std::vector<float> windowed(static_cast<size_t>(count));
    double mean = 0.0;
    for (int i = 0; i < count; ++i)
        mean += samples[i];
    mean /= static_cast<double>(count);
    for (int i = 0; i < count; ++i)
    {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / static_cast<double>(count - 1));
        windowed[static_cast<size_t>(i)] = static_cast<float>((samples[i] - mean) * hann);
    }
    return windowed;
}

int64_t expectRendered(int beats, double tailBeats, double bpm, double sampleRate)
{
    const auto musical = std::llround(static_cast<double>(beats) * 60.0 / bpm * sampleRate);
    const auto tail = tailBeats > 0.0
        ? std::llround(tailBeats * 60.0 / bpm * sampleRate)
        : 0;
    return musical + tail;
}

grokfx::RenderModel tonalModel(int type, int key)
{
    grokfx::RenderModel model;
    model.length = static_cast<int>(grokfx::Length::oneBar);
    model.keyRoot = key;
    model.tension = 0.0f;
    model.brightness = 0.0f;
    model.character = 0.5f;
    model.intensity = 0.5f;
    model.reverbAmount = 0.0f;
    model.delayAmount = 0.0f;
    model.limiterOn = false;
    model.tailBeats = 0.0f;
    auto& layer = model.layers[0];
    layer.enabled = true;
    layer.type = type;
    layer.bank = static_cast<int>(grokfx::Bank::classic);
    layer.seed = 48;
    layer.drive = 0.0f;
    layer.reverbSend = 0.0f;
    layer.delaySend = 0.0f;
    layer.filterType = static_cast<int>(grokfx::FilterType::off);
    layer.pitchSemis = 0;
    layer.fineCents = 0.0f;
    layer.volumeDb = -6.0f;
    return model;
}

grokfx::RenderModel padModel(int type)
{
    auto model = tonalModel(type, 0);
    model.length = static_cast<int>(grokfx::Length::oneBeat);
    model.layers[0].bank = static_cast<int>(grokfx::Bank::classic);
    model.layers[0].shape = static_cast<int>(grokfx::Curve::linear);
    model.layers[0].rollStart = static_cast<int>(grokfx::Division::quarter);
    model.layers[0].rollEnd = static_cast<int>(grokfx::Division::thirtySecond);
    model.layers[0].rollAccel = static_cast<int>(grokfx::Curve::exponential);
    model.layers[0].swing = 0.0f;
    model.layers[0].velStart = 0.35f;
    model.layers[0].velEnd = 1.0f;
    model.tension = 0.5f;
    return model;
}

void renderInBlocks(grokfx::Engine& engine, juce::AudioBuffer<float>& buffer,
                    const grokfx::RenderModel& model, double bpm, int block)
{
    engine.reset();
    buffer.clear();
    int rendered = 0;
    while (rendered < buffer.getNumSamples())
    {
        const int count = std::min(block, buffer.getNumSamples() - rendered);
        float* channels[2] = {
            buffer.getWritePointer(0) + rendered,
            buffer.getWritePointer(1) + rendered
        };
        engine.process(channels, 2, count, rendered, model, bpm);
        rendered += count;
    }
}

int firstAtLeast(const juce::AudioBuffer<float>& buffer, int count, float threshold)
{
    for (int i = 0; i < count; ++i)
        if (std::abs(buffer.getSample(0, i)) >= threshold)
            return i;
    return -1;
}

void silenceBus(GrokFxAudioProcessor& plugin)
{
    setReal(plugin, "reverbAmount", 0.0f);
    setReal(plugin, "delayAmount", 0.0f);
    setReal(plugin, "limiterOn", 0.0f);
    for (int layer = 0; layer < grokfx::layerCount; ++layer)
    {
        setReal(plugin, grokfx::layerParam(layer, "reverbSend"), 0.0f);
        setReal(plugin, grokfx::layerParam(layer, "delaySend"), 0.0f);
    }
}

} // namespace

TEST_CASE("Pitched pads stay within 1 cent in every key", "[pitch]")
{
    auto expectPeak = [](const float* samples, int count, double sampleRate, double target, double offFrequency)
    {
        const auto windowed = hannWindow(samples, count);
        const auto at = [&](double cents)
        {
            const double frequency = target * std::pow(2.0, cents / 1200.0);
            return windowedPower(windowed, frequency, sampleRate);
        };
        const double center = at(0.0);
        const double sharp = at(1.0);
        const double flat = at(-1.0);
        const double off = windowedPower(windowed, offFrequency, sampleRate);
        INFO("center=" << center << " sharp=" << sharp << " flat=" << flat << " off=" << off);
        REQUIRE(center > sharp);
        REQUIRE(center > flat);
        REQUIRE(center > off);
    };

    std::vector<float> sine(32768);
    for (int i = 0; i < 32768; ++i)
        sine[static_cast<size_t>(i)] = std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0);
    expectPeak(sine.data(), 32768, 48000.0, 440.0, 440.0 * 1.5);
    std::vector<float> saw(32768);
    for (int i = 0; i < 32768; ++i)
    {
        const double phase = std::fmod(261.625565 * static_cast<double>(i) / 44100.0, 1.0);
        saw[static_cast<size_t>(i)] = static_cast<float>(2.0 * phase - 1.0);
    }
    expectPeak(saw.data(), 32768, 44100.0, 261.625565, 261.625565 * 1.5);

    const int types[] = {
        static_cast<int>(grokfx::FxType::riser),
        static_cast<int>(grokfx::FxType::downlifter),
        static_cast<int>(grokfx::FxType::subDrop)
    };
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    constexpr int need = 32768;

    for (int type : types)
        for (int key = 0; key < 12; ++key)
            for (double rate : rates)
            {
                const auto model = tonalModel(type, key);
                grokfx::Engine engine;
                juce::AudioBuffer<float> buffer;
                engine.renderOffline(buffer, model, 120.0, rate);
                const int musical = buffer.getNumSamples();
                const int start = musical / 5;
                INFO("type=" << grokfx::fxTypeName(type) << " key=" << key << " rate=" << rate);
                REQUIRE(start + need < musical);
                const float midi = 60.0f + static_cast<float>(key);
                const bool drop = type == static_cast<int>(grokfx::FxType::subDrop);
                const float target = drop ? grokfx::noteToHz(midi - 12.0f) : grokfx::noteToHz(midi);
                const double off = drop ? static_cast<double>(target) * 3.0
                                         : static_cast<double>(target) * 1.5;
                expectPeak(buffer.getReadPointer(0) + start, need, rate, target, off);
            }
}

TEST_CASE("First hit of every pad lands within 1 ms", "[onset]")
{
    const int types[] = {
        static_cast<int>(grokfx::FxType::riser),
        static_cast<int>(grokfx::FxType::downlifter),
        static_cast<int>(grokfx::FxType::impact),
        static_cast<int>(grokfx::FxType::snareRoll),
        static_cast<int>(grokfx::FxType::noiseSweep),
        static_cast<int>(grokfx::FxType::reverseCymbal),
        static_cast<int>(grokfx::FxType::subDrop)
    };
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 512, 2048 };
    constexpr double bpm = 120.0;

    for (double rate : rates)
    {
        grokfx::Engine engine;
        engine.prepare(rate);
        const int tolerance = std::max(1, static_cast<int>(std::llround(rate * 0.001)));
        const int musical = static_cast<int>(grokfx::musicalSamples(
            static_cast<int>(grokfx::Length::oneBeat), bpm, rate));

        for (int type : types)
        {
            const auto model = padModel(type);
            for (int block : blocks)
            {
                juce::AudioBuffer<float> buffer(2, musical);
                renderInBlocks(engine, buffer, model, bpm, block);
                INFO("type=" << grokfx::fxTypeName(type) << " rate=" << rate << " block=" << block);

                if (type == static_cast<int>(grokfx::FxType::impact))
                {
                    const int region = std::max(tolerance * 4, static_cast<int>(std::llround(rate * 0.04)));
                    float peakLevel = 0.0f;
                    for (int i = musical - region; i < musical; ++i)
                        peakLevel = std::max(peakLevel, std::abs(buffer.getSample(0, i)));
                    int lastLoud = musical - region;
                    for (int i = musical - region; i < musical; ++i)
                        if (std::abs(buffer.getSample(0, i)) >= peakLevel * 0.9f)
                            lastLoud = i;
                    INFO("impact last=" << lastLoud << " level=" << peakLevel << " scheduled=" << (musical - 1));
                    REQUIRE(peakLevel > 0.01f);
                    REQUIRE(std::abs(lastLoud - (musical - 1)) <= tolerance);
                }
                else if (type == static_cast<int>(grokfx::FxType::snareRoll))
                {
                    const int onset = firstAtLeast(buffer, musical, 1.0e-6f);
                    INFO("snare onset=" << onset);
                    REQUIRE(onset >= 0);
                    REQUIRE(onset <= tolerance);

                    const int region = std::max(tolerance + 1, static_cast<int>(std::llround(rate * 0.02)));
                    int downbeatIndex = musical - region;
                    float downbeatLevel = 0.0f;
                    for (int i = musical - region; i < musical; ++i)
                    {
                        const float level = std::abs(buffer.getSample(0, i));
                        if (level > downbeatLevel)
                        {
                            downbeatLevel = level;
                            downbeatIndex = i;
                        }
                    }
                    INFO("snare downbeat=" << downbeatIndex << " level=" << downbeatLevel
                         << " scheduled=" << (musical - 1));
                    REQUIRE(downbeatLevel > 0.01f);
                    REQUIRE(std::abs(downbeatIndex - (musical - 1)) <= tolerance);
                }
                else
                {
                    const int onset = firstAtLeast(buffer, musical, 1.0e-8f);
                    INFO("onset=" << onset);
                    REQUIRE(onset >= 0);
                    REQUIRE(onset <= tolerance);
                }
            }
        }
    }
}

TEST_CASE("Rendered length is exact for every bar count tempo and rate", "[render]")
{
    const int beatsForLength[] = { 1, 2, 4, 8, 16, 32, 64 };
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const double tempos[] = { 90.0, 128.0, 174.0 };

    grokfx::RenderModel model;
    model.layers[0].enabled = true;
    model.layers[0].type = static_cast<int>(grokfx::FxType::riser);
    model.reverbAmount = 0.0f;
    model.delayAmount = 0.0f;
    model.limiterOn = false;
    model.layers[0].reverbSend = 0.0f;
    model.layers[0].delaySend = 0.0f;

    for (int length = 0; length < static_cast<int>(grokfx::Length::count); ++length)
    {
        REQUIRE(grokfx::lengthBeats(length) == beatsForLength[length]);
        model.length = length;
        for (double bpm : tempos)
            for (double rate : rates)
            {
                model.tailBeats = 0.0f;
                const auto expected = expectRendered(beatsForLength[length], 0.0, bpm, rate);
                grokfx::Engine engine;
                juce::AudioBuffer<float> buffer;
                const auto written = engine.renderOffline(buffer, model, bpm, rate);
                INFO("length=" << length << " bpm=" << bpm << " rate=" << rate);
                REQUIRE(written == expected);
                REQUIRE(buffer.getNumSamples() == static_cast<int>(expected));
                if (expected % 1024 != 0)
                    REQUIRE(buffer.getNumSamples() % 1024 != 0);

                if (beatsForLength[length] == 1)
                {
                    model.tailBeats = 1.5f;
                    const auto withTail = expectRendered(1, 1.5, bpm, rate);
                    juce::AudioBuffer<float> tailed;
                    const auto tailWritten = engine.renderOffline(tailed, model, bpm, rate);
                    REQUIRE(withTail == expectRendered(1, 0.0, bpm, rate) + std::llround(1.5 * 60.0 / bpm * rate));
                    REQUIRE(tailWritten == withTail);
                    REQUIRE(tailed.getNumSamples() == static_cast<int>(withTail));
                }
            }
    }

    model.layers[0].type = static_cast<int>(grokfx::FxType::impact);
    model.tailBeats = 0.0f;
    model.length = static_cast<int>(grokfx::Length::sixteenBars);
    for (double rate : { 44100.0, 96000.0 })
    {
        const auto expected = expectRendered(64, 0.0, 20.0, rate);
        REQUIRE(expected > 8000000);
        grokfx::Engine engine;
        juce::AudioBuffer<float> buffer;
        const auto written = engine.renderOffline(buffer, model, 20.0, rate);
        INFO("floor tempo rate=" << rate << " expected=" << expected << " written=" << written);
        REQUIRE(written == expected);
        REQUIRE(buffer.getNumSamples() == static_cast<int>(expected));
        float impact = 0.0f;
        for (int i = 0; i < 64; ++i)
            impact = std::max(impact, std::abs(buffer.getSample(0, static_cast<int>(expected) - 1 - i)));
        REQUIRE(impact > 0.01f);
    }
}

TEST_CASE("MIDI one-shot stays silent after the drop", "[midi]")
{
    auto plugin = makePlugin();
    constexpr double rate = 48000.0;
    constexpr int block = 2048;
    plugin->prepareToPlay(rate, block);
    setReal(*plugin, "preview", 0.0f);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "tailBeats", 0.0f);
    setReal(*plugin, "l1_type", static_cast<float>(grokfx::FxType::subDrop));
    setReal(*plugin, "l1_enabled", 1.0f);
    for (int layer = 1; layer < grokfx::layerCount; ++layer)
        setReal(*plugin, grokfx::layerParam(layer, "enabled"), 0.0f);
    silenceBus(*plugin);

    const int total = static_cast<int>(grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBeat), 120.0, rate));
    REQUIRE(total % block != 0);

    juce::AudioBuffer<float> blockBuffer(2, block);
    juce::AudioBuffer<float> rendered(2, total + block);
    rendered.clear();
    bool note = true;
    for (int renderedSamples = 0; renderedSamples < rendered.getNumSamples();)
    {
        juce::MidiBuffer midi;
        if (note)
        {
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
            note = false;
        }
        plugin->processBlock(blockBuffer, midi);
        const int count = std::min(block, rendered.getNumSamples() - renderedSamples);
        for (int channel = 0; channel < 2; ++channel)
            rendered.copyFrom(channel, renderedSamples, blockBuffer, channel, 0, count);
        renderedSamples += count;
    }

    float opening = 0.0f;
    for (int i = 0; i < 2000; ++i)
        opening = std::max(opening, std::abs(rendered.getSample(0, i)));
    float after = 0.0f;
    for (int i = total; i < rendered.getNumSamples(); ++i)
        after = std::max(after, std::abs(rendered.getSample(0, i)));
    REQUIRE(opening > 0.02f);
    REQUIRE(after == 0.0f);

    setReal(*plugin, "preview", 1.0f);
    plugin->prepareToPlay(rate, block);
    juce::AudioBuffer<float> looped(2, total + block);
    for (int renderedSamples = 0; renderedSamples < looped.getNumSamples();)
    {
        juce::MidiBuffer midi;
        plugin->processBlock(blockBuffer, midi);
        const int count = std::min(block, looped.getNumSamples() - renderedSamples);
        for (int channel = 0; channel < 2; ++channel)
            looped.copyFrom(channel, renderedSamples, blockBuffer, channel, 0, count);
        renderedSamples += count;
    }
    float wrapped = 0.0f;
    for (int i = total; i < looped.getNumSamples(); ++i)
        wrapped = std::max(wrapped, std::abs(looped.getSample(0, i)));
    REQUIRE(wrapped > 0.02f);
}

TEST_CASE("A/B recall does not nest slot XML", "[state]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "l1_volume", -4.0f);
    plugin->toggleAB();
    setReal(*plugin, "l1_volume", -20.0f);

    juce::MemoryBlock saved;
    plugin->getStateInformation(saved);
    plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    plugin->toggleAB();

    juce::MemoryBlock again;
    plugin->getStateInformation(again);
    REQUIRE(countBytes(again, "slotA") == 1);
    REQUIRE(countBytes(again, "slotB") == 1);
    REQUIRE(countBytes(again, "showingB") == 1);
    REQUIRE(countBytes(again, "uiScaleMilli") == 1);
    REQUIRE(getReal(*plugin, "l1_volume") == Catch::Approx(-4.0f).margin(0.05f));

    plugin->setStateInformation(again.getData(), static_cast<int>(again.getSize()));
    plugin->toggleAB();
    juce::MemoryBlock third;
    plugin->getStateInformation(third);
    REQUIRE(countBytes(third, "slotA") == 1);
    REQUIRE(third.getSize() <= again.getSize() + 64);
}

TEST_CASE("Preset recall resets the play cursor", "[state]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 256);
    setReal(*plugin, "preview", 1.0f);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBar));
    silenceBus(*plugin);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int i = 0; i < 80; ++i)
        plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getPlayFraction() > 0.05f);

    juce::MemoryBlock saved;
    plugin->getStateInformation(saved);
    for (int i = 0; i < 30; ++i)
        plugin->processBlock(buffer, midi);
    const float before = plugin->getPlayFraction();
    REQUIRE(before > 0.05f);

    plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(plugin->getPlayFraction() == 0.0f);

    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getPlayFraction() < 0.02f);
    REQUIRE(plugin->getPlayFraction() < before);

    std::atomic<bool> run { true };
    std::atomic<int> blocks { 0 };
    std::thread audio([&]
    {
        juce::AudioBuffer<float> local(2, 128);
        juce::MidiBuffer localMidi;
        while (run.load(std::memory_order_relaxed))
        {
            plugin->processBlock(local, localMidi);
            blocks.fetch_add(1, std::memory_order_relaxed);
        }
    });
    for (int i = 0; i < 40; ++i)
    {
        plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        std::this_thread::yield();
    }
    run.store(false, std::memory_order_relaxed);
    audio.join();
    REQUIRE(blocks.load() > 0);
    REQUIRE(plugin->nonFiniteCount() == 0);
    plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(plugin->getPlayFraction() == 0.0f);
}

TEST_CASE("Drag file is ready before the gesture", "[export]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 256);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "tailBeats", 0.0f);
    setReal(*plugin, "exportRate", 2.0f);
    setReal(*plugin, "exportStems", 0.0f);
    silenceBus(*plugin);

    auto editorOwner = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
    auto* editor = dynamic_cast<GrokFxAudioProcessorEditor*>(editorOwner.get());
    REQUIRE(editor != nullptr);

    const auto started = juce::Time::getMillisecondCounter();
    juce::Array<juce::File> files;
    while (juce::Time::getMillisecondCounter() - started < 8000)
    {
        files = editor->readyDragFiles();
        if (!files.isEmpty() && files[0].existsAsFile() && files[0].getSize() > 64)
            break;
        juce::Thread::sleep(20);
    }
    REQUIRE_FALSE(files.isEmpty());
    REQUIRE(files[0].existsAsFile());
    REQUIRE(files[0].getSize() > 64);

    const auto before = juce::Time::getMillisecondCounterHiRes();
    const auto again = editor->readyDragFiles();
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - before;
    REQUIRE(elapsedMs < 50.0);
    REQUIRE(again.size() == files.size());
    REQUIRE(again[0].getFullPathName() == files[0].getFullPathName());

    const auto firstSize = files[0].getSize();
    setReal(*plugin, "tailBeats", 2.0f);
    const auto changed = juce::Time::getMillisecondCounter();
    bool updated = false;
    while (juce::Time::getMillisecondCounter() - changed < 8000)
    {
        const auto next = editor->readyDragFiles();
        if (!next.isEmpty() && next[0].existsAsFile() && next[0].getSize() != firstSize)
        {
            updated = true;
            break;
        }
        juce::Thread::sleep(20);
    }
    REQUIRE(updated);
}

TEST_CASE("Failed stem write surfaces an error", "[export]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "exportStems", 1.0f);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "tailBeats", 0.0f);
    setReal(*plugin, "exportRate", 2.0f);
    setReal(*plugin, "l1_enabled", 1.0f);
    setReal(*plugin, "l1_type", static_cast<float>(grokfx::FxType::riser));
    setReal(*plugin, "l2_enabled", 1.0f);
    setReal(*plugin, "l2_type", static_cast<float>(grokfx::FxType::impact));
    for (int layer = 2; layer < grokfx::layerCount; ++layer)
        setReal(*plugin, grokfx::layerParam(layer, "enabled"), 0.0f);
    silenceBus(*plugin);

    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("GrokFXStemFail");
    directory.deleteRecursively();
    REQUIRE(directory.createDirectory());
    const auto blockedName = plugin->makeExportFileName(120.0, 1);
    const auto firstName = plugin->makeExportFileName(120.0, 0);
    REQUIRE(directory.getChildFile(blockedName).createDirectory());
    directory.getChildFile(blockedName).getChildFile("occupied").replaceWithText("x");

    const auto files = plugin->renderExportFiles(directory, 48000.0);
    REQUIRE(files.isEmpty());
    REQUIRE(plugin->lastExportError().isNotEmpty());
    REQUIRE(plugin->lastExportError().contains(blockedName));
    REQUIRE_FALSE(directory.getChildFile(firstName).existsAsFile());
    directory.deleteRecursively();
}

TEST_CASE("Closing the editor stops the peak render between chunks", "[render]")
{
    grokfx::RenderModel model;
    model.layers[0].enabled = true;
    model.layers[0].type = static_cast<int>(grokfx::FxType::riser);
    model.length = static_cast<int>(grokfx::Length::oneBar);
    model.tailBeats = 0.0f;
    model.reverbAmount = 0.0f;
    model.delayAmount = 0.0f;
    model.limiterOn = false;

    std::atomic<int> polls { 0 };
    auto stop = [](void* context) -> bool
    {
        return static_cast<std::atomic<int>*>(context)->fetch_add(1) >= 2;
    };

    grokfx::Engine engine;
    juce::AudioBuffer<float> buffer;
    const auto written = engine.renderOffline(buffer, model, 120.0, 48000.0, stop, &polls);
    const auto full = grokfx::renderedSamples(model, 120.0, 48000.0);
    REQUIRE(full > 4096);
    REQUIRE(polls.load() >= 2);
    REQUIRE(written < full);
    REQUIRE(written <= 4096);
}

TEST_CASE("Release package lists the AGPL and Inter OFL", "[license]")
{
    const auto root = repoRoot();
    REQUIRE(root.isDirectory());
    const auto license = root.getChildFile("LICENSE").loadFileAsString();
    REQUIRE(license.contains("GNU AFFERO GENERAL PUBLIC LICENSE"));
    REQUIRE(license.contains("Version 3, 19 November 2007"));
    REQUIRE(license.contains("0. Definitions."));
    REQUIRE(license.contains("13. Remote Network Interaction"));
    REQUIRE(license.contains("JUCE"));
    REQUIRE(license.contains("Inter"));
    REQUIRE(license.contains("SIL Open Font License"));

    const auto ofl = root.getChildFile("licenses").getChildFile("OFL-Inter.txt").loadFileAsString();
    REQUIRE(ofl.contains("SIL OPEN FONT LICENSE"));
    REQUIRE(ofl.contains("Version 1.1"));

    const auto workflow = root.getChildFile(".github").getChildFile("workflows").getChildFile("build.yml").loadFileAsString();
    REQUIRE(workflow.contains("cp LICENSE dist/LICENSE"));
    REQUIRE(workflow.contains("cp licenses/OFL-Inter.txt dist/OFL-Inter.txt"));
    REQUIRE(workflow.contains("lipo -archs"));
    REQUIRE(workflow.contains("arm64"));
    REQUIRE(workflow.contains("x86_64"));
}

TEST_CASE("Presets round-trip and reject invalid files", "[preset]")
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getNonexistentChildFile("grokfx-preset-test", ".grokfx", false);
    auto plugin = makePlugin();
    setReal(*plugin, "l1_pitch", 7.0f);
    setReal(*plugin, "delayStyle", 2.0f);
    REQUIRE(plugin->savePreset(file).wasOk());
    setReal(*plugin, "l1_pitch", 0.0f);
    setReal(*plugin, "delayStyle", 0.0f);
    REQUIRE(plugin->loadPreset(file).wasOk());
    REQUIRE(getReal(*plugin, "l1_pitch") == Catch::Approx(7.0f));
    REQUIRE(getReal(*plugin, "delayStyle") == Catch::Approx(2.0f));
    REQUIRE(file.replaceWithText("invalid"));
    REQUIRE(plugin->loadPreset(file).failed());
    REQUIRE(getReal(*plugin, "l1_pitch") == Catch::Approx(7.0f));
    REQUIRE(file.deleteFile());
}

TEST_CASE("Old sessions reset newly added controls to their defaults", "[state][compatibility]")
{
    struct StateCodec : GrokFxAudioProcessor
    {
        static void encode(const juce::XmlElement& xml, juce::MemoryBlock& out) { copyXmlToBinary(xml, out); }
    };
    auto plugin = makePlugin();
    setReal(*plugin, "l1_pitch", 5.0f);
    auto oldState = plugin->apvts.copyState();
    for (int i = oldState.getNumChildren(); --i >= 0;)
    {
        const auto id = oldState.getChild(i).getProperty("id").toString();
        if (id == "delayStyle" || id == "trimStart" || id == "trimEnd"
            || id.endsWith("_useCustom") || id.endsWith("_sampleStart") || id.endsWith("_sampleEnd")
            || id.endsWith("_sampleRoot") || id.endsWith("_matchKey"))
            oldState.removeChild(i, nullptr);
    }
    auto xml = oldState.createXml();
    REQUIRE(xml != nullptr);
    juce::MemoryBlock encoded;
    StateCodec::encode(*xml, encoded);
    setReal(*plugin, "l1_pitch", 0.0f);
    setReal(*plugin, "l1_useCustom", 1.0f);
    setReal(*plugin, "l1_sampleStart", 0.8f);
    setReal(*plugin, "l1_sampleEnd", 0.8f);
    setReal(*plugin, "l1_sampleRoot", 5.0f);
    setReal(*plugin, "l1_matchKey", 1.0f);
    setReal(*plugin, "trimEnd", 0.3f);
    setReal(*plugin, "delayStyle", 2.0f);
    plugin->setStateInformation(encoded.getData(), static_cast<int>(encoded.getSize()));
    REQUIRE(getReal(*plugin, "l1_pitch") == Catch::Approx(5.0f));
    REQUIRE(getReal(*plugin, "l1_useCustom") == Catch::Approx(0.0f));
    REQUIRE(getReal(*plugin, "l1_sampleStart") == Catch::Approx(0.0f));
    REQUIRE(getReal(*plugin, "l1_sampleEnd") == Catch::Approx(1.0f));
    REQUIRE(getReal(*plugin, "l1_sampleRoot") == Catch::Approx(0.0f));
    REQUIRE(getReal(*plugin, "l1_matchKey") == Catch::Approx(0.0f));
    REQUIRE(getReal(*plugin, "trimEnd") == Catch::Approx(1.0f));
    REQUIRE(getReal(*plugin, "delayStyle") == Catch::Approx(0.0f));
}
