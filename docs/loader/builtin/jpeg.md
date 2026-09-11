# Builtin JPEG Loader

## Format lineage

JPEG names both a family of continuous-tone compression processes and the marker-coded interchange syntax standardized as ISO/IEC 10918-1 and [ITU-T T.81](https://www.itu.int/rec/T-REC-T.81). JFIF later standardized common file interchange conventions around that syntax; Exif and Adobe APP markers introduced other widely deployed metadata and component conventions. JPEG 2000, JPEG-LS, and JPEG XL are independent formats despite sharing the name.

The builtin decoder entered libsixel with stb_image 1.33 in commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd) in 2014. It remains an adapted stb-derived core in [`stb_image.h`](../../../src/stb_image.h), but it is no longer stock behavior. Commit [`158bb61ea`](https://github.com/saitoha/libsixel/commit/158bb61ea) added a float output path in 2026. Commits [`cb7fb59bb`](https://github.com/saitoha/libsixel/commit/cb7fb59bb) and [`81184726a`](https://github.com/saitoha/libsixel/commit/81184726a) added and hardened high-depth lossless JPEG. Metadata extraction and frame routing live in [`loader-builtin.c`](../../../src/loader-builtin.c).

## Marker container and coding processes

A JPEG interchange stream begins with SOI and is a sequence of markers and entropy-coded scan data ending at EOI. SOF defines dimensions, components, precision, and sampling; DQT and DHT define quantization and Huffman tables; DRI establishes restart intervals; SOS begins a scan. APPn and COM segments carry conventions or metadata outside the coded sample process.

| SOF marker | Process | Builtin precision |
| --- | --- | --- |
| `SOF0` | Baseline sequential DCT | 8 bits |
| `SOF1` | Extended sequential DCT | 8 through 12 bits |
| `SOF2` | Progressive DCT | 8 through 12 bits |
| `SOF3` | Huffman lossless predictive | 2 through 16 bits |

Lossy modes level-shift samples, divide the image into component blocks, apply an 8×8 DCT and quantization, reorder coefficients, and Huffman-code DC differences and AC runs. Progressive scans deliver coefficient bands or refinements over several scans. Lossless JPEG does not use DCT or quantization: it predicts each sample from neighboring samples with predictor 1 through 7 and Huffman-codes the difference, with a point transform below the sample precision.

The decoder accepts 8- and 16-bit quantization tables, up to four Huffman tables of each class, restart markers, and multiple scans. Horizontal and vertical sampling factors are 1 through 4 and must divide the maximum factor so component upsampling has an integral ratio. SOF dimensions must be known; a zero-height SOF/delayed-height stream is rejected, and DNL is accepted only when it repeats the already established height.

## Component and color interpretation

One-, three-, and four-component frames are accepted. One component becomes grayscale RGB. Three components are direct RGB when the identifiers are `R`, `G`, `B`, or when the applicable Adobe/JFIF signaling selects RGB; otherwise samples are interpreted as YCbCr and converted to RGB. Chroma-subsampled planes are upsampled before the matrix conversion.

Four-component Adobe transform 0 is treated as CMYK and transform 2 as YCCK. The decoder performs its device conversion to RGB. A four-component stream without those recognized semantics uses the first three components through the fallback conversion and does not expose the fourth as alpha. JPEG alpha is therefore not supported.

This distinction matters for profiles: the builtin ICC stage sees RGB pixels after JPEG component conversion. It applies only an RGB-domain profile. A CMYK profile is not applied retroactively to original CMYK samples, because those planes are no longer the frame boundary.

## Metadata and dialect handling

| Marker convention | Treatment |
| --- | --- |
| APP0 JFIF | Recognized for common component interpretation. Density and thumbnail data are not exposed. |
| APP14 Adobe | Transform byte selects direct RGB/CMYK/YCCK behavior where applicable. |
| APP2 `ICC_PROFILE` | Numbered chunks are reassembled only when count, sequence, uniqueness, and lengths are consistent. A usable RGB profile can drive CMS. |
| APP1 Exif | TIFF-formatted orientation is parsed when enabled. Other Exif tags and thumbnails are ignored. |
| Other APPn and COM | Structurally skipped; XMP, IPTC, and comments are not returned as metadata. |

Arithmetic-coded SOF processes, differential and hierarchical modes, JPEG-LS, JPEG 2000, and extension processes outside SOF0–SOF3 are rejected. The optional `libjpeg` loader is a separate component whose feature set depends on its linked library; this page describes `-Lbuiltin!` only.

## Decode and precision pipeline

The dispatcher recognizes SOI, scans markers to decide whether the byte API is safe, and extracts Exif/ICC only for the policies that need them. The fast byte path is used only for 8-bit SOF0/SOF1/SOF2 when CMS is disabled and the encoder has not requested a float loader boundary. SOF3 always takes the float path even at eight bits.

The encoder implicitly requests float loader data when resizing is active, its working colorspace is not gamma, default quantization clusters in a non-gamma space, or another encoder precision policy requests float. That request is distinct from the loader suboption `prefer_8bit`, which controls CMS target storage rather than this JPEG decode-path decision.

| Decode condition | Initial frame |
| --- | --- |
| Eligible 8-bit DCT, CMS off, no float request | Gamma `RGB888`. |
| SOF3, precision above 8, CMS enabled, or encoder float request | Gamma `RGBFLOAT32`. |
| Applicable RGB ICC with CMS enabled | Profile conversion runs on RGB float samples; the frame remains a semantic RGB float boundary and later target conversion follows loader/encoder policy. |

Float output prevents an avoidable byte rounding between JPEG component conversion, resize, and later color transforms. It cannot restore information already discarded by lossy quantization or chroma subsampling.

### Implementation and test map

![A vertical implementation map of the builtin JPEG loader. It follows SOI routing and byte-versus-float planning into marker parsing, sequential, progressive, or lossless entropy decode, component and colorspace conversion, ICC application, and orientation-aware typed-frame output. Every node carries a coverage ID used by the tables below.](pipeline-figures/jpeg.svg)

The byte and float paths share marker and entropy machinery but diverge before component conversion. In particular, high-depth/lossless samples enter `load_jpeg_image_float_high_precision()`; documenting only the final `RGB` label would hide the precision boundary that resize and CMS tests depend on.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `JPG-01` | `sixel_builtin_load_jpeg_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | SOI chunk plus encoder/CMS precision request → byte or float JPEG API | The fast byte API is legal only for eligible eight-bit DCT input when no downstream condition requests float. |
| `JPG-02` | `stbi__decode_jpeg_header()`, `stbi__process_marker()`, and `stbi__process_scan_header()` in [`stb_image.h`](../../../src/stb_image.h) | Marker segments → frame/components, tables, restart state, and scan plan | SOF precision/process, DQT/DHT, Adobe/JFIF hints, and unsupported arithmetic coding must be classified before entropy output is trusted. |
| `JPG-03` | `stbi__parse_entropy_coded_data()` or `stbi__parse_entropy_coded_data_lossless()` in [`stb_image.h`](../../../src/stb_image.h) | Entropy bits → DCT coefficients or predicted source samples | Sequential/progressive coefficient updates, restart handling, lossless predictors, and 9–16-bit sample ranges must remain distinct. |
| `JPG-04` | `load_jpeg_image_float_high_precision()` and `stbi__YCbCr_to_RGB_row_f32()` in [`stb_image.h`](../../../src/stb_image.h) | Component planes → normalized gamma RGB float | Chroma upsampling and Gray/RGB/YCbCr/CMYK/YCCK interpretation must preserve the selected precision and Adobe transform semantics. |
| `JPG-05` | `sixel_builtin_extract_jpeg_icc()` and `sixel_cms_convert_profile_to_srgb()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | APP2 fragments plus float RGB → profile-converted RGB | ICC fragments must be assembled in sequence; only an applicable RGB profile may transform the already decoded RGB samples. |
| `JPG-06` | `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Gamma `RGB888`/`RGBFLOAT32` plus Exif → one oriented frame | Orientation may change dimensions/pixel order, while JPEG still has no alpha/background stage or animation callback. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! photo.jpg
img2sixel -Lbuiltin:cms_engine=auto:cms_target=linear:orientation=1! photo.jpg
```

| Control | Exact effect on JPEG |
| --- | --- |
| `builtin:cms_engine=...` | Enables APP2 ICC use and selects the CMS backend. Enabling CMS also selects float decode. Non-RGB profiles are non-applicable. |
| `cms_target`, `cms_intent`, `prefer_8bit` | Affect an applicable CMS conversion, not marker parsing. `prefer_8bit` does not force high-precision or lossless JPEG through the byte decoder. |
| `builtin:orientation=0|1` | Enables APP1 Exif orientation; default is `1`. Orientation is applied after decode to pixels and transparency state together. |
| Resize, working-colorspace, and clustering-colorspace options | Can request the float decode path before the resize/color-space stages run. |
| `-B`, background policy/colorspace, and `-A` | No image alpha or file background exists in this path, so these do not change JPEG pixels. |
| `-p COLORS` | Controls later palette size but does not make JPEG an indexed loader frame. |
| `-S`, `-l`, `-T`, `-g` | No effect because builtin JPEG emits one still frame with no delay. |
| PNG, HDR, PNM, and BMP-specific suboptions | No effect. |

## Failure and security boundary

Malformed marker lengths, component references, Huffman/quantization tables, scan transitions, restart placement, predictors, and allocation products fail before a frame callback. Extra junk after recognized structures may be tolerated where the inherited JPEG parser deliberately resynchronizes, so this is not a canonical-file validator.

JPEG entropy decode and high-depth prediction operate on attacker-controlled symbols and dimensions. Profile and Exif segments add separate parsers. Selecting the builtin path gives libsixel control over precision and errors, but increases project-owned attack surface and is not process isolation.

## Implementation landmarks

- Entropy, DCT/progressive/lossless decode and component conversion: [`stb_image.h`](../../../src/stb_image.h)
- Precision selection, ICC assembly, orientation, and frame creation: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Loader color contract: [Color Spaces and Loader Color Management](../../concepts/colorspace.md)
- Broader non-owning JPEG regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [JPEG coverage inventory](../../testing/builtin-loader-coverage.md#jpeg) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| JPG-01 | Eligible eight-bit sequential RGB input takes the byte path and produces the fixed complete RGB buffer. | [tests/loader/builtin/1976_loader_builtin_jpeg_rgb8_sequential_digest.t](../../../tests/loader/builtin/1976_loader_builtin_jpeg_rgb8_sequential_digest.t) |
| JPG-03 | A 16-bit lossless stream produces a gamma `RGBFLOAT32` frame with fixed numeric samples. | [tests/loader/builtin/1983_loader_builtin_jpeg_rgb16_lossless_numeric.t](../../../tests/loader/builtin/1983_loader_builtin_jpeg_rgb16_lossless_numeric.t) |
| JPG-04 | Adobe YCCK component interpretation produces the fixed complete RGB buffer for the representative stream. | [tests/loader/builtin/1963_loader_builtin_jpeg_ycck8_rgb_digest.t](../../../tests/loader/builtin/1963_loader_builtin_jpeg_ycck8_rgb_digest.t) |
| JPG-05 | Builtin ICC conversion preserves gamma float typing and representative converted samples. | [tests/loader/builtin/1978_loader_builtin_jpeg_icc_numeric.t](../../../tests/loader/builtin/1978_loader_builtin_jpeg_icc_numeric.t) |
| JPG-06 | The orientation option controls APP1 Exif geometry/pixel transformation. | [tests/loader/builtin/1667_loader_builtin_jpeg_orientation_toggle.t](../../../tests/loader/builtin/1667_loader_builtin_jpeg_orientation_toggle.t) |

### Quality regression tests

| ID | Quality floor protected | Owning test |
| --- | --- | --- |
| JPGQ-01 | An eligible eight-bit sequential RGB image retains its end-to-end visual floor. | [tests/loader/builtin/0119_loader_builtin_jpeg_rgb_8bit_seq444_r0_expected_lsqa.t](../../../tests/loader/builtin/0119_loader_builtin_jpeg_rgb_8bit_seq444_r0_expected_lsqa.t) |
| JPGQ-02 | Adobe YCCK conversion remains visually close to the external reference after encoding. | [tests/loader/builtin/0123_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t](../../../tests/loader/builtin/0123_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t) |
| JPGQ-03 | Embedded ICC conversion remains visually close to the fixed converted PNM after encoding. | [tests/loader/builtin/0056_builtin_jpeg_embedded_icc_matches_reference_pnm.t](../../../tests/loader/builtin/0056_builtin_jpeg_embedded_icc_matches_reference_pnm.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| JPG-02 | Arithmetic-coded JPEG is classified as unsupported and rejected before entropy decode. | [tests/loader/builtin/0131_loader_builtin_rejects_arithmetic_jpeg.t](../../../tests/loader/builtin/0131_loader_builtin_rejects_arithmetic_jpeg.t) |
| JPG-90 | A corrupt JPEG stream fails the builtin decoder rather than yielding a partial frame. | [tests/loader/builtin/0127_loader_builtin_rejects_corrupt_jpeg.t](../../../tests/loader/builtin/0127_loader_builtin_rejects_corrupt_jpeg.t) |

Coverage audit note: direct owners now cover sequential RGB bytes, 16-bit lossless float samples, one YCCK stream, builtin ICC samples, orientation, and two failure classes. The 16-bit fixture preserves the typed high-precision route, although its selected source samples happen to lie on eight-bit fractions and therefore do not alone prove sub-eight-bit value preservation. Progressive scans, sampling-factor combinations, restart-marker placement, and the full Gray/RGB/YCbCr/CMYK/YCCK × precision matrix have additional regressions but are not exhaustively owned by this page.
