> Historical development notes, superseded by Bulkhead 2.0.0. Current routing,
> memory and compatibility decisions are in [the authenticity audit](AUTHENTICITY-AUDIT.md).
> Direct/legacy options and session migration described below were removed.
> DOS work remains paused and is not validated against the version 2 API.

# SB Pro II / 1994 direction — 1.1.0 development checkpoint

This checkpoint starts from clean public main 5b94d1665f1c6aa347b14c478386c702de47d401
(1.0.1). It is a new sound option, not a claim of sonic parity or 486 feasibility.
The released converter code, timer choices, unsigned 8-bit coding, ADC/DAC filter
approximations and no-dither default are retained. No hardware upgrade to 16-bit
or arbitrary stereo rates is implied.

## Historical contract and assumptions

Creative's *Sound Blaster Series Hardware Programming Guide*, tables 3-2/3-3
(printed 3-8/3-9; PDF pages 38/39), lists DSP 3.xx unsigned 8-bit high-speed
stereo at nominal 11,025/22,050 Hz, and high-speed mono through 44,100 Hz.
Its command 40h discussion (printed 6-14) describes channel-dependent time
constants; CT1345 mixer controls (printed 4-6 onward) support the analogue
mixing premise. Source: [Creative guide, MIT-hosted copy](https://pdos.csail.mit.edu/6.828/2005/readings/hardware/SoundBlaster.pdf).
This later consolidated guide documents the older card; it does not establish
that this particular software chamber existed in 1994.

The existing implementation approximates nominal stereo rates as 11,111 and
21,739 Hz using integer divisors, and models a separate 45,454 Hz DAC grid.
Those are retained compatibility choices, not new measurements of a CT1600.
Analogue filtering remains an approximation. Direct dry models a mixer/bypass
topology: it is exact host input, with no invented analogue noise model.
On a physical card the dry mix source must be routed accordingly; the plug-in
does not reproduce every physical mixer-source restriction or gain step.

The SB Pro II is half-duplex: a single card does not provide simultaneous ADC
capture and DAC playback for a live software reverb. A plausible DOS workflow
captures/loads material first, processes it, then plays DMA buffers. Analogue
monitoring is separate from CPU processing. Continuous live VST operation uses
the modern host's I/O. A real live 1994 rig would need a separate capture/playback
arrangement; CPU optimization alone cannot resolve that hardware limitation.

## Implemented direction

New instances select direct dry, shared wet, 20 packets, nominal 22.05 kHz
stereo, 12-bit internal arithmetic and no dither. Nominal 11.025 kHz remains
the economy/extreme setting. The 4-64 density mapping is unchanged to protect
automation; 16-24 is the design target rather than a proven deadline budget.
Revised factory packet counts are 20, 24, 16, 24, 8.

Shared wet averages the two captured 8-bit stereo samples into one internal
history. It does not replace the ADC with a higher-bit converter. Opposite-polarity
stereo material can cancel, and one-sided material enters the average at half
amplitude. Deterministic equal-power packet pan gains create stereo output;
Width zero gives equal wet channels (without independent dither), and wider
settings distribute packets. Dry remains stereo. Geometry, signed fragments,
Scatter, finite tails, Body, internal precision and truncation remain controls.
The initial checkpoint's random pan gains biased the dominant decay-weighted
packets to the right. Shared mode now normalizes each channel's weighted packet
energy to half the nominal total at control rate. This retains stereo variation
without making the selected strongest packet determine overall channel level.
It adds two square roots and coefficient scaling per configuration, with no
additional per-sample multiplies. Correlated reflections, fixed-point rounding,
Body and final 8-bit conversion can still produce small measured L/R differences.

Fragment weight, packet envelope and pan coefficients are multiplied together
when configuring read-head sets. Shared mode reads each of three fragment
samples once and uses three precombined Q15 products per output channel.
It removes the separate two packet-gain multiplies and the intermediate
packet quantization/clipping. Thus low-bit behavior intentionally differs from
legacy; combined coefficients are not a bit-exact arithmetic substitution.
Both complete coefficient/position sets crossfade together when parameters move.
The steady kernel has no packet-gain multiply; configuration and modern ramps
still use floating point, including trigonometry outside the reflection loop.

Each channel's sum of absolute fragment weights is bounded by sqrt(3), scaled
by a channel-normalized packet gain no greater than sqrt(0.5). Full-scale
signed history times these Q15 weights stays below about 1.861 billion per
packet (plus tiny coefficient rounding), below INT32_MAX. Quantized packet
values and their complementary crossfade also fit 32 bits; at most 64 such
values are accumulated. Saturation still occurs at PCM output stages.

| Settled reflection kernel, N active packets | Legacy | Shared |
|---|---:|---:|
| History reads/frame | 6N | 3N |
| Sample/coefficient multiplies/frame | 8N | 6N |
| Active history storage | 512 KiB | 256 KiB |
| History writes/frame | 2 | 1 |

These are structural counts for the reflection kernel, excluding zero gains,
filters, output conversion, setup and automation. Geometry/coefficient fades
read both old/new sets and cost more. At 21,739 Hz and 20 active packets,
history reads fall from 2,608,680/s to 1,304,340/s; at 24, from 3,130,416/s to
1,565,208/s. Sparse quantized gains may reduce actual work further.

The VST allocates both 131,072-word rings in prepare (512 KiB total) so loading
old projects or changing modes never allocates on the audio thread. Shared mode
touches only the first ring. `activeBytes()` reports working history;
`bytes()` reports reserved history. A DOS-only shared build could omit the second
ring; this checkpoint does not claim reduced total VST allocation. Switching
wet topology fades down over 25 ms, invalidates the old history/filter state,
then fades up over 25 ms. Existing wet tails are intentionally discarded.

## Compatibility

Parameter IDs 0-20, the normalized Clock/Bits/Density mappings, processor and
controller IDs remain unchanged. IDs 21 (Direct dry) and 22 (Shared history)
are appended. Direct dry ramps over 50 ms; Shared history is a saved setup
choice and is not advertised as automatable. Defensive handling still tolerates
host changes. Both choices are exposed in Options with the established GUI.

Component state version 8 stores 23 values; versions 1-7 retain their existing
range migrations and explicitly select ADC dry / legacy stereo wet. User preset
format 6 stores 23 values; formats 1-5 select legacy topology. New versions are
not loadable by released 1.0.1, so retain copies of older projects when reverting.
No old on-disk preset is rewritten by loading it. New factory selections use the
revised packet counts; saved old parameter states retain their original counts.
Loading an old state after audio has started applies the same existing parameter
ramps plus the topology fade/reset; bit-exact comparison concerns states loaded
before processing, not uninterrupted old-tail migration.

Dry: Direct / Wet: Legacy and Dry: ADC / Wet: Shared are valid combinations.
The legacy reflection arithmetic, clipping stages, channel isolation and read
spacing remain intact. Captured wet data is not migrated between topologies.
Random draws for TPDF are now explicitly sequenced. The right DAC retains the
opposite draw orientation observed in the reference 1.0.1 MSVC binary, which
the previous expression left to compiler operand ordering. The paired binary
comparison checks that this clarification preserves the released output.

## 44.1 kHz mono investigation

Hardware support is real, but adding a third stereo Clock choice would be
misleading and would reinterpret existing normalized automation. A clean future
representation needs an appended converter-mode choice: stereo 11/22 versus
mono high-speed, mono capture before ADC quantization, one wet DAC stream,
and an explicit duplicated-mono wet output policy when dry stays stereo.
The existing divisor convention would yield about 43,478 Hz for nominal 44.1k.

At twice the engine frame rate, preserving the current maximum time span needs
roughly twice the mono history (512 KiB); keeping 256 KiB halves the available
tail span or requires a different multirate engine. The current chamber clamps
its operating rate to 22,050, so merely selecting 44.1k would also give wrong
filter/decay behavior. Mono output forfeits the new stereo packet panning.
Accordingly this checkpoint defers the mode rather than mislabeling a high-rate
stereo path. Hardware support alone does not settle the memory/decay/output policy.

## Validation and remaining 486 gates

The delivered CHECKPOINT.md records fresh results, executable hashes and measured
modern-core comparisons. `bulkhead_direction_tests --benchmark` compares both
topologies in the same compiled engine using identical thirty-second inputs and
seven alternating pairs per workload, pinned to one available logical processor.
`bulkhead_compare old.vst3 new.vst3` explicitly loads released-format v7 states
and automates only the original 21 IDs, checking old-state output compatibility.

A strict 486 claim still requires a DOS integer implementation, compiler and
assembly inspection, measured 486 model/clock/cache/RAM wait states, DMA buffer
deadlines/underruns, IRQ and GUI load, and worst-case coefficient/layout changes.
Real-card filter/converter comparison and listening to representative stereo
material are also open. Modern x64 timing, passing VST3 validation and structural
operation counts establish neither real-card sonic parity nor 486 real-time safety.
