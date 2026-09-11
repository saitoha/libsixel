# OR-mode output

`img2sixel -O` (or `--ormode`) encodes palette indices as overlapping bit planes. The receiver combines the selected index bits with bitwise OR, then looks up the resulting index in the palette. This is a SIXEL dialect signaled by `P2=5`, so ordinary SIXEL support alone does not establish compatibility.

The [img2sixel manual](../../converters/img2sixel.1) attributes the dialect to the NetBSD/x68k `ite` console and the `sayaka` client. This document describes the current libsixel implementation; it does not certify current versions of those or other receivers.

## Usage and API

OR mode is disabled by default. Enable it explicitly when producing a stream for a receiver that implements this dialect:

```console
img2sixel -O -p 16 -o image-or.six image.png
sixel2png -i image-or.six -o image-or.png
```

The second command checks the decoded image with libsixel; it does not test the terminal. Palette construction, palette application, and dithering still precede OR-mode serialization. `-p 16` is only an example palette budget, not a requirement of the mode.

For the high-level C encoder, use `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_ORMODE, NULL)`. For the low-level output context, use `sixel_output_set_ormode(output, 1)` before encoding a palette image; passing zero disables the flag. These interfaces are declared in the [public header template](../../include/sixel.h.in).

## Index composition

Normal SIXEL painting replaces a pixel's color with the selected register. OR-mode painting accumulates bits in its palette index:

```text
normal paint: index = selected_register
OR paint:     index = index | selected_register
final color:  palette[index]
```

The operation applies to integer palette indices, not RGB components. For example, painting register 1 and then register 2 onto the same pixel yields index 3. If palette entry 3 is blue, the result is blue even when entries 1 and 2 are red and green.

| Desired index | Binary index | Encoder plane selectors |
| --- | --- | --- |
| 0 | 000 | No set bits |
| 1 | 001 | `#1` |
| 3 | 011 | `#1`, `#2` |
| 5 | 101 | `#1`, `#4` |
| 7 | 111 | `#1`, `#2`, `#4` |

Repeated painting with the same selector is idempotent. Painting another selector can add bits but cannot remove them. Consequently, ordinary size-policy solid underpainting followed by color repair cannot be used: the repair would need to clear already accumulated bits. See [encoding policy](encode-policy.md).

## Wire representation

The encoder header uses `P1=7` and `P2=5`. The decoder recognizes OR mode from `P2=5`; it does not use `P1=7` as the mode discriminator. DCS framing, six vertical bits per data character, repeat runs, `$` carriage returns, and `-` band advances retain their structural roles described in the [SIXEL format guide](../sixel-format.md).

The following escaped stream defines a one-pixel raster, defines four palette entries, and paints its top pixel twice. `ESC` here denotes byte `0x1b`, not literal letters:

```text
ESC P 7;5 q
"1;1;1;1
#0;2;0;0;0 #1;2;100;0;0 #2;2;0;100;0 #3;2;0;0;100
#1 @ $ #2 @
ESC \
```

Whitespace and line breaks are shown only for readability. `@` has SIXEL mask value 1, and `$` returns to the same column, so the two paints produce index 3. A receiver that simply overwrites would end at index 2 instead.

The encoder emits palette definitions and then visits the palette-index bit planes for each six-row band. It uses power-of-two selectors such as `#1`, `#2`, and `#4`. The number of candidate planes is the ceiling of the base-two logarithm of the palette size, with at least one plane and at most eight in this encoder. Thus a 256-color palette needs eight candidate planes, not 256 per-color masks. Final bands shorter than six rows use only the available rows. `-E size` can omit entirely empty planes while preserving horizontal positions with leading zero runs.

## Palette, transparency, and other options

| Interaction | Current behavior and boundary |
| --- | --- |
| Fixed palettes | Builtin palettes and mapfiles still supply the final palette. `-X` does not rebuild a fixed palette merely because `-O` is active. |
| `-E size` | Optimizes OR-plane serialization, including empty-plane omission; it does not enable ordinary overwrite-based repair. |
| `--threads` | OR mode has a producer/writer pipeline with ordered band output. The serial body and pipeline have a byte-equivalence regression fixture. Scheduling and fallback details are in the [encoder threading guide](../threading/encoder.md). |
| Transparent offset (`-+`) | The encoder expands the geometry and emits zero-bit padding. This alone does not guarantee that a receiver preserves the existing terminal image in that margin. |
| Alpha and index zero | libsixel decoding initializes the OR index raster to zero. Every cell in the decoded raster, including an untouched cell, has a palette index and is treated as painted. Direct RGBA decoding resolves entry zero to an opaque color. |
| High color (`-I`) | High-color dispatch precedes the ordinary indexed encoder. It is a separate repeated-register technique, not an additional OR bit plane or a way to expand this eight-plane encoder. Combining `-I` with `-O` is not covered here as a supported composition. See [high-color output](high-color.md). |

In particular, do not infer normal `P2=1` transparency from a zero SIXEL mask in a `P2=5` stream. The libsixel image decoder's opaque index-zero behavior and a terminal's existing framebuffer behavior are different observation boundaries. The offset regression checks geometry and padding bytes, not visual transparency on hardware. Resolve source transparency to the intended background before encoding when an opaque standalone image is required; see [alpha policy](../loader/alpha-policy.md) and [background policy](../loader/background-policy.md).

## Quality, size, and performance

OR decomposition does not itself choose different colors: for a fixed palette and valid index raster, recombining its bits recovers those indices. Input quantization, dithering, palette serialization, and receiver behavior remain relevant to the final image. This is not a claim that two complete CLI runs with different modes or scheduling necessarily have identical intermediate rasters.

Fewer candidate planes can reduce output size, especially when many palette colors occur in a band. Actual byte size also depends on index assignment, spatial patterns, repeat-run compression, and empty planes. A normal encoder can be smaller on images with simple per-color runs; eight planes are not a promise of an eightfold size or speed improvement.

Compare stream byte counts and decoded images using the same source, loader, palette, dithering, geometry, encode policy, and thread budget. Distinguish complete conversion time from body serialization and terminal rendering. This document supplies no benchmark or terminal compatibility measurement; follow the [quality measurement policy](../quality/measurement-policy.md) when publishing such comparisons.

## Implementation references

- [Encoder core](../../src/encoder-core-encode.c): `sixel_encode_header`, `sixel_encode_body_ormode_nplanes`, `sixel_encode_body_ormode_band`, offset handling, and pipelined serialization.
- [Decoder parser](../../src/fromsixel.c): `parser_context_is_ormode_request`, index OR accumulation, index-zero initialization, direct-color finalization, and paint-mask completion.
- [Parallel decoder](../../src/decoder-parallel.c): OR accumulation in worker spans and direct-color index storage.
- [Output API](../../src/output.c) and [high-level encoder](../../src/encoder.c): mode configuration and default state.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| OR-01 | CLI conversion accepts `-O`. | [tests/cli/options/general/0002_ormode_option_runs.t](../../tests/cli/options/general/0002_ormode_option_runs.t) |
| OR-02 | Full six-row bands serialize the expected plane bytes. | [tests/processing/encoder-core/0002_encoder_core_ormode_body_full_band.t](../../tests/processing/encoder-core/0002_encoder_core_ormode_body_full_band.t) |
| OR-03 | A short final band serializes only its available rows. | [tests/processing/encoder-core/0003_encoder_core_ormode_body_tail_band.t](../../tests/processing/encoder-core/0003_encoder_core_ormode_body_tail_band.t) |
| OR-04 | The OR producer/writer pipeline matches the serial body for its fixture. | [tests/processing/encoder-core/0005_encoder_core_ormode_pipeline_matches_body.t](../../tests/processing/encoder-core/0005_encoder_core_ormode_pipeline_matches_body.t) |
| OR-05 | Size policy omits an empty plane in the regression fixture. | [tests/processing/encoder-core/0006_encoder_core_ormode_body_skips_empty_planes.t](../../tests/processing/encoder-core/0006_encoder_core_ormode_body_skips_empty_planes.t) |
| OR-06 | Raw decoding composes selectors 1 and 2 into index 3. | [tests/processing/decoder/0002_decoder_ormode_raw_overlay.t](../../tests/processing/decoder/0002_decoder_ormode_raw_overlay.t) |
| OR-07 | Wide-index decoding preserves OR composition. | [tests/processing/decoder/0003_decoder_ormode_wide_overlay.t](../../tests/processing/decoder/0003_decoder_ormode_wide_overlay.t) |
| OR-08 | Direct decoding resolves the composed index and opaque entry zero. | [tests/processing/decoder/0004_decoder_ormode_direct_overlay.t](../../tests/processing/decoder/0004_decoder_ormode_direct_overlay.t) |
| OR-09 | Repeat runs preserve OR composition. | [tests/processing/decoder/0005_decoder_ormode_repeat_overlay.t](../../tests/processing/decoder/0005_decoder_ormode_repeat_overlay.t) |
| OR-10 | The legacy decode API follows OR composition. | [tests/processing/decoder/0006_decoder_ormode_legacy_api.t](../../tests/processing/decoder/0006_decoder_ormode_legacy_api.t) |
| OR-11 | The parallel request path accumulates OR indices. | [tests/processing/decoder/0007_decoder_ormode_parallel_request.t](../../tests/processing/decoder/0007_decoder_ormode_parallel_request.t) |
| OR-12 | Dequantization retains opaque OR raster cells. | [tests/processing/decoder/0022_decoder_ormode_dequantize_opaque.t](../../tests/processing/decoder/0022_decoder_ormode_dequantize_opaque.t) |
| OR-13 | Builtin palette OR output ignores clustering-space selection. | [tests/quant/palette/usage/0137_builtin_palette_ormode_clustering_colorspace_ignored_output.t](../../tests/quant/palette/usage/0137_builtin_palette_ormode_clustering_colorspace_ignored_output.t) |
| OR-14 | PAL mapfile OR output ignores clustering-space selection. | [tests/quant/palette/usage/0138_mapfile_pal_ormode_clustering_colorspace_ignored_output.t](../../tests/quant/palette/usage/0138_mapfile_pal_ormode_clustering_colorspace_ignored_output.t) |
| OR-15 | Transparent-offset OR output carries expanded geometry and left padding. | [tests/quant/palette/usage/0174_transparent_offset_ormode_geometry.t](../../tests/quant/palette/usage/0174_transparent_offset_ormode_geometry.t) |

### Defensive and malformed-input tests

The [encoder-core test category](../../tests/processing/encoder-core/) also exercises invalid arguments to the private OR body helper. These checks defend an implementation boundary and do not establish additional terminal behavior. General malformed-stream coverage belongs to the [decoder tests](../../tests/processing/decoder/) and [security tests](../../tests/security/).

### Coverage limits

The CLI test is a conversion smoke test, not an exhaustive default, long-option, repeated-option, or multi-input state test. The table does not establish every palette size, every alpha-policy combination, `-I`/`-O` composition, equivalence under every thread schedule, or terminal display compatibility. Receiver handling of `P2=5`, existing framebuffer content, and offset margins requires a separate receiver-specific observation.
