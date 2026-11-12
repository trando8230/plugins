#pragma once
#include "Model.h"

namespace grokfx
{
// Runs only while loading a file, before publishing its immutable sample data.
// Detects the dominant periodic root; ambiguous/noisy material remains unknown.
void analyseSampleRoot(LoadedSample& sample);
int sampleKeyShift(const LayerModel& layer, int targetRoot) noexcept;
}
