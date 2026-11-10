# Grok FX agent notes

- Plugin identity is stable: product name `Grok FX`, manufacturer `Rando Audio`, bundle id `com.randoaudio.grokfx`, plugin code `Grok`, manufacturer code `Raud`. Hosts identify a saved set by these codes. Enforced by the Catch2 test `Plugin identity codes stay stable`.
- Parameter IDs are stable for the same reason. Do not rename an ID that has shipped. Enforced by `Required parameters exist with host-facing names` and `State round-trips through save and reload`.
- Host tempo falls back to 120 BPM when the playhead is missing or the tempo is non-positive or non-finite. Enforced by `Tempo falls back to 120 BPM when the host has no tempo or playhead`.
- Musical length is `llround(beats * sampleRate * 60 / bpm)` in 4/4, and the impact or drop is the last musical sample before the tail. Enforced by `Rendered length matches every musical option at 120 and 130 BPM` and `Impact and riser land on the drop`.
- `processBlock` does not allocate, lock, log, read or write files, or repaint. Waveform and meter repaints come from the editor's 60 fps timer. Enforced by `Engine::process` staying on preallocated buffers and by the DSP safety tests.
- Non-finite samples are counted and replaced with silence, and subnormals in the output are flushed to zero. Enforced by `No NaNs or Infs at parameter extremes`, `Each parameter extreme stays finite and bounded`, and `Output stays bounded` inside those tests.
- Saved state must round-trip to the same bytes, including UI scale. Enforced by `State round-trips through save and reload`.
