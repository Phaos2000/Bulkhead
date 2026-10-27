> Historical development notes, superseded by Bulkhead 2.0.0. Current routing,
> memory and compatibility decisions are in [the authenticity audit](docs/AUTHENTICITY-AUDIT.md).
> Direct/legacy options and session migration described below were removed.
> DOS work remains paused and is not validated against the version 2 API.

# Bulkhead: 1994 systems-programmer audit

## Current 1.1.0 checkpoint

See [docs/1994-CHECKPOINT.md](docs/1994-CHECKPOINT.md) for the new shared-history
design, historical sources, mono investigation, state migration, and validation.
New instances use direct dry, 20 shared packets, 22.05 kHz nominal stereo and
no dither. Legacy mode retains the design below for existing projects.
Shared mode halves the active history/read traffic and precombines coefficients;
the VST still reserves the legacy second ring for allocation-free mode changes.
This is not a measured 486 real-time implementation.

## Retained legacy design and audit

I treat the sound card as PCM I/O hardware. The reverb is my program's job on
the 486. I would first capture or load a sample into RAM, then process it into
playback DMA buffers. The SB Pro 2 is not a full-duplex live effects processor.
Analogue monitoring through its mixer does not run the signal through my code.

## The routine I am budgeting

Two 131,072-word int16 circular buffers cost 512 KiB. Index reads and writes
with a mask, not division. ADC bytes become signed PCM words before the
software filters. Packet reads stay within the buffer; three signed fragments
are combined with individual Q15 coefficients, then multiplied by a normalized gain.
Filter products and packet products fit signed 32-bit intermediates. The
unit-energy fragment weights have an absolute sum at most sqrt(3), apart from
Q15 rounding. The summed products stay below 1.861 billion, below INT32_MAX.
The bounded sum of 64 clipped packet products also fits a 32-bit accumulator.
Saturate instead of wrapping, and reduce products at each arithmetic stage.
TRUNC deliberately discards low product bits and quantizes coefficient updates.

I keep the two channels separate. Stereo doubles memory and core work, but
needs no elaborate stereo matrix or crossfeed. Width changes read positions;
Scatter changes irregularity. The finite signed packets are this program's
bespoke reverb design, not a stock Schroeder, FDN or convolution routine.

## DMA and deadlines on a real DOS build

Use an auto-initialize 8-bit DMA playback buffer split into two halves. Put
only interrupt acknowledgement and a ready-half flag in the ISR. Render the
free half in the main loop, before the DMA reaches it again. Bound every loop;
never allocate, access the filesystem or recalculate a patch inside the IRQ.
For example, a half containing 256 stereo frames gives about 11.8 ms at the
21,739 Hz playback rate. That is a refill deadline, not zero I/O latency.
Service audio before drawing the next interface frame. Keep the UI at a
modest fixed rate and let coefficient changes use small integer ramps.

Precompute logarithmic knob mappings, coefficient tables, path arrangements
and decay weights outside the sample loop. A strict no-FPU DOS port would
replace the current floating-point control mapping/ramping with integer tables
and accumulators. Real card analogue filters cost no 486 cycles; the host-side
ADC/DAC filter simulations are a separate modern emulation workload.

## What the current VST proves, and what it does not

It proves bounded sample memory, finite tails, 32-bit product bounds, stable
channel isolation, converter grids, control transitions and host usability.
It is a Windows VST implementation, not a DOS executable or a DMA driver.
Setup, ramps and host conversion still use floating point. It therefore does
not yet qualify as a demonstrated strict no-FPU, real-time 486 routine.

At 32 packets, three reads per packet per channel require 192 history reads
per engine frame: about 4.17 million reads/s at the high clock. Maximum density
doubles that before geometry fades and filtering. A convincing 486 claim
requires profiling the integer DOS build, including RAM wait states, interrupt
load and worst-case parameter changes. The maximum density is especially
unproven; a lower budget or a faster early Pentium may be needed. Modern-core
benchmarks cannot establish this. Do not call these chosen limits universal
1994 limits or claim the SB Pro 2 itself ran this reverb.

The user authorized the 0.5.6 Geometry revision described below. Further work
should preserve its sound unless another audible change is requested.
A complete DOSBox device emulator and
Creative ADPCM are outside the agreed stereo PCM audio-path scope.

## References

- Creative Sound Blaster Series Hardware Programming Guide: DSP v3 input/output
  capability tables, CT1345 register map and input/output filtering.
  https://ardent-tool.com/sound/Sound_Blaster_HW_Programming_Guide_1st.pdf
- DOSBox Staging audio-path reference (original Bulkhead code, no direct port):
  https://github.com/dosbox-staging/dosbox-staging/blob/main/src/hardware/audio/soundblaster.cpp

## 0.5.4 envelope and Body refinement

Start decay weights at the first active packet. This shifts all exponents by a
common origin, preserving their relative envelope while avoiding tiny numbers
and the normalization floor when Size is large and Decay very short. Delay
positions, circular-buffer capacity and the clock policy remain unchanged.

Body adds four one-pole filter states per channel: a 200/1400 Hz band estimate
and two low-pass stages on the added saturated component. Cubic weighting of
the knob preserves gentle lower settings; an inexpensive bounded cubic shapes
the low-mid band at the top. Coefficients depend on the native clock and are
configured at control updates. No FFT, oversampling, lookahead or envelope
follower is added. The VST Body filters still use floating point, so a strict
486 port must convert these to fixed-point coefficients and accumulator states
and include their cost in a measured cycle budget. No new strict-486 claim.

The former Tone enable state ID remains hidden for compatibility. The main
L/H controls always operate, while ADC/DAC filter options remain independent.

## 0.5.5 refinement

Scatter also controls the two relative fragment offsets, from 0.2 to 3.0 times
their base spacing. All three positions in each signed packet participate in
stationary read-head fades. The sample loop still reads three fragments per
packet per channel. Read-head metadata grows, but the two 512 KiB sample rings
and 64-packet cap remain unchanged. Base delays reserve 4096 samples for packet
offsets, bounding all reads within the circular history.

Body's band begins at 120 Hz, with 1.5 times the prior added contribution. Its
new symmetric monotone soft limiter is linear through 0.82, uses a cubic knee,
and approaches a 0.98 ceiling. Limiting fades in with the Body control. Output
gain remains downstream of this shaping and affects only wet audio.

ADC -> CPU routine -> DAC remains the hypothetical processing flow. SB Pro
hardware is half-duplex; the emulated continuous path does not reproduce that
driver/DMA limitation. Linux's original SB driver marks the PCM device
SNDRV_PCM_INFO_HALF_DUPLEX. No emulator implementation code has been imported.

## 0.5.6 Geometry revision

Geometry folds the bit-reversed reflection distribution toward eight uneven
cluster centres, and expands each three-fragment impact by a further 10:1
spacing range. Scatter controls irregularity and retains its independent
15:1 spacing factor. The combined maximum offset is bounded per packet when
choosing the base delay; it no longer depends on a fixed 4096-sample reserve.

Fragment weights move from (1, .45, -.15) to (1, .90, -.75), normalized to unit
energy and quantized to Q15 at control updates. The positive first fragment
remains the anchor. There is no new feedback, crossfeed, modulation, buffer,
or read. There are two additional 32-bit multiplies per packet per channel:
128 per engine frame at 32 packets, or 256 at 64 packets. During geometry
fades both complete layouts are evaluated as before. Coefficients travel with
the read positions so the 18 ms stationary crossfade covers the entire shape.
Modern floating-point control calculations remain outside the inner read loop.

72 DSP checks pass, including all three Geometry regions, both clocks and
Scatter endpoints, automation, short decay/large size, and channel isolation.
The transient comparison varied by 0.76 to 4.77 dB across Geometry settings;
unit-energy coefficients do not imply source-independent loudness matching.
Measured modern CPU use: 2.07% / 3.51% of one core at 32 / 64 packets, 48 kHz
host rate. These measurements do not establish a strict 486 cycle budget.

## 0.5.7 shared input ADC

The user requested mandatory ADC colour, including the direct signal. Capture
now happens before splitting into the direct and reverb branches. A pair of
held samples exposes that same 8-bit capture between native input-clock ticks.
Mix is an additive reverb amount; it never attenuates the ADC direct branch.
The DAC, output filter and Output gain remain on the reverb branch. Full host
bypass fades to the original input. No new buffers, latency or history reads.

A separate ADC dither RNG prevents wet-path changes from changing the direct
signal. It resets deterministically. Chamber injection fades after capture
when bypassing, so bypass does not attenuate or quantize the shared capture.
This remains a hypothetical continuous input-processing-output model, not a
claim that the half-duplex physical SB Pro 2 could record/play simultaneously.
