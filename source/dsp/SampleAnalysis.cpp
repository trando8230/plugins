#include "SampleAnalysis.h"
#include <array>
#include <vector>

namespace grokfx
{
int sampleKeyShift(const LayerModel& layer, int targetRoot) noexcept
{
    if (!layer.matchKey || layer.sampleRoot == 1)
        return 0;
    const int root = layer.sampleRoot >= 2 ? layer.sampleRoot - 2
        : layer.sample != nullptr ? layer.sample->detectedRoot : -1;
    if (root < 0 || root > 11)
        return 0;
    return ((targetRoot - root + 18) % 12) - 6;
}

void analyseSampleRoot(LoadedSample& sample)
{
    sample.detectedRoot = -1;
    sample.rootConfidence = 0.0f;
    sample.dcSumLeft.clear();
    sample.dcSumRight.clear();
    if (sample.left.size() == sample.right.size())
    {
        double left = 0.0, right = 0.0;
        for (size_t i = 0; i < sample.left.size(); ++i)
        {
            if (i % 256 == 0)
            {
                sample.dcSumLeft.push_back(left);
                sample.dcSumRight.push_back(right);
            }
            left += sample.left[i]; right += sample.right[i];
        }
        if (sample.left.size() % 256 == 0)
        {
            sample.dcSumLeft.push_back(left);
            sample.dcSumRight.push_back(right);
        }
    }
    if (sample.left.size() < 128 || sample.left.size() != sample.right.size()
        || !std::isfinite(sample.sampleRate) || sample.sampleRate < 8000.0)
        return;

    // YIN's cumulative mean-normalised difference rejects noise and octave
    // harmonics. Analyse at most 32 evenly spaced windows, independently of
    // file length. Pick the stronger stereo channel (anti-phase must not cancel).
    constexpr int window = 1024;
    const int stride = std::max(1, static_cast<int>(sample.sampleRate / 12000.0));
    const double rate = sample.sampleRate / stride;
    const int maxLag = static_cast<int>(rate / 40.0);
    const int minLag = std::max(2, static_cast<int>(rate / 1800.0));
    const int frameSize = window + maxLag + 1;
    const int available = static_cast<int>(sample.left.size()) / stride;
    if (available < frameSize)
        return;
    const int frames = std::clamp(available / window, 1, 32);
    std::vector<double> frame(static_cast<size_t>(frameSize));
    std::vector<double> difference(static_cast<size_t>(maxLag + 1));
    std::array<double, 12> votes {};
    double totalWeight = 0.0;
    int tonalFrames = 0;
    for (int f = 0; f < frames; ++f)
    {
        const int first = frames == 1 ? 0 : (available - frameSize) * f / (frames - 1);
        double energyL = 0.0, energyR = 0.0;
        for (int i = 0; i < frameSize; ++i)
        {
            const auto index = static_cast<size_t>((first + i) * stride);
            energyL += sample.left[index] * sample.left[index];
            energyR += sample.right[index] * sample.right[index];
        }
        const auto& channel = energyL >= energyR ? sample.left : sample.right;
        double mean = 0.0;
        for (int i = 0; i < frameSize; ++i)
        {
            // Short box average before decimation reduces high-frequency aliasing.
            double value = 0.0;
            for (int s = 0; s < stride; ++s)
                value += channel[static_cast<size_t>((first + i) * stride + s)];
            frame[static_cast<size_t>(i)] = value / stride;
            mean += value / stride;
        }
        mean /= frameSize;
        double energy = 0.0;
        for (auto& value : frame) { value -= mean; energy += value * value; }
        if (energy / frameSize < 1.0e-7)
            continue;
        double sum = 0.0;
        difference[0] = 1.0;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double delta = 0.0;
            for (int i = 0; i < window; ++i)
            {
                const double d = frame[static_cast<size_t>(i)] - frame[static_cast<size_t>(i + lag)];
                delta += d * d;
            }
            sum += delta;
            difference[static_cast<size_t>(lag)] = sum > 1.0e-12 ? delta * lag / sum : 1.0;
        }
        int lag = minLag;
        for (; lag < maxLag - 1; ++lag)
            if (difference[static_cast<size_t>(lag)] < 0.15)
            {
                while (lag + 1 < maxLag && difference[static_cast<size_t>(lag + 1)] < difference[static_cast<size_t>(lag)]) ++lag;
                break;
            }
        if (lag >= maxLag - 1) continue;
        const double a = difference[static_cast<size_t>(lag - 1)];
        const double b = difference[static_cast<size_t>(lag)];
        const double c = difference[static_cast<size_t>(lag + 1)];
        const double divisor = a - 2.0 * b + c;
        const double refined = lag + (std::abs(divisor) > 1.0e-12 ? 0.5 * (a - c) / divisor : 0.0);
        const double midi = 69.0 + 12.0 * std::log2(rate / refined / 440.0);
        // A sweep between notes is not a dependable source key.
        if (!std::isfinite(midi) || std::abs(midi - std::round(midi)) > 0.35) continue;
        const int note = (static_cast<int>(std::lround(midi)) % 12 + 12) % 12;
        const double weight = (1.0 - b) * std::sqrt(energy / frameSize);
        votes[static_cast<size_t>(note)] += weight;
        totalWeight += weight;
        ++tonalFrames;
    }
    if (totalWeight <= 0.0 || tonalFrames < std::max(1, frames / 4)) return;
    const auto best = std::max_element(votes.begin(), votes.end());
    const double confidence = *best / totalWeight;
    if (confidence < 0.6) return;
    sample.detectedRoot = static_cast<int>(std::distance(votes.begin(), best));
    sample.rootConfidence = static_cast<float>(confidence);
}
}
