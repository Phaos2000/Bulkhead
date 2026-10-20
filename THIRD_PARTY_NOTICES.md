# Bulkhead third-party notices

This file records known third-party materials and unresolved provenance.
It is not a declaration that every bundled asset is cleared for redistribution.
The Bulkhead license applies only to material whose rights Phaos controls.
Adding a license or a credit does not establish rights to third-party material.
Older cursor authorship claims in documentation are superseded by this file.

## Steinberg VST3 SDK

Location: vendor/vst3sdk/
The local SDK identifies as VST 3.8.1 and carries:
Copyright (c) 2026 Steinberg Media Technologies GmbH.
The root, base, cmake, pluginterfaces, and public.sdk licenses are MIT.
The full root notice is reproduced in LICENSES/Steinberg-VST3-SDK-MIT.txt.
Existing release archives also contain LICENSE-VST3-SDK.txt; retain it.

VST is a registered trademark of Steinberg Media Technologies GmbH.
SDK licensing does not grant ownership of Steinberg names or logos.
See https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html
and the linked trademark usage guidelines.

The complete SDK tree includes separately licensed examples and libraries:
- public.sdk/samples/vst/mda-vst3/: Paul Kellett MIT notices in source headers.
- public.sdk/samples/vst/dataexchange/source/3rdparty/nanovg/: zlib-style
  NanoVG/fontstash notices; stb_truetype.h includes MIT/public-domain alternatives.
- public.sdk/samples/vst/dataexchange/source/3rdparty/MetalNanoVG/: MIT,
  Copyright (c) 2017 Ollix.
- public.sdk/source/vst/moduleinfo/json.h: public-domain/Unlicense terms in header.
- Some SDK templates and examples retain BSD-style notices in their headers.

These examples are not compiled into the Bulkhead plugin by its current build.
Preserve their own notices if redistributing their source. The Hoehrmann UTF-8
decoder in nanovg/src/fontstash.h has an attribution but lacks its full original
MIT permission text in the local copy. Include that text before redistributing
this optional dependency, or omit the unused example dependency.
Original terms: https://bjoern.hoehrmann.de/utf-8/decoder/dfa/

## ANTORB1 title artwork - redistribution rights unverified

Location: source/TitleFont.h, rendered by source/Draw.h. The original atlas is not included in this source repository.
Credited in source as Antor / Ghostbusters; obtained from:
https://github.com/ianhan/BitmapFonts

The collection provides no separate license for this sheet and its maintainer
does not claim rights to the collected artwork. Phaos has chosen to retain the
title while accepting this unresolved permission risk. This records that choice;
it does not grant permission, claim ownership, or place the artwork under the
Bulkhead license.

Other font sheets, assets/phaos.png, and artifacts/BitmapFonts-main.zip have
no established redistribution permission in this project. Exclude unneeded
reference assets from a public repository unless their rights are established.

## Cursor artwork - redistribution rights unverified

Location: source/CursorSprites.h. Downloaded cursor reference images are not included in this source repository.
The active pointer is arrowCursorSprite, recoloured by the plugin into four
styles. Phaos reports its origin as a freely downloadable archival copy of
cursor artwork from a 1993 Amiga CD; the CD, author, archive, and original
terms have not been identified.

No original authorship by Phaos or redistribution license is established.
Free availability and a publisher's dissolution do not establish permission.
The artwork is excluded from the grant of the Bulkhead license. Unused cursor
glyphs were removed from this source snapshot; the active glyph and all four
colour choices are preserved. Its redistribution rights remain unverified.

The provenance of the floppy pixels in source/PresetFloppy.h also needs
confirmation. Do not infer a third-party license or Phaos ownership from this file.

## PCG hash and small bitmap tables - provenance to establish

source/Engine.h (Chamber::random) and tests/dsp_tests.cpp (excite) use the
recognizable PCG RXS-M-XS hash sequence with constants 747796405, 2891336453,
and 277803737. The actual source used for this implementation is undocumented.
A matching algorithm is described at:
https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/

An algorithm match alone does not identify the source or establish copying.
Record actual provenance and applicable notices before calling this cleared.
The origin of the small bitmap glyph tables in source/Draw.h likewise needs
confirmation; microGlyph is described in source as hand-drawn.

## Hardware references and system libraries

source/SbPro2Dac.h describes an original audio-path model informed by DOSBox
Staging behaviour; source/SbPro2Adc.h references Creative hardware information.
No DOSBox implementation is included in the current Bulkhead build.
Separate downloaded DOSBox research files carry GPL-2.0-or-later and are not
covered by the Bulkhead license. Do not include those research copies in a
release without meeting their separate license requirements.

References:
https://github.com/dosbox-staging/dosbox-staging
https://ardent-tool.com/sound/Sound_Blaster_HW_Programming_Guide_1st.pdf

The current Windows build links the Microsoft runtime statically and uses
Windows system libraries. Do not redistribute compiler tools or Windows DLLs
as project source/assets. Microsoft runtime redistribution remains subject to
the applicable Microsoft terms.

## Presets and diagnostic audio

Factory parameter presets are defined in source/Parameters.h.
tests/dsp_tests.cpp synthesizes the diagnostic excitation and renders audio.
No external sample, wavetable, or impulse-response dependency was identified
in the current Bulkhead audio path.

## Release status

The documents added in this licensing pass do not clear the unresolved font,
cursor, code, or other asset provenance above. Source/build 1.0.0 is based on the 0.5.7 audio engine and is separate
from the historical 0.2.0-0.5.5 ZIPs. Their licensing documents were updated
without changing plugin binaries or asserting that those releases are cleared.
