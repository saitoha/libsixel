# Builtin WebP Loader

## Format lineage

Google introduced WebP in 2010 around the VP8 intra-frame codec and later added the independent VP8L lossless codec, alpha, color metadata, and animation in an extended RIFF container. The authoritative container and bitstream references are Google's [WebP container specification](https://developers.google.com/speed/webp/docs/riff_container) and [WebP lossless bitstream specification](https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification).

libsixel once depended on optional libwebp for this family. The dependency-free decoder began with VP8L in commit [`fd73c11b5`](https://github.com/saitoha/libsixel/commit/fd73c11b5) on 2026-04-23, then gained lossy VP8, separate alpha, animation, ICC/Exif/XMP handling, and structural split into [`fromwebp.c`](../../../src/fromwebp.c) plus `fromwebp-*` modules. This is a native implementation, not a private copy of libwebp, and its accepted boundary is deliberately enumerated below.

## RIFF container and structural dialects

Recognition requires `RIFF`, a little-endian RIFF size, and `WEBP` as the form type. Every child chunk has a four-byte FourCC, little-endian payload length, payload bytes, and zero-valued padding when the length is odd. Any RIFF/WEBP-looking input is routed to this family so a malformed WebP fails as WebP rather than being guessed as TGA or another weak-signature format.

| Container form | Required image data | Builtin result |
| --- | --- | --- |
| Simple lossy | One top-level `VP8 ` chunk | Static eight-bit RGBA canvas decoded from a VP8 key frame. |
| Simple lossless | One top-level `VP8L` chunk | Static eight-bit RGBA canvas decoded from VP8L version 0. |
| Extended static | `VP8X`, then a compatible `VP8 ` or `VP8L`, optionally `ALPH` before lossy VP8 | Static image plus the declared alpha/metadata features. |
| Extended animation | `VP8X`, `ANIM`, and one or more `ANMF` chunks | Full-canvas frame sequence with loop, timing, blend, and disposal. |

`ICCP`, `EXIF`, and `XMP ` are recognized as top-level metadata. Unknown chunks are skipped only after length and zero-padding validation. The parser rejects duplicate singleton chunks, conflicting VP8/VP8L payloads, invalid chunk order, feature chunks inconsistent with the `VP8X` flag bitmap, animation data in a static structure, nonzero reserved fields, and canvas/image dimension mismatches.

The builtin hard limits are 32767 per dimension, 268435456 canvas pixels, and 1024 animation frames. These are parser limits, not values controlled by `libwebp:max_output_frames`.

## Lossy VP8 coding

The `VP8 ` path accepts a lossy intra/key-frame payload, not a general VP8 video sequence. VP8 represents image color through luma and chroma planes, normally 4:2:0 sampled, and reconstructs macroblocks using intra prediction, transform coefficients, boolean arithmetic coding, quantization, and loop filtering. The decoder parses the key-frame header, segmentation, filter and quantization state, probability updates, prediction modes, coefficient partitions, inverse transforms, and edge filtering before converting the reconstructed YCbCr planes to RGBA8.

Interframes, reference-frame refresh/copy semantics, temporal prediction, and a standalone WebM/video container are outside this loader. A `VP8 ` payload that is valid only as a non-key video frame is rejected.

## Lossless VP8L coding

`VP8L` version 0 stores an ARGB image through spatial transforms plus entropy coding. The builtin decoder implements predictor, color, subtract-green, and color-index transforms; meta-Huffman image selection; canonical prefix codes; literal/back-reference decoding; two-dimensional distance mapping; and the optional color cache. Transforms are inverted in the required reverse order to produce eight-bit RGBA.

VP8L is lossless with respect to its eight-bit source pixels, but it is not a high-bit-depth format. Invalid transform ordering, table construction, back references, color-cache references, dimensions, or bitstream termination fail rather than being repaired.

## Separate alpha coding

Extended lossy WebP can place an `ALPH` chunk before `VP8 `. Its one-byte header selects compression, filter, and preprocessing:

| Field | Accepted values |
| --- | --- |
| Compression | 0 for raw bytes, or 1 for a VP8L-coded alpha image with implicit canvas dimensions. |
| Filter | 0 none, 1 horizontal, 2 vertical, or 3 gradient; the inverse filter reconstructs alpha. |
| Preprocessing | 0 or 1 are accepted and currently decoded identically. The optional level-reduction reversal associated with preprocessing 1 is not separately implemented. |
| Reserved values | Preprocessing 2/3 and nonzero reserved bits are rejected. |

The reconstructed alpha plane is combined with the lossy RGB result. Alpha does not change the VP8 chroma reconstruction itself; common loader finalization later decides whether the alpha survives as transparency or is composited.

## Animation container and composition

`ANIM` supplies a BGRA background color and loop count. Each `ANMF` supplies an x/y offset in two-pixel units, width and height stored minus one, millisecond duration, a blend flag, a disposal flag, and an embedded VP8/VP8L image fragment. Rectangles are checked against the full canvas.

The decoder maintains one RGBA canvas. A frame whose blend flag selects replace overwrites its rectangle; blend-over alpha-composites it onto the existing canvas. After the complete canvas is emitted, disposal either keeps it or clears the frame rectangle to the `ANIM` background. Full-canvas replacements and known-opaque rectangles can decode directly into the canvas; other frames use bounded scratch/composition paths.

`-T` may skip emission, but frames before the selected frame are decoded when their pixels are required to reconstruct canvas state. The implementation can begin later only when a full-canvas overwrite proves that earlier state is irrelevant. Metadata is cached once and applied consistently to every emitted full-canvas frame. Millisecond duration is converted to the frame timing used by libsixel; the encoder's `-g` policy determines whether it sleeps.

## Metadata and color/orientation precedence

| Metadata | Builtin treatment |
| --- | --- |
| `ICCP` | With CMS enabled, a payload up to 1 MiB is opened as the source ICC profile and applied to RGB channels, producing eight-bit sRGB RGBA. Invalid profiles are ignored. |
| `EXIF` | With orientation enabled, a payload up to 1 MiB is parsed only for TIFF/Exif orientation 2–8 and the complete frame is transformed. |
| `XMP ` color | With CMS enabled, a payload up to 256 KiB is narrowly scanned for aliases of sRGB/IEC 61966-2-1, Display P3, or Adobe RGB (1998), from which a source profile is synthesized. This is not a general XMP or embedded-profile language. |
| `XMP ` orientation | With orientation enabled, the parser accepts supported attribute/element spellings for TIFF orientation and transforms the complete frame. |

ICCP suppresses XMP color fallback when the ICCP chunk is within the parse-size limit, even if that profile later proves invalid or unusable. An over-limit ICCP permits the bounded XMP color fallback. Exif has even stricter precedence for orientation: the presence of an Exif chunk suppresses XMP orientation, including when Exif is empty, malformed, or over the 1 MiB limit. XMP orientation is therefore used only when no Exif chunk is present.

Metadata is container-global. The component does not define frame-local ICC or orientation semantics. Unknown Exif/XMP properties are neither exposed nor preserved.

## Builtin decode and output pipeline

1. The RIFF planner validates the complete chunk graph and chooses static VP8, static VP8+ALPH, static VP8L, or animation.
2. The selected native codec decodes to a gamma `RGBA8888` image or full animation canvas.
3. Container-global ICCP or XMP color interpretation, when applicable, is applied to the color channels and remains an eight-bit RGBA operation.
4. Exif or eligible XMP orientation transforms pixels and alpha together. WebP performs this internally so its trace contract stays with the decoder; common non-WebP orientation code deliberately skips it.
5. Common finalization composites or separates alpha according to background/alpha policy and invokes the frame callback.

The source is always eight-bit. `prefer_8bit=0` does not manufacture high source precision, and this component does not promote static or animation frames to a source `PAL8` representation. Later resize, encoder colorspace conversion, and quantization may convert the returned three color components to float or another working colorspace.

### Implementation and test map

![A vertical implementation map of the builtin WebP loader. It follows RIFF graph parsing and decode-plan construction into the native VP8, VP8L, and separate ALPH codec branches, animated ANMF canvas composition, then bounded ICC, XMP, and Exif processing before typed-frame finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/webp.svg)

The VP8, VP8L, and animation paths are mutually selected by `sixel_webp_build_decode_plan()`. An animated `ANMF` frame may itself use VP8 plus ALPH or VP8L, but it decodes into a rectangle/scratch buffer owned by the animation state before the full canvas is emitted. Metadata remains container-global and is applied to completed frames, not to entropy-code blocks.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `WBP-01` | `sixel_webp_parse_container()` and `sixel_webp_build_decode_plan()` in [`fromwebp-container.c`](../../../src/fromwebp-container.c) | RIFF bytes → validated static/alpha/lossless/animation plan plus metadata references | RIFF size/padding, VP8X flags, unique/conflicting chunks, canvas/frame limits, and nested `ANMF` structure must be proven before codec dispatch. |
| `WBP-02` | `sixel_webp_decode_vp8_payload()` in [`fromwebp-vp8.c`](../../../src/fromwebp-vp8.c) and its native submodules | VP8 key-frame payload → gamma RGBA rectangle | Partition tables, tokens, prediction, inverse transforms, loop filters, and YUV upsampling must reproduce an intra-frame without libwebp. |
| `WBP-03` | `sixel_webp_decode_stream()` and `sixel_webp_decode_vp8l_payload()` in [`fromwebp-vp8l.c`](../../../src/fromwebp-vp8l.c) | VP8L bitstream → gamma ARGB/RGBA pixels | Huffman groups, cache/back references, nested transforms, and reverse predictor/color/green/index operations must obey dependency order and bounds. |
| `WBP-04` | `sixel_webp_apply_vp8_alpha_payload()` and `sixel_webp_vp8_alpha_reconstruct()` in [`fromwebp-vp8-alpha.c`](../../../src/fromwebp-vp8-alpha.c) | ALPH chunk → alpha bytes attached to VP8 RGBA | Raw or VP8L-compressed alpha must reverse its declared filter and must not disturb decoded RGB channels. |
| `WBP-05` | `sixel_fromwebp_load_animation()`, `sixel_webp_anim_composite_rect()`, and `sixel_webp_anim_clear_rect()` in [`fromwebp.c`](../../../src/fromwebp.c) | ANIM/ANMF plan plus decoded rectangles → emitted full-canvas frames | Rectangle offsets, blend/replace, dispose, background clear, timing, loop/start-frame pre-roll, and ownership-transfer fast paths must agree. |
| `WBP-06` | `sixel_webp_apply_iccp_to_srgb_rgba()`, `sixel_webp_try_apply_exif_orientation()`, and `sixel_fromwebp_load()` in [`fromwebp.c`](../../../src/fromwebp.c) | Completed RGBA frame plus bounded metadata → oriented sRGB RGBA frame | ICCP/XMP and Exif/XMP precedence, size caps, best-effort failures, and geometry-plus-alpha transforms must remain container-global and deterministic. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin:cms_engine=auto:orientation=1! animation.webp
img2sixel -Lbuiltin! -T 4 -l disable animation.webp
```

| Control | Exact effect on builtin WebP |
| --- | --- |
| `builtin:orientation=0|1` (`O0`/`O1`) | Default `1` enables Exif-or-eligible-XMP orientation. `0` parses the container but does not transform geometry. Exif chunk presence still defines metadata precedence. |
| `cms_engine=none|auto|builtin|lcms2|colorsync` | `none` disables ICCP/XMP color conversion. Other values select an available engine; source decoding remains RGBA8. |
| `cms_target`, `cms_intent`, `prefer_8bit` | WebP metadata is normalized to eight-bit sRGB within this component; these controls do not promote it to a typed float source frame. They can still affect shared CMS engine selection/intent behavior where implemented. |
| `-S`, `--static` | Emits only the frame selected by `-T` and stops. Required pre-roll may still decode. Static WebP is unchanged. |
| `-T N`, `--start-frame=N` | Selects a zero-based animation source frame on the first loop; negative values count from the end. Out-of-range values fail. It has no effect on static WebP. |
| `-l auto|force|disable` | `disable` stops after one pass. `auto` honors a positive `ANIM` loop count and treats zero as indefinite. `force` ignores a finite count and repeats until cancellation. A one-frame animation is not replayed. |
| `-g`, `--ignore-delay` | Does not change parsed millisecond durations or disposal; it prevents encoder presentation sleeps. |
| `-B`, `background_colorspace`, `-A auto|composite|clear|keep` | Determine how decoded alpha is flattened or retained. The `ANIM` background remains the animation canvas-clear color; an explicit output background is a later finalization concern. |
| `background_policy` | WebP has no file-background source participating in the shared PNG/GIF priority policy, so changing it does not choose against `ANIM` background. |
| `libwebp:max_output_frames=COUNT` | No effect on `builtin`. It belongs only to the optional `libwebp` component. Builtin retains its fixed 1024-frame structural limit. |
| `-p COLORS`, resize, crop, sampling, and encoder color modes | Run after each decoded frame and do not alter VP8/VP8L parsing. |
| `trns_keycolor`, HDR, PNM, BMP, and PSD-specific suboptions | No effect. |

## Unsupported behavior and security boundary

VP8 interframes, WebM, future VP8L versions, unspecified `ALPH` preprocessing reversal, arbitrary XMP, arbitrary XMP-described ICC profiles, per-frame metadata, unknown-chunk preservation, dimensions above 32767, canvases above 268435456 pixels, and animations above 1024 frames are unsupported.

RIFF sizes, nested `ANMF` fragments, entropy tables, transform graphs, back references, macroblock partitions, canvas rectangles, and loop counts are attacker-controlled. The builtin implementation removes a library dependency but adds a substantial codec and animation attack surface to libsixel. `-Lbuiltin!` makes rejection deterministic; it does not sandbox decode or limit the CPU work of `-l force`.

## Implementation landmarks

- Top-level planner, metadata, animation, and frame emission: [`fromwebp.c`](../../../src/fromwebp.c)
- RIFF graph and limits: [`fromwebp-container.c`](../../../src/fromwebp-container.c)
- Lossy codec: [`fromwebp-vp8.c`](../../../src/fromwebp-vp8.c) and related `fromwebp-vp8-*` modules
- Lossless codec: [`fromwebp-vp8l.c`](../../../src/fromwebp-vp8l.c) and related `fromwebp-vp8l-*` modules
- Alpha codec: [`fromwebp-vp8-alpha.c`](../../../src/fromwebp-vp8-alpha.c)
- Common alpha/background finalization: [`loader-builtin.c`](../../../src/loader-builtin.c), [Alpha Policy](../alpha-policy.md), and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| WBP-02 | The native VP8 key-frame path retains an MS-SSIM quality floor against libwebp. | [tests/loader/builtin/1647_loader_builtin_webp_vp8_static_quality_snake64_msssim.t](../../../tests/loader/builtin/1647_loader_builtin_webp_vp8_static_quality_snake64_msssim.t) |
| WBP-03 | VP8L color-index transform subsampling is reversed with the required quality. | [tests/loader/builtin/1931_loader_builtin_webp_vp8l_transform_subsample_quality_msssim.t](../../../tests/loader/builtin/1931_loader_builtin_webp_vp8l_transform_subsample_quality_msssim.t) |
| WBP-04 | A static VP8+ALPH image decodes RGB and its separate alpha plane with the required quality. | [tests/loader/builtin/1669_loader_builtin_webp_vp8_alpha_static_quality_msssim.t](../../../tests/loader/builtin/1669_loader_builtin_webp_vp8_alpha_static_quality_msssim.t) |
| WBP-05 | Animated lossy frames with separate alpha decode, composite, and emit with the required quality. | [tests/loader/builtin/1725_loader_builtin_webp_animation_lossy_alpha_quality_msssim.t](../../../tests/loader/builtin/1725_loader_builtin_webp_animation_lossy_alpha_quality_msssim.t) |
| WBP-06 | XMP orientation and color metadata coexist and are applied in their documented roles. | [tests/loader/builtin/1847_loader_builtin_webp_static_xmp_orientation_and_cms_coexist_code.t](../../../tests/loader/builtin/1847_loader_builtin_webp_static_xmp_orientation_and_cms_coexist_code.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| WBP-01 | A chunk whose payload exceeds the enclosing RIFF size is rejected during container-plan construction. | [tests/loader/builtin/1620_loader_builtin_webp_bad_chunk_payload_exceeds_riff_code.t](../../../tests/loader/builtin/1620_loader_builtin_webp_bad_chunk_payload_exceeds_riff_code.t) |
| WBP-90 | A frame containing duplicate ALPH chunks is rejected before animation composition. | [tests/loader/builtin/1716_loader_builtin_webp_bad_anim_anmf_duplicate_alpha_chunk_code.t](../../../tests/loader/builtin/1716_loader_builtin_webp_bad_anim_anmf_duplicate_alpha_chunk_code.t) |

Coverage audit note: the owners cover each top-level codec family, separate alpha, animated composition, metadata coexistence, and two container failures. They do not exhaust every VP8 macroblock/filter/partition mode, VP8L Huffman/transform nesting, ALPH filter, animation blend/dispose permutation, or metadata precedence/size-limit combination.
