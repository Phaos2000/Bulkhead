[![Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/phaos)

# Bulkhead

Bulkhead 1.0.0 is a Windows x64 VST3 reverb by Phaos. It combines a finite
signed reflection-packet engine with Sound Blaster Pro II ADC/DAC audio-path
emulation: clocked unsigned 8-bit PCM conversion, held samples, and selectable
input and output filters.

The reverb runs between the input and output conversion stages. Its selectable
8-, 12-, and 16-bit internal precision is separate from the 8-bit converters.

![Bulkhead 1.0.0 interface](docs/images/bulkhead.png)

## Download

[Bulkhead 1.0.0 for Windows x64 (VST3)](https://github.com/Phaos2000/Bulkhead/releases/tag/v1.0.0)

Download `Bulkhead-1.0.0-Windows-x64.zip` from the release assets. Extract it
and copy the entire `Bulkhead.vst3` folder to your host's VST3 search directory.
The automatic GitHub source archives contain source code, not an installed plugin.

## Signal path

```text
Host input -> optional input low-pass -> clocked 8-bit ADC -> held direct ----+
                                                         |                 |
                                                         v                 |
                          integer PCM reflection chamber -> Body shaping    |
                          -> 8-bit DAC -> held playback                     |
                          -> optional output low-pass -> Output -> Mix -----+ -> output
```

The ADC capture feeds both the direct branch and the reverb. Mix controls how
much reverb is added to the fixed-level ADC direct signal. At 0%, the direct
signal still passes through input filtering, quantization, and sample hold.
Full bypass fades to the original host input.

## Converter emulation

| Stage | Implementation |
|---|---|
| Input conversion | Unsigned 8-bit linear PCM; centred at code 128; saturation at the converter limits |
| Input filter | Off, 3.2 kHz, or 8.8 kHz low-pass; two-pole Butterworth approximation at host sample rate |
| Nominal clock choices | 11.025 kHz and 22.05 kHz |
| Implemented stereo clock rates | 11,111 Hz and 21,739 Hz, derived from integer DSP time-constant divisors |
| Clock changes | Phase-continuous 8 ms ramp; stored reflections change pitch and duration with the clock |
| Output conversion | Unsigned 8-bit linear PCM, independent of internal chamber precision |
| Playback hold | 45,454 Hz converter update grid |
| Output filter | Optional two-pole 3.2 kHz low-pass after the playback hold |
| Dither | Optional TPDF at ADC capture, internal quantization, and final DAC conversion; digital idle silence remains zero |

The scope is the card's audio path. Analogue filter responses are approximated;
the implementation has not been established as a component-level or measured
match to a particular physical card. ISA registers, DMA, interrupts, FM
synthesis, and ADPCM are outside its scope.

The physical Sound Blaster Pro II did not run this reverb and was half-duplex.
Bulkhead uses the modern host's simultaneous input/output to place a software
reverb between the two conversion stages. This continuous live effects chain
is an extension of the hardware workflow. See [DOS-DESIGN.md](DOS-DESIGN.md).

The converter behaviour is informed by Creative's
[Hardware Programming Guide](https://ardent-tool.com/sound/Sound_Blaster_HW_Programming_Guide_1st.pdf)
and the [DOSBox Staging audio-path reference](https://github.com/dosbox-staging/dosbox-staging/blob/c50a9b4c7b09f3cbd3153c3f95afc736134a6dba/src/hardware/audio/soundblaster.cpp).

## Reverb design

- Finite feed-forward reflection cloud with 4-64 packets per channel, each
  comprising three weighted, signed delay reads.
- Two 131,072-word signed 16-bit circular buffers: 512 KiB total delay memory.
- Q15 reflection coefficients and bounded 32-bit products and accumulation.
  Host I/O, converter filter calculations, parameter smoothing, and parts of
  control generation use floating point.
- Geometry changes packet clustering and fragment weights/spacing; Scatter
  changes timing irregularity and polarity. Changed layouts crossfade over
  18 ms using stationary read positions.
- Left and right channels process independently. Width changes path spacing;
  there is no cross-channel signal feed.
- Body applies wet-only low-mid shaping and bounded soft limiting before
  the final DAC conversion.
- The reflection engine uses no recirculating comb/all-pass network, feedback
  delay network, or FFT convolution. It is not based on a named historical reverb.

The arithmetic and memory budget draw on a DOS-era processing design. The
plugin is a modern C++ implementation; real-time operation on a 486 has not
been verified.

## Controls

| Control | Range or function |
|---|---|
| Size | 0.05-32x reflection geometry scale |
| Decay | 0.025-4 s at the 22.05 kHz reference; readout follows the actual selected clock |
| Density | 4-64 reflection packets per channel |
| Geometry | Packet clustering and fragment shape |
| Scatter | Reflection timing irregularity, fragment spacing, and polarity |
| Tone L | Wet-path low cutoff, 0-2,000 Hz; 0 is Off |
| Tone H | Wet-path high cutoff, 350-20,000 Hz, limited by the processing rate |
| Width | Left/right path spacing, 0-100% |
| Body | Wet-path low-mid shaping and soft limiting, 0-100% |
| Output | Wet-only gain, -36 to +12 dB |
| Mix | Added reverb amount, 0-100%; ADC direct level stays fixed |
| Clock | Nominal 11.025 / 22.05 kHz |
| Bits | 8 / 12 / 16-bit internal processing precision |
| T | Truncate fixed-point products and coarsen control coefficients |
| D | Enable dither |
| Input filter | Off / 3.2 kHz / 8.8 kHz |
| Output filter | Off / 3.2 kHz |
| Title button | Full bypass |

## Host and interface

- Windows x64, VST3, matching mono or stereo input/output buses.
- 32- and 64-bit floating-point host buffers; host sample rates 8-384 kHz.
- Zero reported buffering latency; reflection delays, filtering, and converter
  timing still affect the signal. Reported tail length is 13 seconds.
- Native 320x263 pixel interface, fixed 32-colour palette, nearest-neighbour
  scaling, and error-diffused textures. GUI updates and mouse-drag commits are
  capped at 25 Hz; audio and host automation are processed independently.
- Interface scales: 1.5x, 2x, 3x, 4x, and 5x, with host DPI handling.
- One custom arrow cursor with four colour choices, plus the system cursor.
- INIT and five factory presets; user preset load/save/update/rename/delete.

User presets are stored in `%APPDATA%/Bulkhead/Presets`. Size and cursor
preferences also have fallbacks under `%APPDATA%/Bulkhead`.

## Build from source

Install Git, CMake 3.25 or newer, and Visual Studio with the Desktop development
with C++ workload. In PowerShell, from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/fetch-sdk.ps1
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64
cmake --build build-win --config Release --target Bulkhead bulkhead_tests bulkhead_preset_tests bulkhead_preview
ctest --test-dir build-win -C Release --output-on-failure -R "bulkhead_dsp|bulkhead_presets"
```

Use the matching generator for another supported Visual Studio version.
The setup script fetches the official VST3 SDK at `v3.8.1_build_84`, commit
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, with the base, cmake,
pluginterfaces, and public.sdk submodules. VSTGUI is not required.

The plugin bundle is produced at `build-win/VST3/Release/Bulkhead.vst3/`.
Copy the entire bundle to your host's VST3 search directory, commonly
`C:/Program Files/Common Files/VST3`, and rescan.

Tests write diagnostic logs/audio and temporary presets under `artifacts/`.
The diagnostic audio is generated from synthetic excitation. It is not a
collection of sampled instruments or commercial recordings.

This repository contains source code. Historical archives, local build files,
and downloaded reference artwork are excluded. Before redistributing a build,
include `LICENSE`, `THIRD_PARTY_NOTICES.md`, and `LICENSES/` with the plugin.

## License and attribution

Copyright (c) 2026 Phaos - phaos2000@pm.me.

Bulkhead uses the custom [Bulkhead Freeware and Source-Available License](LICENSE).
Free use includes commercial music, recordings, performances, and paid audio
production. No royalties or credits in musical output are required.

Redistribution must preserve attribution and license notices. Distributed
modifications must include complete corresponding source under the same terms,
identify changes, and be marked unofficial. Before releasing a modified version,
email phaos2000@pm.me with a description and source location; a reply is not required.
Selling the plugin, paid bundles/services containing it, or commercial code
reuse requires separate written permission. The full license controls.

Dependencies and embedded artwork have separate terms. The title font and
cursor artwork have unresolved redistribution permissions, documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The custom license grants no
additional rights to those materials.

VST is a registered trademark of Steinberg Media Technologies GmbH.
Sound Blaster and Sound Blaster Pro are trademarks of Creative Technology Ltd.
Bulkhead is an independent project and is not endorsed by either company.
