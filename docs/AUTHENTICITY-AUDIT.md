# Authenticity audit — Bulkhead 2.0.1

2026-10-27. Windows checkpoint. Version 2.0.1 corrects the product name to
**Bulkhead**, retaining the 2.0.0 DSP, state and plugin IDs. The separate DOS study is preserved and paused.

| Area | Contract and limits |
|---|---|
| Converters | Unsigned 8-bit PCM, 128 silence and converter saturation retained. No 16-bit ADC/DAC or invented stereo rate. |
| Stereo modes | Nominal 11.025/22.05 kHz. The retained integer time-constant policy implements 11,111/21,739 Hz; labels are nominal. |
| Dry/wet topology | Captured dry and processed wet sum before a single final DAC, as in software processing of a captured sample. Mix 0 still converts; output filtering affects the complete sum. Host bypass is raw input. |
| Wet history | One 131,072-word signed 16-bit ring, 256 KiB actually allocated. Mono history and software stereo packet panning are original, plausible period programming choices, not SB Pro hardware modes. Opposite-polarity stereo input can cancel in wet processing. |
| Stereo balance | Decay-weighted coefficient normalization per channel corrects the previous software bias. Setup work adds no sample-loop multiplies. This is fixture balance, not listening acceptance. |
| Sample loop | Three history reads and six coefficient multiplies per active packet, plus filtering, mixing and other work. Changing layouts temporarily doubles reflection work during crossfades. |
| Coefficients | Fragment/packet/pan products are precombined into Q15 coefficients at control rate. This removes intermediate packet quantization/clipping and intentionally changes sound. |
| Precision | Internal 8/12/16-bit settings are independent of the 8-bit converters. No-dither default; optional TPDF is software behavior, not an asserted card circuit. |
| Timing/filters | The retained 45,454 Hz update grid follows an emulator reference. Host-rate two-pole analogue-filter approximations and simultaneous channel capture/hold are not a measured interleaved card implementation. |
| Live I/O | One physical SB Pro II is half-duplex. Continuous live VST input/output is a modern extension. A faithful single-card DOS workflow captures/loads first, then processes/plays. |
| Controls/Body | Floating-point host ramps, clock glides, layout crossfades and Body are original modern implementation choices. Period equivalents require separate costing/implementation. |
| 486 | 16-24 packets is a design ambition, not a validated DX2/66 budget. Prior instruction estimates did not substantiate strict real-time feasibility for the current kernel. No such claim is made. |
| Mono 44.1 kHz | Documented hardware capability, deferred until explicit mono converter, output and memory policies are designed. |
| Interface | Established DOS-inspired appearance retained in a modern Windows editor. |

## Validation boundary

The Windows tests check converter code boundaries, the ADC/DAC cascade, bounded
chamber output, deterministic reset, automation, stereo balance fixtures, preset
and state behavior, native editor behavior and compiled-plugin/source equivalence.
Unfiltered cascade values require exact equality; independently compiled filtered
floating-point recurrences allow 1e-12 absolute error. This does not relax PCM
code checks. Detailed results accompany the checkpoint package.

The active-ramp mask and settled filter-coefficient cache reduce avoidable host
work. No new Windows timing result is presented as a 486 measurement. The fixed
architecture removes legacy buffers and code rather than promising compatibility.
Version 2 has new plugin IDs and rejects prior audio-state/preset formats.

Earlier DOS kernel execution studies are historical evidence only. They are
not a complete application, soundcard driver, physical-card test or measured
486 DMA deadline/underrun result. No DOS work was resumed for this release.
Listening acceptance in the user's DAW and real-card/486 validation remain open.

## Primary and implementation references

- Creative, *Sound Blaster Series Hardware Programming Guide*, DSP 3.xx mode
  tables, command 40h, CT1345 mixer/filter sections:
  https://pdos.csail.mit.edu/6.828/2005/readings/hardware/SoundBlaster.pdf
- DOSBox Staging, pinned audio-path reference (emulator evidence):
  https://github.com/dosbox-staging/dosbox-staging/blob/c50a9b4c7b09f3cbd3153c3f95afc736134a6dba/src/hardware/audio/soundblaster.cpp
- Intel, *i486 Microprocessor*, November 1989, 240440-002, table 10.1 and its
  cache/memory assumptions:
  https://bitsavers.trailing-edge.com/components/intel/80486/240440-002_i486_Microprocessor_Nov89.pdf
