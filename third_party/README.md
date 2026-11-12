# Signalsmith DSP dependencies

Vendored source, with licenses also included in release archives:

- Signalsmith Stretch 1.3.2, commit `88061fd4eda56a2353c5d65c74aa3f4ccf76f9da` from https://github.com/Signalsmith-Audio/signalsmith-stretch
- Signalsmith Linear 0.6.6, commit `7bb1c8f36794d6980080ce817c91f8da8fe56a66` from https://github.com/Signalsmith-Audio/linear

Both use the MIT license (see `licenses/MIT-Signalsmith-*.txt`). Only the C++ headers needed for the portable FFT implementation are included; no binary downloads or platform frameworks are required.

Local change to Stretch: bypass the optional silence-detection input scan when an input span exceeds one analysis window. The renderer can skip large source spans during severe compression; this keeps processing work bounded by the configured window/output length instead of scanning millions of skipped frames. Audio analysis and synthesis are unchanged. The maximum-size compression/expansion regression exercises these paths.

`SamplePlayer` allocates in `prepare`, uses bounded output chunks and a virtual input buffer, and compensates latency on seeks. Upstream `process` resizes scratch vectors only within the capacity configured by `configure`; the allocation regression covers first playback, transposition, sample replacement, trims, reverse, and seeking.
