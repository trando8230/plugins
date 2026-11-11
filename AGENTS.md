# Grok FX agent notes

- Plugin identity is stable: product name `Grok FX`, manufacturer `Rando Audio`, bundle id `com.randoaudio.grokfx`, plugin code `Grok`, manufacturer code `Raud`. Hosts identify a saved set by these codes. Enforced by the Catch2 test `Plugin identity codes stay stable`.
- Parameter IDs are stable for the same reason. Do not rename an ID that has shipped. Enforced by `Required parameters exist with host-facing names` and `State round-trips through save and reload`.
- Host tempo falls back to 120 BPM when the playhead is missing or the tempo is non-positive or non-finite. Enforced by `Tempo falls back to 120 BPM when the host has no tempo or playhead`.
- Musical length is `llround(beats * sampleRate * 60 / bpm)` in 4/4, and the impact or drop is the last musical sample before the tail. Enforced by `Rendered length matches every musical option at 120 and 130 BPM` and `Impact and riser land on the drop`.
- `processBlock` does not allocate, lock, log, read or write files, or repaint. Waveform and meter repaints come from the editor's 60 fps timer. Enforced by `Engine::process` staying on preallocated buffers and by the DSP safety tests.
- Non-finite samples are counted and replaced with silence, and subnormals in the output are flushed to zero. Enforced by `No NaNs or Infs at parameter extremes`, `Each parameter extreme stays finite and bounded`, and `Output stays bounded` inside those tests.
- Saved state must round-trip to the same bytes, including UI scale. Enforced by `State round-trips through save and reload`.
- A MIDI one-shot does not replay the opening after the drop. Enforced by `MIDI one-shot stays silent after the drop`.
- The first onset of every pad, including the snare downbeat at the default tail, lands within 1 ms of its scheduled sample. Enforced by `First hit of every pad lands within 1 ms`.
- Every pitched pad stays within 1 cent of `noteToHz` in all 12 keys at 44.1, 48, and 96 kHz. Enforced by `Pitched pads stay within 1 cent in every key`.
- Rendered length is bars times beats times 60 / BPM times the sample rate, plus the tail, exact to the sample and not rounded to the block size. Enforced by `Rendered length is exact for every bar count tempo and rate`.
- A/B slot XML stored in the session contains parameter values only. Enforced by `A/B recall does not nest slot XML`.
- Preset recall resets the play cursor on atomics the audio thread can read. Enforced by `Preset recall resets the play cursor`.
- Drag starts from a WAV the peak thread wrote when the parameters changed. Enforced by `Drag file is ready before the gesture`.
- A failed stem write deletes that export and surfaces the error. Enforced by `Failed stem write surfaces an error`.
- Closing the editor stops the peak render between chunks instead of killing the thread. Enforced by `Closing the editor stops the peak render between chunks`.
- The release zip includes the full GNU AGPL-3.0 text and the Inter OFL, and the macOS job checks `lipo -archs` for arm64 and x86_64. Enforced by `Release package lists the AGPL and Inter OFL`.
