# Builtin Softimage PIC Loader

## Format lineage

Softimage PIC is the raster format associated with early Softimage 3D systems. It is unrelated to Radiance files that also commonly use the `.pic` suffix. Historical descriptions such as Paul Bourke's [Softimage image format](https://paulbourke.net/dataformats/softimagepic/index.html) document the characteristic magic, fixed header, channel packets, and scanline RLE, but the libsixel contract is the implemented subset below.

PIC remains in the adapted stb-derived core imported by commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd). libsixel's wrapper requests a four-component intermediate so alpha packets can reach common finalization. Dedicated regression coverage was added in commit [`813c8e08e`](https://github.com/saitoha/libsixel/commit/813c8e08e) in 2026. Unlike PSD, HDR, BMP, or WebP, PIC has not been split into a separately maintained format module.

## Header and recognition

Recognition requires both the four-byte big-endian magic `53 80 f6 34` and ASCII `PICT` at byte offset 88. This distinguishes Softimage PIC from Radiance `#?RADIANCE`/`#?RGBE` despite their suffix collision.

The parser consumes this fixed header shape:

| Field | Builtin treatment |
| --- | --- |
| Magic and version | Magic is validated by the predicate. The four-byte version value is skipped and not exposed. |
| 80-byte comment/name area | Skipped; text is not decoded or preserved. |
| `PICT` marker | Required at offset 88. |
| Width and height | Unsigned 16-bit big-endian values; used as the raster dimensions and checked against allocation/dimension limits. |
| Pixel ratio | Four bytes skipped; no aspect-ratio resampling. |
| Fields | Two bytes skipped; no field/interlace reconstruction. |
| Padding | Two bytes skipped. |

There is no palette, ICC container, Exif orientation, frame directory, or high-bit-depth declaration in the supported boundary.

## Channel packets and color representation

After the fixed header comes a chain of one through ten four-byte packet descriptors. The first byte indicates whether another descriptor follows. The remaining bytes select sample size, compression type, and a channel mask.

| Channel bit | Component written into the intermediate canvas |
| --- | --- |
| `0x80` | Red |
| `0x40` | Green |
| `0x20` | Blue |
| `0x10` | Alpha |

Every accepted packet declares 8-bit samples. A packet can carry several selected components per pixel, and the same component may be written by more than one packet. The decoder initializes the full RGBA canvas to `0xff`, so an omitted color component remains white and omitted alpha remains opaque. The aggregate packet mask reports RGB unless any packet selects alpha, but the libsixel wrapper explicitly requests RGBA in either case.

PIC bytes are treated as gamma-encoded RGB without an interpreted embedded profile. The format can carry alpha as an independent sample, but the stable loader result is normalized color plus transparency state rather than a promise of permanently interleaved RGBA.

## Compression types

Packet descriptors select one compression algorithm, applied independently to each scanline and to the packet's selected component tuple:

| Type | Coding accepted by the stb-derived decoder |
| --- | --- |
| 0 | Uncompressed: one selected-component tuple for every pixel in the row. |
| 1 | Pure RLE: an eight-bit count followed by one tuple, repeated until the row width is filled. A run that exceeds the remaining row is clipped by this legacy path rather than rejected. |
| 2 | Mixed RLE: command 0–127 means 1–128 literal tuples; 129–255 means a repeated tuple with length `command - 127`; command 128 takes a following 16-bit big-endian repeated length. Runs that exceed the row are rejected. |

Packet payloads are stored row by row, with every packet decoded for a row before the decoder advances to the next row. There is no palette lookup, planar postprocessing, or vertical-origin flag.

## Builtin decode and output pipeline

1. Strong magic and `PICT` marker route the input to the residual PIC parser before weak-signature TGA probing.
2. The fixed header establishes width/height and skips non-rendered metadata.
3. Up to ten packet descriptors are validated; a white, opaque RGBA canvas is allocated.
4. Each row's raw/pure-RLE/mixed-RLE packet payload writes its selected channels into that canvas.
5. The wrapper requests `RGBA8888`, then common finalization composites alpha against a background or produces the frame transparency representation.
6. One gamma eight-bit frame is delivered.

No CMS or orientation stage occurs inside PIC. Later sampling, resize, crop, encoder working-colorspace conversion, palette initialization, quantization, lookup, and dithering are separate encoder stages.

### Implementation and test map

![A vertical implementation map of the builtin Softimage PIC loader. It follows magic and PICT recognition into fixed-header and channel-packet parsing, raw or pure and mixed RLE scanline decoding, and common RGBA alpha finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/pic.svg)

PIC remains inside the adapted stb source, so several conceptual stages share `stbi__pic_load_core()`. They are split in the map because their invariants and tests differ: packet-table validation determines channel ownership, while each scanline compression mode determines how many component tuples are produced.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `PIC-01` | `stbi__pic_test()` and `stbi__pic_test_core()` in [`stb_image.h`](../../../src/stb_image.h) | Residual bytes → recognized Softimage PIC | Both the big-endian magic and `PICT` marker at byte 88 are required; the `.pic` suffix alone must not collide with Radiance HDR. |
| `PIC-02` | `stbi__pic_load()` and header/packet setup in `stbi__pic_load_core()` in [`stb_image.h`](../../../src/stb_image.h) | Fixed header plus descriptor chain → dimensions and selected-channel packet plan | One-to-ten descriptors, 8-bit sample size, compression 0–2, channel masks, and continuation termination must be validated before raster writes. |
| `PIC-03` | Raw-packet branch in `stbi__pic_load_core()` in [`stb_image.h`](../../../src/stb_image.h) | Uncompressed per-row tuples → white-initialized RGBA canvas | Every selected component must land in its RGBA lane while omitted channels retain the documented white/opaque initialization. |
| `PIC-04` | Pure- and mixed-RLE branches in `stbi__pic_load_core()` in [`stb_image.h`](../../../src/stb_image.h) | Run/literal commands → selected-component tuples | Byte and extended counts, literal payload size, clipping compatibility for pure RLE, and strict mixed-run row bounds must remain distinct. |
| `PIC-05` | `sixel_builtin_apply_pic_alpha_policy()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Requested `RGBA8888` canvas → gamma RGB plus mask or composite | Packet alpha must survive the wrapper request and reach common finalization; opaque files must not acquire synthetic transparency. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! image.pic
img2sixel -Lbuiltin! -A keep -B '#ffffff' alpha.pic
```

| Control | Exact effect on Softimage PIC |
| --- | --- |
| `-A auto|composite|clear|keep` | Controls final handling of a decoded alpha channel. It has no visible effect when all alpha samples are opaque. |
| `-B COLOR`, `background_colorspace` | Supplies the color/transfer interpretation used when PIC alpha is composited. |
| `background_policy` | PIC has no interpreted file background, so it does not choose between file and explicit sources. |
| `-p COLORS`, resize, crop, sampling, and encoder color modes | Operate after PIC has already expanded to RGB plus transparency. There is no palette-fusion path. |
| `cms_engine`, `cms_target`, `cms_intent`, `prefer_8bit` | No effect on PIC decode because no source profile is interpreted. The source is always eight-bit. |
| `builtin:orientation` | No effect; PIC has no interpreted orientation metadata. |
| `-S`, `-T`, `-l`, `-g`, `trns_keycolor`, HDR, PNM, BMP, and PSD-specific suboptions | No effect. PIC emits one frame. |

## Unsupported behavior and security boundary

Sample sizes other than eight bits, compression types outside 0–2, more than ten packet descriptors, high-depth output, palette data, field/interlace reconstruction, pixel-aspect correction, embedded color profiles, orientation metadata, animation, and unknown-metadata preservation are unsupported. The version field is not used to select alternate dialect behavior.

Dimensions, packet masks, chain length, run counts, and source availability are attacker-controlled. The legacy decoder's permissive clipping of oversized pure-RLE runs is part of its observable compatibility, not a general repair promise. `-Lbuiltin!` limits fallback but does not sandbox this parser.

## Implementation landmarks

- Predicate, fixed header, packet chain, and RLE: [`stb_image.h`](../../../src/stb_image.h)
- PIC selection, RGBA request, alpha/background finalization, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha/background contract: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [Softimage PIC coverage inventory](../../testing/builtin-loader-coverage.md#softimage-pic) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PIC-02 | A chained packet table combines channel selections in descriptor order. | [tests/loader/builtin/0694_loader_builtin_pic_chained_packets_decode.t](../../../tests/loader/builtin/0694_loader_builtin_pic_chained_packets_decode.t) |
| PIC-03 | Raw RGB packet data decodes into the expected component lanes. | [tests/loader/builtin/0689_loader_builtin_pic_raw_rgb_decode.t](../../../tests/loader/builtin/0689_loader_builtin_pic_raw_rgb_decode.t) |
| PIC-04 | Mixed RLE accepts the extended repeat-count form and reconstructs the row. | [tests/loader/builtin/0693_loader_builtin_pic_rle_mixed_ext_count_decode.t](../../../tests/loader/builtin/0693_loader_builtin_pic_rle_mixed_ext_count_decode.t) |
| PIC-05 | A PIC alpha channel reaches explicit-background composition with the expected numeric result. | [tests/loader/builtin/0708_loader_builtin_pic_rgba_composite_numeric.t](../../../tests/loader/builtin/0708_loader_builtin_pic_rgba_composite_numeric.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PIC-01 | Missing `PICT` at the fixed offset prevents recognition even when other header bytes are present. | [tests/loader/builtin/0696_loader_builtin_pic_missing_pict_signature_reject.t](../../../tests/loader/builtin/0696_loader_builtin_pic_missing_pict_signature_reject.t) |
| PIC-90 | A mixed-RLE run that exceeds the scanline is rejected instead of writing past the row. | [tests/loader/builtin/0702_loader_builtin_pic_rle_mixed_scanline_overrun_reject.t](../../../tests/loader/builtin/0702_loader_builtin_pic_rle_mixed_scanline_overrun_reject.t) |

Coverage audit note: the owners distinguish recognition, packet chaining, raw decode, extended mixed RLE, alpha composition, and mixed-run overrun. Pure-RLE oversized-run clipping compatibility and every multi-channel packet-mask overlap are not isolated by reciprocal owners here.
