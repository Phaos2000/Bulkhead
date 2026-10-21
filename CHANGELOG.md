# Changelog

## 1.0.1 - 2026-10-21

- Change the main `TONE` label to `Tone`, matching the other dial labels.
- Reduce audio-thread work with exact PCM rounding, cached fixed-point scales,
  and reflection kernels selected once per chamber tick for settled precision,
  truncation mode, and read-head position. Automated transitions use the existing
  general arithmetic and crossfades.
- Add PCM boundary regressions and a compiled-plugin output/CPU comparison tool.
- Update the interface screenshot and report version 1.0.1 in the interface and
  VST3 metadata.

The signal path, converter clocks, filter coefficients, reflection geometry,
quantization rules, dither, presets, parameter IDs, and saved-state formats are
unchanged.

### Performance and validation

Compared with the released 1.0.0 Windows x64 plugin, CPU time decreased by
12-24% across six measured workloads, including about 15% at the default
48 kHz settings. These are measurements on the test computer, not guaranteed
savings on every system. Each result is the median of seven alternating
old/new runs of five seconds of stereo audio on one CPU core, using 512-sample
blocks and 64-bit host buffers.

- 288 compiled-plugin cases / 6,912,000 frames matched 1.0.0 bit-for-bit,
  covering mono/stereo, 32/64-bit audio, 8-384 kHz sample rates, variable block
  sizes, sample-offset automation, and silence flags.
- 148 source-engine cases / 13,056,000 stereo frames matched bit-for-bit,
  including resets and long tails.
- 83 DSP, 16 preset, 155 host/editor, and 47 Steinberg VST3 checks passed.

## 1.0.0 - 2026-10-19

- Initial public Windows x64 VST3 release, with source, technical documentation,
  interface screenshot, license notices, and archive checksum.
