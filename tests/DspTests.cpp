#include "Support.h"
#include "dsp/Model.h"
#include "dsp/WavExport.h"
#include "parameters/Parameters.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <cstring>
#include <limits>

TEST_CASE("Tempo falls back to 120 BPM when the host has no tempo or playhead", "[tempo]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;

    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    HostPlayHead head;
    plugin->setPlayHead(&head);
    head.report = false;
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    head.report = true;
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    head.info.setIsPlaying(true);
    head.info.setBpm(0.0);
    head.info.setPpqPosition(0.0);
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    head.info.setBpm(-12.0);
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    head.info.setBpm(std::numeric_limits<double>::quiet_NaN());
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 120.0);

    head.info.setBpm(130.0);
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getDisplayedBpm() == 130.0);

    plugin->setPlayHead(nullptr);
    REQUIRE(grokfx::sanitizeBpm(0.0) == 120.0);
    REQUIRE(grokfx::sanitizeBpm(-4.0) == 120.0);
    REQUIRE(grokfx::sanitizeBpm(std::numeric_limits<double>::infinity()) == 120.0);
}

TEST_CASE("Rendered length matches every musical option at 120 and 130 BPM", "[render]")
{
    const int beats[] = { 1, 2, 4, 8, 16, 32, 64 };
    auto plugin = makePlugin();
    setReal(*plugin, "tailBeats", 0.0f);

    for (int length = 0; length < static_cast<int>(grokfx::Length::count); ++length)
    {
        setReal(*plugin, "length", static_cast<float>(length));
        for (double bpm : { 120.0, 130.0 })
        {
            for (double rate : { 48000.0, 44100.0 })
            {
                const auto expected = static_cast<int>(std::llround(
                    static_cast<double>(beats[length]) * rate * 60.0 / bpm));
                REQUIRE(grokfx::musicalSamples(length, bpm, rate) == expected);

                if (rate == 48000.0)
                {
                    juce::AudioBuffer<float> rendered;
                    const auto frames = plugin->renderMix(rendered, bpm, rate);
                    REQUIRE(frames == expected);
                    REQUIRE(rendered.getNumSamples() == expected);
                    REQUIRE(rendered.getNumChannels() == 2);
                    expectFiniteBuffer(rendered);
                }
            }
        }
    }

    REQUIRE(grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBeat), 120.0, 48000.0) == 24000);
    REQUIRE(grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBar), 120.0, 44100.0) == 88200);
    REQUIRE(grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBeat), 130.0, 48000.0) == 22154);
}

TEST_CASE("Tail extends the rendered clip past the drop", "[render]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBar));
    setReal(*plugin, "tailBeats", 2.0f);
    juce::AudioBuffer<float> rendered;
    plugin->renderMix(rendered, 120.0, 48000.0);
    const auto musical = grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBar), 120.0, 48000.0);
    const auto tail = grokfx::tailSamples(2.0f, 120.0, 48000.0);
    REQUIRE(musical == 96000);
    REQUIRE(tail == 48000);
    REQUIRE(rendered.getNumSamples() == musical + tail);
}

TEST_CASE("Impact and riser land on the drop", "[render]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBar));
    setReal(*plugin, "tailBeats", 1.0f);
    setReal(*plugin, "l1_type", 0.0f);
    setReal(*plugin, "l1_enabled", 1.0f);
    for (int layer = 1; layer < 4; ++layer)
        setReal(*plugin, grokfx::layerParam(layer, "enabled"), 0.0f);

    juce::AudioBuffer<float> riser;
    plugin->renderMix(riser, 120.0, 48000.0);
    const int musical = static_cast<int>(grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBar), 120.0, 48000.0));
    float early = 0.0f;
    float late = 0.0f;
    const int window = 2400;
    for (int sample = 0; sample < window; ++sample)
        early = std::max(early, std::abs(riser.getSample(0, sample)));
    for (int sample = musical - window; sample < musical; ++sample)
        late = std::max(late, std::abs(riser.getSample(0, sample)));
    REQUIRE(late > early);

    setReal(*plugin, "l1_type", static_cast<float>(grokfx::FxType::impact));
    juce::AudioBuffer<float> impact;
    plugin->renderMix(impact, 120.0, 48000.0);
    int peakIndex = 0;
    float peak = 0.0f;
    for (int sample = 0; sample < musical; ++sample)
    {
        const float value = std::abs(impact.getSample(0, sample));
        if (value >= peak)
        {
            peak = value;
            peakIndex = sample;
        }
    }
    REQUIRE(peak > 0.01f);
    REQUIRE(peakIndex >= musical - static_cast<int>(0.05 * 48000.0));
}

TEST_CASE("Stopped or empty mix is silent", "[dsp]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 128);
    setReal(*plugin, "preview", 0.0f);
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    plugin->processBlock(buffer, midi);
    REQUIRE(maxAbs(buffer) == 0.0f);

    setReal(*plugin, "preview", 1.0f);
    for (int layer = 0; layer < 4; ++layer)
        setReal(*plugin, grokfx::layerParam(layer, "enabled"), 0.0f);
    plugin->processBlock(buffer, midi);
    REQUIRE(maxAbs(buffer) == 0.0f);

    setReal(*plugin, "l1_enabled", 1.0f);
    setReal(*plugin, "l1_mute", 1.0f);
    plugin->processBlock(buffer, midi);
    REQUIRE(maxAbs(buffer) == 0.0f);
}

TEST_CASE("Preview of the default riser is audible, finite and bounded", "[dsp]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 256);
    setReal(*plugin, "preview", 1.0f);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int block = 0; block < 40; ++block)
    {
        plugin->processBlock(buffer, midi);
        expectFiniteBuffer(buffer);
        peak = std::max(peak, maxAbs(buffer));
    }
    REQUIRE(peak > 0.01f);
    REQUIRE(plugin->nonFiniteCount() == 0);
}

TEST_CASE("No NaNs or Infs at parameter extremes", "[dsp]")
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 2048 };

    auto exercise = [](GrokFxAudioProcessor& plugin, double rate, int block, int repeats)
    {
        plugin.prepareToPlay(rate, block);
        plugin.resetNonFiniteCount();
        juce::AudioBuffer<float> buffer(2, block);
        juce::MidiBuffer midi;
        setReal(plugin, "preview", 1.0f);
        for (int i = 0; i < repeats; ++i)
        {
            plugin.processBlock(buffer, midi);
            expectFiniteBuffer(buffer);
        }
        REQUIRE(plugin.nonFiniteCount() == 0);
    };

    for (double rate : rates)
        for (int block : blocks)
        {
            auto plugin = makePlugin();
            exercise(*plugin, rate, block, block == 1 ? 32 : 2);

            for (auto* parameter : plugin->getParameters())
                parameter->setValue(0.0f);
            exercise(*plugin, rate, block, block == 1 ? 16 : 1);

            for (auto* parameter : plugin->getParameters())
                parameter->setValue(1.0f);
            exercise(*plugin, rate, block, block == 1 ? 16 : 1);
        }
}

TEST_CASE("Each parameter extreme stays finite and bounded", "[dsp]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 64);
    setReal(*plugin, "preview", 1.0f);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;

    for (auto* parameter : plugin->getParameters())
    {
        const float original = parameter->getValue();
        for (float extreme : { 0.0f, 1.0f })
        {
            parameter->setValueNotifyingHost(extreme);
            plugin->resetNonFiniteCount();
            plugin->processBlock(buffer, midi);
            expectFiniteBuffer(buffer);
            REQUIRE(plugin->nonFiniteCount() == 0);
        }
        parameter->setValueNotifyingHost(original);
        REQUIRE(parameter->getName(128).isNotEmpty());
        const bool ranged = dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr
                         || dynamic_cast<juce::AudioParameterInt*>(parameter) != nullptr;
        if (ranged)
            REQUIRE(parameter->getLabel().isNotEmpty());
    }
}

TEST_CASE("State round-trips through save and reload", "[state]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "l1_volume", -12.0f);
    setReal(*plugin, "keyRoot", 9.0f);
    setReal(*plugin, "keyMode", 1.0f);
    setReal(*plugin, "l2_seed", 4242.0f);
    setReal(*plugin, "intensity", 0.25f);
    setReal(*plugin, "tailBeats", 1.5f);
    plugin->setUiScale(1.25f);
    plugin->setArmedLayer(2);

    juce::MemoryBlock saved;
    plugin->getStateInformation(saved);
    plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    juce::MemoryBlock again;
    plugin->getStateInformation(again);
    REQUIRE(saved.getSize() == again.getSize());
    REQUIRE(std::memcmp(saved.getData(), again.getData(), saved.getSize()) == 0);

    auto restored = makePlugin();
    restored->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(getReal(*restored, "l1_volume") == Catch::Approx(-12.0f).margin(0.02f));
    REQUIRE(getReal(*restored, "keyRoot") == Catch::Approx(9.0f).margin(0.01f));
    REQUIRE(getReal(*restored, "keyMode") == Catch::Approx(1.0f).margin(0.01f));
    REQUIRE(getReal(*restored, "l2_seed") == Catch::Approx(4242.0f).margin(0.01f));
    REQUIRE(getReal(*restored, "intensity") == Catch::Approx(0.25f).margin(0.01f));
    REQUIRE(getReal(*restored, "tailBeats") == Catch::Approx(1.5f).margin(0.01f));
    REQUIRE(restored->getUiScale() == Catch::Approx(1.25f).margin(0.001f));
    REQUIRE(restored->getArmedLayer() == 2);

    juce::MemoryBlock fromNew;
    restored->getStateInformation(fromNew);
    REQUIRE(fromNew.getSize() == saved.getSize());
    REQUIRE(std::memcmp(fromNew.getData(), saved.getData(), saved.getSize()) == 0);
}

TEST_CASE("Undo restores the previous parameter value", "[state]")
{
    auto plugin = makePlugin();
    auto* parameter = plugin->apvts.getParameter("l1_volume");
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(0.2f);
    juce::ignoreUnused(plugin->apvts.copyState());
    plugin->undoManager.beginNewTransaction();
    parameter->setValueNotifyingHost(0.85f);
    juce::ignoreUnused(plugin->apvts.copyState());
    REQUIRE(plugin->undoManager.canUndo());
    plugin->undoManager.undo();
    REQUIRE(parameter->getValue() == Catch::Approx(0.2f).margin(0.02f));
}

TEST_CASE("A/B compare restores the other slot", "[state]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "l1_volume", -3.0f);
    plugin->toggleAB();
    REQUIRE(plugin->isShowingB());
    setReal(*plugin, "l1_volume", -18.0f);
    plugin->toggleAB();
    REQUIRE_FALSE(plugin->isShowingB());
    REQUIRE(getReal(*plugin, "l1_volume") == Catch::Approx(-3.0f).margin(0.05f));
    plugin->toggleAB();
    REQUIRE(getReal(*plugin, "l1_volume") == Catch::Approx(-18.0f).margin(0.05f));
}

TEST_CASE("Locked layers keep their seed on new take", "[random]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "l1_locked", 1.0f);
    setReal(*plugin, "l1_seed", 111.0f);
    setReal(*plugin, "l2_locked", 0.0f);
    setReal(*plugin, "l2_seed", 222.0f);
    const float lockedBefore = getReal(*plugin, "l1_seed");
    const float openBefore = getReal(*plugin, "l2_seed");
    plugin->newTake();
    REQUIRE(getReal(*plugin, "l1_seed") == Catch::Approx(lockedBefore).margin(0.01f));
    REQUIRE(getReal(*plugin, "l2_seed") != Catch::Approx(openBefore).margin(0.01f));
}

TEST_CASE("Randomize strength of zero leaves unlocked controls alone", "[random]")
{
    REQUIRE(grokfx::blendNormalized(0.2f, 0.8f, 0.0f) == Catch::Approx(0.2f));
    REQUIRE(grokfx::blendNormalized(0.2f, 0.8f, 1.0f) == Catch::Approx(0.8f));
    REQUIRE(grokfx::blendNormalized(0.2f, 0.8f, 0.5f) == Catch::Approx(0.5f));

    auto plugin = makePlugin();
    setReal(*plugin, "randomStrength", 0.0f);
    setReal(*plugin, "l1_volume", -9.0f);
    setReal(*plugin, "l1_locked", 0.0f);
    juce::Random random(1);
    plugin->randomizeLayers(random);
    REQUIRE(getReal(*plugin, "l1_volume") == Catch::Approx(-9.0f).margin(0.05f));
}

TEST_CASE("Export name and WAV frames follow the musical length", "[export]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "tailBeats", 0.0f);
    setReal(*plugin, "keyRoot", 0.0f);
    setReal(*plugin, "keyMode", 0.0f);
    for (int layer = 1; layer < 4; ++layer)
        setReal(*plugin, grokfx::layerParam(layer, "enabled"), 0.0f);
    setReal(*plugin, "l1_enabled", 1.0f);
    setReal(*plugin, "l1_type", 0.0f);
    REQUIRE(plugin->makeExportFileName(120.0, -1) == "GrokFX_Riser_Cmaj_120bpm_1beat.wav");

    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("GrokFXTests");
    directory.deleteRecursively();
    setReal(*plugin, "exportBits", 1.0f);
    setReal(*plugin, "exportRate", 2.0f);
    const auto files = plugin->renderExportFiles(directory, 44100.0);
    REQUIRE(files.size() == 1);
    REQUIRE(files[0].existsAsFile());
    REQUIRE(files[0].getFileName() == "GrokFX_Riser_Cmaj_120bpm_1beat.wav");

    juce::FileInputStream stream(files[0]);
    char riff[4] {};
    stream.read(riff, 4);
    REQUIRE(std::string(riff, 4) == "RIFF");
    stream.setPosition(20);
    const auto format = static_cast<unsigned>(stream.readShort());
    const auto channels = static_cast<unsigned>(stream.readShort());
    const auto rate = static_cast<unsigned>(stream.readInt());
    REQUIRE(format == 1);
    REQUIRE(channels == 2);
    REQUIRE(rate == 48000);
    stream.setPosition(34);
    const auto bits = static_cast<unsigned>(stream.readShort());
    REQUIRE(bits == 24);

    const auto frames = grokfx::musicalSamples(static_cast<int>(grokfx::Length::oneBeat), 120.0, 48000.0);
    REQUIRE(files[0].getSize() > static_cast<juce::int64>(frames * 6));
}

TEST_CASE("Shape curves stay inside 0 to 1", "[dsp]")
{
    for (int curve = 0; curve < 4; ++curve)
    {
        REQUIRE(grokfx::curveAt(0.0f, curve) == Catch::Approx(0.0f).margin(1.0e-5f));
        REQUIRE(grokfx::curveAt(1.0f, curve) == Catch::Approx(1.0f).margin(1.0e-4f));
        float previous = -1.0f;
        for (int step = 0; step <= 10; ++step)
        {
            const float value = grokfx::curveAt(static_cast<float>(step) / 10.0f, curve);
            REQUIRE(value >= -0.0001f);
            REQUIRE(value <= 1.0001f);
            REQUIRE(value + 0.0001f >= previous);
            previous = value;
        }
    }
}

TEST_CASE("Required parameters exist with host-facing names", "[parameters]")
{
    auto plugin = makePlugin();
    const char* globals[] = {
        "keyRoot", "keyMode", "length", "character", "intensity", "space", "brightness", "tension",
        "randomStrength", "masterWidth", "reverbAmount", "reverbSize", "delayAmount", "delayFeedback",
        "delayDivision", "masterFilter", "masterCutoff", "masterResonance", "limiterOn", "limiterCeiling",
        "masterGain", "tailBeats", "followHost", "preview", "exportBits", "exportRate", "exportStems"
    };
    for (const char* id : globals)
    {
        auto* parameter = plugin->apvts.getParameter(id);
        REQUIRE(parameter != nullptr);
        REQUIRE(parameter->getName(64).isNotEmpty());
    }

    const char* suffixes[] = {
        "enabled", "type", "bank", "volume", "pan", "pitch", "fine", "offset", "fadeIn", "fadeInCurve",
        "fadeOut", "fadeOutCurve", "reverse", "solo", "mute", "filterType", "cutoffStart", "cutoffEnd",
        "resonance", "drive", "width", "reverbSend", "delaySend", "locked", "shape", "rollStart",
        "rollEnd", "rollAccel", "swing", "velStart", "velEnd", "velCurve", "seed"
    };
    for (int layer = 0; layer < 4; ++layer)
        for (const char* suffix : suffixes)
        {
            const auto id = grokfx::layerParam(layer, suffix);
            auto* parameter = plugin->apvts.getParameter(id);
            REQUIRE(parameter != nullptr);
            REQUIRE(parameter->getName(80).startsWith("Layer " + juce::String(layer + 1)));
        }
}

TEST_CASE("Downlifter fades while reverse makes it rise", "[downlifter]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "l1_type", static_cast<float>(grokfx::FxType::downlifter));
    setReal(*plugin, "l1_reverbSend", 0.0f);
    setReal(*plugin, "l1_delaySend", 0.0f);
    setReal(*plugin, "l1_shape", 0.0f);
    for (int reverse = 0; reverse < 2; ++reverse)
    {
        setReal(*plugin, "l1_reverse", static_cast<float>(reverse));
        juce::AudioBuffer<float> rendered;
        plugin->renderMix(rendered, 120.0, 48000.0);
        const int window = rendered.getNumSamples() / 5;
        const auto first = rendered.getRMSLevel(0, window / 2, window);
        const auto last = rendered.getRMSLevel(0, rendered.getNumSamples() - window * 3 / 2, window);
        if (reverse == 0) REQUIRE(first > last * 1.5f);
        else REQUIRE(last > first * 1.5f);
    }
}

TEST_CASE("Delay note lengths support dotted and triplet rhythms", "[delay]")
{
    using Catch::Approx;
    REQUIRE(grokfx::delayBeats(1, 0) == Approx(1.0));
    REQUIRE(grokfx::delayBeats(2, 1) == Approx(0.75));
    REQUIRE(grokfx::delayBeats(4, 2) == Approx(1.0 / 6.0));
    REQUIRE(grokfx::delayBeats(3, 0) == Approx(0.75));
    auto plugin = makePlugin();
    setReal(*plugin, "delayStyle", 2.0f);
    REQUIRE(plugin->readModel().delayStyle == 2);
}

TEST_CASE("Waveform preview starts at the clicked position and stops", "[preview]")
{
    auto plugin = makePlugin();
    plugin->prepareToPlay(48000.0, 64);
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "followHost", 1.0f);
    plugin->togglePreviewAt(0.5f);
    REQUIRE(getReal(*plugin, "followHost") == Catch::Approx(0.0f));
    REQUIRE(getReal(*plugin, "preview") == Catch::Approx(1.0f));
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    plugin->processBlock(buffer, midi);
    REQUIRE(plugin->getPlayFraction() > 0.49f);
    REQUIRE(plugin->getPlayFraction() < 0.52f);
    plugin->togglePreviewAt(0.8f);
    REQUIRE(getReal(*plugin, "preview") == Catch::Approx(0.0f));
}

TEST_CASE("Space at zero removes reverb without changing the dry sound", "[reverb]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "l1_delaySend", 0.0f);
    setReal(*plugin, "space", 0.0f);
    juce::AudioBuffer<float> noSend;
    juce::AudioBuffer<float> sent;
    setReal(*plugin, "l1_reverbSend", 0.0f);
    plugin->renderMix(noSend, 120.0, 48000.0);
    setReal(*plugin, "l1_reverbSend", 1.0f);
    plugin->renderMix(sent, 120.0, 48000.0);
    REQUIRE(sent.getNumSamples() == noSend.getNumSamples());
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < sent.getNumSamples(); ++sample)
            REQUIRE(sent.getSample(channel, sample) == Catch::Approx(noSend.getSample(channel, sample)).margin(0.000001f));
}

TEST_CASE("Exported waveform uses the selected trim region", "[export][trim]")
{
    auto plugin = makePlugin();
    setReal(*plugin, "length", static_cast<float>(grokfx::Length::oneBeat));
    setReal(*plugin, "trimStart", 0.25f);
    setReal(*plugin, "trimEnd", 0.75f);
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("grokfx-trim-test", {}, false);
    const auto files = plugin->renderExportFiles(directory, 48000.0);
    REQUIRE(files.size() == 1);
    juce::WavAudioFormat format;
    auto stream = std::make_unique<juce::FileInputStream>(files[0]);
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    REQUIRE(reader != nullptr);
    REQUIRE(reader->lengthInSamples == 12000);
    juce::AudioBuffer<float> trimmed(2, 12000);
    REQUIRE(reader->read(&trimmed, 0, 12000, 0, true, true));
    juce::AudioBuffer<float> full;
    plugin->renderMix(full, 120.0, 48000.0);
    for (int sample = 0; sample < 12000; sample += 97)
        REQUIRE(trimmed.getSample(0, sample) == Catch::Approx(full.getSample(0, sample + 6000)).margin(0.00001f));
    reader.reset();
    REQUIRE(files[0].deleteFile());
    REQUIRE(directory.deleteFile());
}
