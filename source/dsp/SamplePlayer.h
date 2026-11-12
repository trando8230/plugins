#pragma once
#include "Model.h"
#include <memory>

namespace grokfx
{
// Independent time and pitch playback. All storage is allocated in prepare().
class SamplePlayer
{
public:
    SamplePlayer();
    ~SamplePlayer();
    void prepare(double sampleRate);
    void reset() noexcept;
    void read(const LayerModel& layer, int keyRoot, int64_t position, int64_t length,
              float& left, float& right) noexcept;
private:
    struct State;
    std::unique_ptr<State> state;
};
}
