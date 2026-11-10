# Grok FX

Grok FX is a stereo instrument by Rando Audio. It builds transition effects in the song's key and tempo: risers, downlifters, impacts, snare rolls, noise sweeps, reverse-cymbal swells and sub drops. Load it on a MIDI track, preview the mix, then drag the waveform card onto an audio track.

This repository is phase 1. Custom sample import, key detection, slicing, stretch and presets are not in this build. Each sound pad still has a drop zone so that path can land later; it shows "Coming soon" and does not load the file.

## Identity

These codes are part of the plugin's saved identity. They live in `CMakeLists.txt` and are generated into `PluginIdentity.h`.

| | |
| --- | --- |
| Product | Grok FX |
| Manufacturer | Rando Audio |
| Bundle ID | `com.randoaudio.grokfx` |
| Plugin code | `Grok` |
| Manufacturer code | `Raud` |

## Build

The project downloads JUCE 8.0.15 and Catch2 with CMake. You need a C++20 compiler, CMake 3.25 or newer, and Ninja.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Formats:

- macOS: VST3, AU (`Grok FX.component`) and Standalone, universal arm64 + x86_64
- Windows: VST3 and Standalone

There is no VST2, and this repo does not sign or notarize.

Bundles are written under `build/GrokFX_artefacts/Release/`. See `packaging/INSTALL.txt` for the folders Ableton scans and for the macOS Privacy & Security approval.

## Layout

Pick, Mix and Drop sit left to right. Pick chooses a bank and a type for the armed layer. Mix is four layer strips. Drop shows the rendered clip, the preview transport, the macros and the master. Drag the waveform to write a stereo WAV named `GrokFX_<types>_<key>_<bpm>bpm_<length>.wav`.

The UI uses an 8 px grid, the Inter family (SIL Open Font License, see `licenses/OFL-Inter.txt`) and one accent blue on a light glass surface. The window keeps its aspect ratio from 75% to 200% and stores that size in the plugin state.

## Tests and CI

Catch2 covers silence, finite output at parameter extremes, 44.1/48/96 kHz, block sizes 1/64/2048, the 120 BPM fallback, render length at 120 and 130 BPM, state recall, undo, A/B and the identity codes. GitHub Actions builds macOS and Windows, runs pluginval at strictness 10, runs `auval -v` on the Audio Unit, and uploads a zip plus GUI snapshots.

## License

Grok FX is licensed under the GNU Affero General Public License v3.0 because it is built with JUCE under JUCE's open-source license. Inter is used under the SIL Open Font License. No paid signing service is configured.
