# Changelog

## 2.0.1 - 2026-10-27

- Name the plugin **Bulkhead** in the host, controller and bundle. The product
  name has no version suffix; 2.0.1 is the version number. Bundle: `Bulkhead.vst3`.
- Keep the version 2 processor/controller IDs, saved-state format, presets,
  settings directory and DSP unchanged. Existing 2.0.0 sessions remain valid.
- Replace the 2.0.0 bundle when upgrading. For side-by-side version 1 use,
  keep the two complete bundles in separate version subfolders.

## 2.0.0 - 2026-10-27 Windows checkpoint

This is an intentionally different sound and a separate plugin, **Bulkhead 2**.
The old release remains usable alongside it.

- Use distinct processor/controller IDs, `Bulkhead2.vst3`, version 2.0.0 and
  separate `%APPDATA%/Bulkhead2` preferences/presets. No old audio-state or preset
  migration; version 2 rejects older formats instead of guessing their meaning.
- Fix one route: optional input filter -> unsigned 8-bit ADC -> captured dry
  plus shared-history stereo wet -> one final unsigned 8-bit DAC -> output filter.
  Mix at zero remains converted. Output controls wet gain before the final DAC.
- Remove Direct/ADC dry and Shared/Legacy wet choices and their fallback code.
  The single mono wet-history ring now allocates 256 KiB. Stereo input is averaged
  for wet processing; opposite-polarity signals can cancel in the wet path.
- Precombine fragment, packet and stereo gains; normalize decay-weighted energy
  per output channel to correct the right-heavy image without extra sample-loop
  multiplies. Preserve the existing interface premise and converter specifications.
- Default to nominal 22.05 kHz, 20 packets and no dither. Keep 11.025 kHz as
  economy/extreme. Factory densities centre on 16-24, with one sparse 8-packet sound;
  the available range remains 4-64. This is not a proven 486 real-time budget.
- Retain active-ramp-only advancement and cached settled filter coefficients.
  Windows optimization does not establish performance on a 486.
- Validate converter/reference behavior, stereo fixtures, presets, editor/state,
  and compiled-plugin processing. DOS work and 44.1 kHz mono remain deferred.

## 1.1.0 - superseded development checkpoint (not released)

- Correct right-heavy shared wet output by normalizing decay-weighted packet
  energy separately per channel during configuration. No extra sample-loop
  multiplies; legacy mode is unchanged. Add factory/default stereo-balance checks.
- Advance only parameter ramps that are actually moving and cache settled
  fixed-point filter coefficients. Ramp arithmetic and filter rounding rules
  are unchanged; both shared and legacy output are checked bit-for-bit.

- Default to analogue/direct dry plus a shared mono wet history with stereo
  packet panning, 20 packets and the existing nominal 22.05 kHz stereo clock.
  Keep unsigned 8-bit ADC/DAC and no-dither default; 11.025 kHz is economy/extreme.
- Precombine fragment/packet/pan coefficients in shared mode at control rate.
  Keep 4-64 density mapping, with factory sounds now at 8/16/20/24 packets.
- Add independent Direct dry and Shared history choices in Options. Wet topology
  changes fade out/in and clear the wet tail without allocation. Active history
  falls to 256 KiB; 512 KiB remains reserved for legacy switching.
- State v8 and preset v6 append two IDs; earlier states/presets select legacy
  ADC dry and independent stereo wet. The released arithmetic is retained.
- Investigate high-speed 44.1 kHz mono; defer it pending an explicit mono
  converter/output and memory/decay policy. Do not present it as stereo.
- See docs/1994-CHECKPOINT.md for evidence and unresolved DOS/486 validation.

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
