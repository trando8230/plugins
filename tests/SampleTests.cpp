#include "Support.h"
#include "AllocationProbe.h"
#include "dsp/SampleAnalysis.h"
#include "dsp/SamplePlayer.h"
#include "dsp/WavExport.h"
#include <array>

namespace
{
grokfx::LoadedSample tone(double rate, double seconds, double hz, bool harmonics = false)
{
    grokfx::LoadedSample sample;
    sample.sampleRate = rate;
    sample.left.resize(static_cast<size_t>(rate * seconds));
    sample.right.resize(sample.left.size());
    for (size_t i = 0; i < sample.left.size(); ++i)
    {
        const double phase = juce::MathConstants<double>::twoPi * hz * static_cast<double>(i) / rate;
        const float value = static_cast<float>(harmonics
            ? 0.08 * std::sin(phase) + 0.25 * std::sin(2 * phase) + 0.15 * std::sin(3 * phase)
            : 0.3 * std::sin(phase));
        sample.left[i] = value;
        sample.right[i] = -value * 0.5f;
    }
    return sample;
}

juce::AudioBuffer<float> play(grokfx::LayerModel layer, double rate, int count, int key = 0, int start = 0)
{
    grokfx::SamplePlayer player;
    player.prepare(rate);
    juce::AudioBuffer<float> output(2, count - start);
    for (int i = start; i < count; ++i)
    {
        float left, right;
        player.read(layer, key, i, count, left, right);
        output.setSample(0, i - start, left);
        output.setSample(1, i - start, right);
    }
    return output;
}

double frequency(const juce::AudioBuffer<float>& audio, double rate, int channel = 0)
{
    const int start = audio.getNumSamples() / 4, end = audio.getNumSamples() * 3 / 4;
    double first = -1.0, last = 0.0;
    int crossings = 0;
    for (int i = start + 1; i < end; ++i)
    {
        const double a = audio.getSample(channel, i - 1), b = audio.getSample(channel, i);
        if (a < 0.0 && b >= 0.0)
        {
            const double position = i - 1 + -a / (b - a);
            if (first < 0.0) first = position;
            last = position;
            ++crossings;
        }
    }
    return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
}
}

TEST_CASE("Sample root detection recognises every key and rejects uncertain audio", "[sample][key]")
{
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (int root = 0; root < 12; ++root)
        {
            CAPTURE(rate, root);
            auto sample = tone(rate, 0.6, grokfx::noteToHz(static_cast<float>(48 + root)), true);
            grokfx::analyseSampleRoot(sample);
            REQUIRE(sample.detectedRoot == root);
            REQUIRE(sample.rootConfidence > 0.9f);
        }
    auto silent = tone(48000.0, 0.5, 0.0);
    grokfx::analyseSampleRoot(silent);
    REQUIRE(silent.detectedRoot == -1);
    juce::Random random(4815);
    for (size_t i = 0; i < silent.left.size(); ++i)
        silent.left[i] = silent.right[i] = random.nextFloat() * 0.6f - 0.3f;
    grokfx::analyseSampleRoot(silent);
    REQUIRE(silent.detectedRoot == -1);
    auto shortSample = tone(48000.0, 0.001, 440.0);
    grokfx::analyseSampleRoot(shortSample);
    REQUIRE(shortSample.detectedRoot == -1);
    auto ambiguous = tone(48000.0, 1.0, 440.0);
    auto alternate = tone(48000.0, 1.0, grokfx::noteToHz(60.0f));
    for (size_t i = ambiguous.left.size() / 2; i < ambiguous.left.size(); ++i)
    {
        ambiguous.left[i] = alternate.left[i];
        ambiguous.right[i] = alternate.right[i];
    }
    grokfx::analyseSampleRoot(ambiguous);
    REQUIRE(ambiguous.detectedRoot == -1);
}

TEST_CASE("Sample fit preserves pitch across lengths rates trims reverse and seeks", "[sample][stretch]")
{
    auto sample = tone(44100.0, 1.0, 440.0);
    grokfx::LayerModel layer;
    layer.sample = &sample;
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const double seconds : { 0.4, 1.0, 2.0 })
        {
            CAPTURE(rate, seconds);
            const auto output = play(layer, rate, static_cast<int>(rate * seconds));
            REQUIRE(frequency(output, rate) == Catch::Approx(440.0).margin(0.6));
            REQUIRE(output.getRMSLevel(0, output.getNumSamples() / 4, output.getNumSamples() / 2) > 0.05f);
            // Phase-inverted stereo must retain its width and phase relationship.
            for (int i = output.getNumSamples() / 4; i < output.getNumSamples() * 3 / 4; i += 113)
                REQUIRE(output.getSample(1, i) == Catch::Approx(-0.5f * output.getSample(0, i)).margin(0.015f));
        }
    layer.sampleStart = 0.2f;
    layer.sampleEnd = 0.8f;
    layer.reverse = true;
    auto reversed = play(layer, 48000.0, 96000);
    REQUIRE(frequency(reversed, 48000.0) == Catch::Approx(440.0).margin(0.6));
    auto sought = play(layer, 48000.0, 96000, 0, 36000);
    REQUIRE(frequency(sought, 48000.0) == Catch::Approx(440.0).margin(0.6));
}

TEST_CASE("Sample key matching manual root and unpitched mode keep duration", "[sample][key][stretch]")
{
    auto sample = tone(48000.0, 0.8, 440.0);
    grokfx::analyseSampleRoot(sample);
    REQUIRE(sample.detectedRoot == 9);
    grokfx::LayerModel layer;
    layer.sample = &sample;
    layer.matchKey = true;
    auto matched = play(layer, 48000.0, 48000, 0);
    REQUIRE(frequency(matched, 48000.0) == Catch::Approx(grokfx::noteToHz(72)).margin(0.8));
    layer.sampleRoot = 2; // manually identify source as C; matching C is unshifted
    auto manual = play(layer, 48000.0, 48000, 0);
    REQUIRE(frequency(manual, 48000.0) == Catch::Approx(440.0).margin(0.6));
    layer.sampleRoot = 1; // unpitched ignores the song key
    auto unpitched = play(layer, 48000.0, 48000, 5);
    REQUIRE(frequency(unpitched, 48000.0) == Catch::Approx(440.0).margin(0.6));
    layer.pitchSemis = 12;
    layer.fineCents = 50.0f;
    auto shifted = play(layer, 48000.0, 48000, 5);
    REQUIRE(frequency(shifted, 48000.0) == Catch::Approx(880.0 * std::pow(2.0, 0.5 / 12.0)).margin(1.0));
    REQUIRE(shifted.getRMSLevel(0, 40000, 4000) > 0.05f); // no early end after pitching up
    REQUIRE(shifted.getNumSamples() == matched.getNumSamples());
}

TEST_CASE("Sample key controls and detection survive project recall", "[sample][key][state]")
{
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("grokfx-key", ".wav", false);
    const auto sample = tone(48000.0, 0.5, 440.0);
    juce::AudioBuffer<float> input(2, static_cast<int>(sample.left.size()));
    input.copyFrom(0, 0, sample.left.data(), input.getNumSamples());
    input.copyFrom(1, 0, sample.right.data(), input.getNumSamples());
    REQUIRE(grokfx::writeStereoWav(file, input, 48000.0, grokfx::WavFormat::float32));
    auto plugin = makePlugin();
    REQUIRE(plugin->loadSample(0, file).wasOk());
    setReal(*plugin, "l1_sampleRoot", 11.0f); // A
    setReal(*plugin, "l1_matchKey", 1.0f);
    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    auto restored = makePlugin();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
    GrokFxAudioProcessor::SampleReadGuard guard(*restored);
    auto model = restored->readModel();
    REQUIRE(model.layers[0].sample != nullptr);
    REQUIRE(model.layers[0].sample->detectedRoot == 9);
    REQUIRE(model.layers[0].sampleRoot == 11);
    REQUIRE(model.layers[0].matchKey);
    REQUIRE(file.deleteFile());
}

TEST_CASE("Sample stretching allocates no C++ storage during playback or seeks", "[sample][realtime][stretch]")
{
    auto sample = tone(48000.0, 0.5, 440.0);
    auto replacement = tone(44100.0, 0.8, 220.0);
    grokfx::analyseSampleRoot(sample);
    grokfx::analyseSampleRoot(replacement);
    grokfx::LayerModel layer;
    layer.sample = &sample;
    grokfx::SamplePlayer player;
    player.prepare(48000.0);
    float left = 0.0f, right = 0.0f;
    bool finite = true;
    size_t allocations = 0;
    {
        allocationProbe::Scope scope;
        for (int i = 0; i < 48000; ++i)
        {
            if (i == 8000) layer.pitchSemis = 7;
            if (i == 16000) layer.sampleStart = 0.2f;
            if (i == 24000) { layer.reverse = true; layer.sample = &replacement; }
            if (i == 32000) player.reset();
            player.read(layer, 0, i < 40000 ? i : i - 30000, 48000, left, right);
            finite = finite && std::isfinite(left) && std::isfinite(right);
        }
        allocations = allocationProbe::count;
    }
    REQUIRE(finite);
    REQUIRE(allocations == 0);
}

TEST_CASE("Custom sample live blocks and exports agree at different tempos", "[sample][stretch][export]")
{
    auto sample = tone(48000.0, 0.8, 440.0);
    grokfx::RenderModel model;
    model.length = static_cast<int>(grokfx::Length::oneBeat);
    model.space = model.delayAmount = model.reverbAmount = 0.0f;
    model.masterWidth = 0.5f;
    auto& layer = model.layers[0];
    layer.enabled = layer.useCustom = true;
    layer.sample = &sample;
    layer.drive = 0.0f;
    for (const double bpm : { 60.0, 120.0, 180.0 })
    {
        CAPTURE(bpm);
        grokfx::Engine offline, live;
        juce::AudioBuffer<float> exported;
        offline.renderOffline(exported, model, bpm, 48000.0);
        REQUIRE(exported.getNumSamples() == grokfx::musicalSamples(model.length, bpm, 48000.0));
        REQUIRE(frequency(exported, 48000.0) == Catch::Approx(440.0).margin(0.7));
        live.prepare(48000.0);
        juce::AudioBuffer<float> streamed(2, exported.getNumSamples());
        // A non-power-of-two host buffer deliberately crosses internal chunks.
        for (int first = 0; first < streamed.getNumSamples(); first += 197)
        {
            float* pointers[2] { streamed.getWritePointer(0, first), streamed.getWritePointer(1, first) };
            live.process(pointers, 2, std::min(197, streamed.getNumSamples() - first), first, model, bpm);
        }
        for (int i = 0; i < streamed.getNumSamples(); ++i)
            for (int channel = 0; channel < 2; ++channel)
                REQUIRE(streamed.getSample(channel, i) == Catch::Approx(exported.getSample(channel, i)).margin(1.0e-6f));
        REQUIRE(exported.getRMSLevel(0, 0, 48) > 0.001f); // no uncompensated start latency
    }
}

TEST_CASE("Detected note labels cover all twelve pitch classes", "[sample][key]")
{
    const std::array<const char*, 12> names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int root = 0; root < 12; ++root)
        REQUIRE(juce::String(grokfx::noteName(root)) == names[static_cast<size_t>(root)]);
}
