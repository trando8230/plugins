#include "SamplePlayer.h"
#include "SampleAnalysis.h"
#include <signalsmith-stretch.h>

namespace grokfx
{
namespace
{
double regionMean(const std::vector<float>& data, const std::vector<double>& sums,
                  double first, double last) noexcept
{
    if (sums.empty() || last <= first) return 0.0;
    auto integral = [&](size_t end)
    {
        const auto block = end / 256;
        if (block >= sums.size()) return 0.0;
        double sum = sums[block];
        for (size_t i = block * 256; i < end; ++i) sum += data[i];
        return sum;
    };
    const auto begin = static_cast<size_t>(first);
    const auto end = std::min(data.size(), static_cast<size_t>(std::ceil(last)));
    return end > begin ? (integral(end) - integral(begin)) / static_cast<double>(end - begin) : 0.0;
}
// A virtual input buffer resamples to the engine rate before time stretching.
// No temporary input allocation, including on seeks or extreme length ratios.
struct SampleInput
{
    const LoadedSample& sample;
    double first, last, rate, offset;
    bool reverse;
    float dc[2] {};
    struct Channel
    {
        const SampleInput& input;
        const std::vector<float>& data;
        float dc;
        float operator[](int index) const noexcept
        {
            const double distance = (input.offset + index) * input.rate;
            if (distance < 0.0 || distance >= input.last - input.first) return 0.0f;
            const double p = input.reverse ? input.last - 1.0 - distance : input.first + distance;
            const double bounded = std::clamp(p, 0.0, static_cast<double>(data.size() - 1));
            const auto i = static_cast<size_t>(bounded);
            const auto j = std::min(i + 1, data.size() - 1);
            const float fraction = static_cast<float>(bounded - static_cast<double>(i));
            return data[i] + fraction * (data[j] - data[i]) - dc;
        }
    };
    Channel operator[](int channel) const { return { *this, channel == 0 ? sample.left : sample.right, dc[channel] }; }
};
}

struct SamplePlayer::State
{
    signalsmith::stretch::SignalsmithStretch<float> stretch { 0 };
    static constexpr int chunk = 128;
    float output[2][chunk] {};
    double sampleRate = 44100.0;
    double inputCursor = 0.0;
    double speed = 1.0;
    const LoadedSample* previousSample = nullptr;
    float start = -1.0f, end = -1.0f, pitch = 0.0f;
    bool reverse = false;
    int64_t length = 0, next = -1;
    int cursor = chunk;
    float dc[2] {};
};

SamplePlayer::SamplePlayer() = default;
SamplePlayer::~SamplePlayer() = default;

void SamplePlayer::prepare(double sampleRate)
{
    if (!state) state = std::make_unique<State>();
    state->sampleRate = sanitizeSampleRate(sampleRate);
    // A power-of-two analysis window (~170 ms at 48/96 kHz) gives stable
    // frequency estimation and efficient portable FFTs on both target OSes.
    int window = 512;
    while (window < state->sampleRate * 0.16) window *= 2;
    state->stretch.configure(2, window, window / 4);
    reset();
}

void SamplePlayer::reset() noexcept
{
    // Invalidate only: the next read resets the already allocated DSP storage.
    if (state) { state->next = -1; state->previousSample = nullptr; }
}

void SamplePlayer::read(const LayerModel& layer, int keyRoot, int64_t position, int64_t length,
                        float& left, float& right) noexcept
{
    left = right = 0.0f;
    if (!state || !layer.sample || length < 1 || position < 0 || position >= length) return;
    const auto& sample = *layer.sample;
    if (sample.left.size() < 2 || sample.right.size() != sample.left.size()
        || !std::isfinite(sample.sampleRate) || sample.sampleRate <= 0.0) return;
    auto& s = *state;
    const float start = std::clamp(layer.sampleStart, 0.0f, 1.0f);
    const float end = std::clamp(layer.sampleEnd, start, 1.0f);
    if (end <= start) return;
    const float pitch = static_cast<float>(layer.pitchSemis + sampleKeyShift(layer, keyRoot)) + layer.fineCents / 100.0f;
    const double frames = static_cast<double>(sample.left.size());
    const double sourceRate = sample.sampleRate / s.sampleRate;
    const double selectedFrames = (end - start) * frames / sourceRate;
    const double speed = selectedFrames / static_cast<double>(length);
    SampleInput input { sample, start * frames, end * frames, sourceRate, 0.0, layer.reverse };
    if (s.next != position || s.previousSample != &sample || s.start != start || s.end != end
        || s.reverse != layer.reverse || s.length != length || s.pitch != pitch)
    {
        s.dc[0] = static_cast<float>(regionMean(sample.left, sample.dcSumLeft, input.first, input.last));
        s.dc[1] = static_cast<float>(regionMean(sample.right, sample.dcSumRight, input.first, input.last));
        input.dc[0] = s.dc[0]; input.dc[1] = s.dc[1];
        s.stretch.setTransposeSemitones(pitch);
        input.offset = static_cast<double>(position) * speed;
        const int preRoll = s.stretch.outputSeekLength(static_cast<float>(speed));
        // Compensate both input and output latency using random access to the
        // loaded sample. Preview, MIDI and exports start at the requested frame.
        s.stretch.outputSeek(input, preRoll);
        s.inputCursor = input.offset + preRoll;
        s.previousSample = &sample;
        s.start = start; s.end = end; s.reverse = layer.reverse;
        s.length = length; s.pitch = pitch; s.speed = speed;
        s.cursor = State::chunk;
    }
    if (s.cursor == State::chunk)
    {
        input.dc[0] = s.dc[0]; input.dc[1] = s.dc[1];
        const double nextCursor = s.inputCursor + State::chunk * s.speed;
        const auto inputStart = static_cast<int64_t>(std::floor(s.inputCursor));
        const int inputCount = static_cast<int>(std::floor(nextCursor) - static_cast<double>(inputStart));
        input.offset = static_cast<double>(inputStart);
        float* outputs[2] { s.output[0], s.output[1] };
        s.stretch.process(input, inputCount, outputs, State::chunk);
        s.inputCursor = nextCursor;
        s.cursor = 0;
    }
    left = s.output[0][s.cursor] + s.dc[0];
    right = s.output[1][s.cursor] + s.dc[1];
    ++s.cursor;
    s.next = position + 1;
}
}
