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

The decoder accepts 8- and 16-bit quantization tables, up to four Huffman tables of each class, restart markers, and multiple scans. Horizontal and vertical sampling factors are 1 through 4 and must divide the maximum factor so component upsampling has an integral ratio.

A zero-height SOF is accepted when DNL supplies a nonzero height immediately after the first scan, as specified in T.81 B.2.2 and B.2.5. The builtin loader already buffers its input: only deferred-height frames perform a bounded marker prepass before component allocation. This covers SOF0 through SOF3 and retains the ordinary byte/float output selection.

The prepass skips length-delimited metadata, stuffed entropy bytes, restart markers, and marker fill bytes. Decode then checks that the first scan contains exactly the MCU rows required by DNL, with only final one-bit padding remaining. Missing, misplaced, duplicate, truncated, or inconsistent DNL is rejected. DNL extension bytes are skipped within the segment bounds. For a nonzero SOF height, DNL must still repeat that height; redefining a known height remains unsupported. The private stb callback-input API does not implement this prepass; all public builtin loader inputs use the buffered path.

## Component and color interpretation

One-, three-, and four-component frames are accepted. One component becomes grayscale RGB. Three components are direct RGB when the identifiers are `R`, `G`, `B`, or when the applicable Adobe/JFIF signaling selects RGB; otherwise samples are interpreted as YCbCr and converted to RGB. Chroma-subsampled planes are upsampled before the matrix conversion.

Four-component Adobe transform 0 is treated as complemented CMYK and transform 2 as YCCK. With a matching CMYK ICC, an absent Adobe marker means ordinary, non-complemented CMYK. Other Adobe transforms are not assigned CMYK semantics. The fourth component is never exposed as alpha; JPEG alpha is not supported.

ICC input domains are matched to the decoded components: Gray profiles consume the one-component intensity, RGB profiles consume three-component RGB/YCbCr output, and CMYK profiles consume all four device inks. A mismatched profile is skipped without reporting a transform failure.

Gray samples remain replicated float intensities until the shared linear-RGB CMS facade packs them as `GRAY_F32`. For CMYK/YCCK, the decoder additionally preserves normalized `CMYK_F32` coordinates before the legacy RGB approximation: Adobe CMYK complements all four components; Adobe YCCK reconstructs C/M/Y with the inverse YCbCr matrix and complements K; plain CMYK uses the original ink values. The normalization denominator is `2^precision - 1`, including twelve- and sixteen-bit input. YCCK chroma offsets use `2^(precision - 1)`. Device-coordinate range limiting belongs to JPEG reconstruction; no range limiting is applied to the resulting linear RGB.

Gray and CMYK profiles convert directly to extended linear sRGB through the selected common CMS backend. Only a complete finite result is published. CMYK uses a separate scratch buffer, so a failed or partially written transform cannot overwrite the original device-RGB fallback. Rejected profiles and failed conversions retain the existing unmanaged gamma-float result and report conversion failure. With CMS disabled or no embedded profile, existing unprofiled JPEG interpretation is unchanged, including the historical first-three-component fallback for four-component images without recognized Adobe signaling.

## Metadata and dialect handling

| Marker convention | Treatment |
| --- | --- |
| APP0 JFIF | Recognized for common component interpretation. Density and thumbnail data are not exposed. |
| APP14 Adobe | Transform byte selects direct RGB/CMYK/YCCK behavior where applicable. |
| APP2 `ICC_PROFILE` | Numbered chunks are reassembled only when count, sequence, uniqueness, and lengths are consistent. A matching Gray, RGB, or CMYK profile can drive CMS. |
| APP1 Exif | TIFF-formatted orientation is parsed when enabled. Other Exif tags and thumbnails are ignored. |
| Other APPn and COM | Structurally skipped; XMP, IPTC, and comments are not returned as metadata. |

Arithmetic-coded SOF processes, differential and hierarchical modes, JPEG-LS, JPEG 2000, and extension processes outside SOF0–SOF3 are rejected. The optional `libjpeg` loader is a separate component whose feature set depends on its linked library; this page describes `-Lbuiltin!` only.

## Decode and precision pipeline

The dispatcher recognizes SOI, scans markers to decide whether the byte API is safe, and extracts Exif/ICC only for the policies that need them. The fast byte path is used only for 8-bit SOF0/SOF1/SOF2 when there is no collected ICC and the encoder has not requested a float loader boundary. CMS being enabled alone does not promote an unprofiled image; disabling CMS also disables ICC collection. SOF3 always takes the float path even at eight bits.

The encoder implicitly requests float loader data when resizing is active, its working colorspace is not gamma, default quantization clusters in a non-gamma space, or another encoder precision policy requests float. That request is distinct from the loader suboption `prefer_8bit`, which does not override this JPEG adapter's byte/float decode or source-domain CMS boundary.

| Decode condition | Initial frame |
| --- | --- |
| Eligible 8-bit DCT, no collected ICC, no float request | Gamma `RGB888`. |
| SOF3, precision above 8, collected ICC, or encoder float request | Gamma `RGBFLOAT32` before any successful Gray/CMYK conversion. |
| Applicable RGB ICC with CMS enabled | Profile conversion runs on RGB float samples; the existing gamma `RGBFLOAT32` boundary is retained. |
| Applicable Gray or CMYK ICC with CMS enabled | Extended `LINEARRGBFLOAT32`, tagged linear, without an intermediate byte buffer. |

Float output prevents an avoidable byte rounding between JPEG component conversion, resize, and later color transforms. It cannot restore information already discarded by lossy quantization or chroma subsampling.

### Implementation and test map

![A vertical implementation map of the builtin JPEG loader. It follows SOI routing and byte-versus-float planning into marker parsing, sequential, progressive, or lossless entropy decode, component and colorspace conversion, ICC application, and orientation-aware typed-frame output. Every node carries a coverage ID used by the tables below.](pipeline-figures/jpeg.svg)

The byte and float paths share marker and entropy machinery but diverge before component conversion. In particular, high-depth/lossless samples enter `load_jpeg_image_float_high_precision()`; documenting only the final `RGB` label would hide the precision boundary that resize and CMS tests depend on.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `JPG-01` | `sixel_builtin_load_jpeg_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | SOI chunk plus encoder/CMS precision request → byte or float JPEG API | The fast byte API is legal only for eligible eight-bit DCT input when no downstream condition requests float. |
| `JPG-02` | `stbi__decode_jpeg_header()`, `stbi__process_marker()`, and `stbi__process_scan_header()` in [`stb_image.h`](../../../src/stb_image.h) | Marker segments → frame/components, tables, restart state, and scan plan | SOF precision/process, DQT/DHT, Adobe/JFIF hints, and unsupported arithmetic coding must be classified before entropy output is trusted. |
| `JPG-03` | `stbi__parse_entropy_coded_data()` or `stbi__parse_entropy_coded_data_lossless()` in [`stb_image.h`](../../../src/stb_image.h) | Entropy bits → DCT coefficients or predicted source samples | Sequential/progressive coefficient updates, restart handling, lossless predictors, and 9–16-bit sample ranges must remain distinct. |
| `JPG-04` | `load_jpeg_image_float_high_precision()` and `stbi__YCbCr_to_RGB_row_f32()` in [`stb_image.h`](../../../src/stb_image.h) | Component planes → gamma RGB fallback and, when requested, preserved CMYK floats | Chroma upsampling and Gray/RGB/YCbCr/CMYK/YCCK interpretation must preserve the selected precision and Adobe transform semantics. |
| `JPG-05` | `sixel_builtin_extract_jpeg_icc()`, `sixel_builtin_load_jpeg_frame()`, and `sixel_builtin_jpeg_cmyk_linear()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | APP2 fragments plus matching source-domain floats → profile-converted RGB | Assemble fragments in sequence, preserve source components and precision, and publish successful Gray/CMYK conversions as linear RGB. |
| `JPG-06` | `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Typed byte/float RGB plus Exif → one oriented frame | Orientation may change dimensions/pixel order, while JPEG still has no alpha/background stage or animation callback. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! photo.jpg
img2sixel -Lbuiltin:cms_engine=auto:cms_target=linear:orientation=1! photo.jpg
```

| Control | Exact effect on JPEG |
| --- | --- |
| `builtin:cms_engine=...` | Enables APP2 ICC use and selects the CMS backend. Collected ICC selects float decode; profile input domains must match the JPEG component model. |
| `cms_target`, `cms_intent`, `prefer_8bit` | `cms_intent` selects the ICC rendering intent. This adapter publishes successful Gray/CMYK CMS as linear float and RGB CMS as gamma float; it does not narrow these to `cms_target`/`prefer_8bit`. Encoder working/output conversion is separate. |
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
| JPG-05 | Builtin ICC conversion preserves gamma float typing and matches independently calculated JFIF/ICC samples within the existing 0.000001 bound. | [tests/loader/builtin/1978_loader_builtin_jpeg_icc_numeric.t](../../../tests/loader/builtin/1978_loader_builtin_jpeg_icc_numeric.t) |
| JPG-06 | The orientation option controls APP1 Exif geometry/pixel transformation. | [tests/loader/builtin/1667_loader_builtin_jpeg_orientation_toggle.t](../../../tests/loader/builtin/1667_loader_builtin_jpeg_orientation_toggle.t) |
| JPG-07 | Gray16 lossless samples that do not lie on eight-bit increments remain exact in a gamma `RGBFLOAT32` frame. | [tests/loader/builtin/2006_loader_builtin_jpeg_gray16_sub8bit_numeric.t](../../../tests/loader/builtin/2006_loader_builtin_jpeg_gray16_sub8bit_numeric.t) |
| JPG-08 | A representative progressive 4:2:0 stream produces the fixed complete RGB buffer after scan refinement. | [tests/loader/builtin/2007_loader_builtin_jpeg_progressive_420_digest.t](../../../tests/loader/builtin/2007_loader_builtin_jpeg_progressive_420_digest.t) |
| JPG-09 | High-precision lossless samples immediately around a restart boundary remain fixed. | [tests/loader/builtin/2008_loader_builtin_jpeg_gray16_restart_numeric.t](../../../tests/loader/builtin/2008_loader_builtin_jpeg_gray16_restart_numeric.t) |
| JPG-10 | A sequential YCbCr 4:4:4 stream produces the fixed complete RGB buffer. | [tests/loader/builtin/2009_loader_builtin_jpeg_ycbcr444_digest.t](../../../tests/loader/builtin/2009_loader_builtin_jpeg_ycbcr444_digest.t) |
| JPG-11 | A sequential Adobe CMYK stream produces the fixed complete RGB buffer independently of YCCK coverage. | [tests/loader/builtin/2010_loader_builtin_jpeg_cmyk8_digest.t](../../../tests/loader/builtin/2010_loader_builtin_jpeg_cmyk8_digest.t) |
| JPG-12 | Minimal in-memory lossless streams fix predictors 1 through 7 and point-transform reconstruction at exact float samples. | [tests/loader/builtin/2030_loader_builtin_jpeg_lossless_predictors_numeric.t](../../../tests/loader/builtin/2030_loader_builtin_jpeg_lossless_predictors_numeric.t) |
| JPG-13 | A three-component lossless stream fixes independent predictor resets across two restart markers. | [tests/loader/builtin/2031_loader_builtin_jpeg_rgb_restart_numeric.t](../../../tests/loader/builtin/2031_loader_builtin_jpeg_rgb_restart_numeric.t) |
| JPG-14 | Unmanaged float samples match independently decoded YCbCr and JFIF conversion before ICC interpretation. | [tests/loader/builtin/2045_loader_builtin_jpeg_source_numeric.t](../../../tests/loader/builtin/2045_loader_builtin_jpeg_source_numeric.t) |
| JPG-15 | Resolve delayed height without leaving the RGB888 path. | [tests/loader/builtin/2219_jpeg_dnl_byte_numeric.t](../../../tests/loader/builtin/2219_jpeg_dnl_byte_numeric.t) |
| JPG-16 | Resolve delayed height with explicit float32 output. | [tests/loader/builtin/2220_jpeg_dnl_float_numeric.t](../../../tests/loader/builtin/2220_jpeg_dnl_float_numeric.t) |
| JPG-17 | Preserve twelve-bit extended sequential samples with DNL. | [tests/loader/builtin/2221_jpeg_dnl_12bit_numeric.t](../../../tests/loader/builtin/2221_jpeg_dnl_12bit_numeric.t) |
| JPG-18 | Continue progressive AC scans after the first-scan DNL. | [tests/loader/builtin/2222_jpeg_dnl_progressive_numeric.t](../../../tests/loader/builtin/2222_jpeg_dnl_progressive_numeric.t) |
| JPG-19 | Preserve lossless predictor samples after delayed height. | [tests/loader/builtin/2223_jpeg_dnl_lossless_numeric.t](../../../tests/loader/builtin/2223_jpeg_dnl_lossless_numeric.t) |
| JPG-20 | Resolve delayed height across lossless RGB restart markers. | [tests/loader/builtin/2224_jpeg_dnl_restart_numeric.t](../../../tests/loader/builtin/2224_jpeg_dnl_restart_numeric.t) |
| JPG-21 | Skip stuffed FF bytes while finding the first-scan DNL. | [tests/loader/builtin/2225_jpeg_dnl_stuffed_numeric.t](../../../tests/loader/builtin/2225_jpeg_dnl_stuffed_numeric.t) |
| JPG-22 | Preserve the fixed progressive 4:2:0 pixel digest with DNL. | [tests/loader/builtin/2226_jpeg_dnl_420_digest.t](../../../tests/loader/builtin/2226_jpeg_dnl_420_digest.t) |
| JPG-23 | Skip bounded DNL extension bytes without decoding them. | [tests/loader/builtin/2237_jpeg_dnl_extension_numeric.t](../../../tests/loader/builtin/2237_jpeg_dnl_extension_numeric.t) |
| JPG-24 | Preserve known-height JPEG with a matching DNL. | [tests/loader/builtin/2238_jpeg_dnl_known_numeric.t](../../../tests/loader/builtin/2238_jpeg_dnl_known_numeric.t) |
| JPG-25 | Accept repeated FF fill bytes before the DNL marker. | [tests/loader/builtin/2241_jpeg_dnl_fill_numeric.t](../../../tests/loader/builtin/2241_jpeg_dnl_fill_numeric.t) |

The profiled-JPEG numeric oracle starts with libjpeg-turbo 3.2.0 YCbCr output (`JDCT_ISLOW`, fancy upsampling), independently of libsixel and without a CMS. [verify_jpeg_color_source.c](../../../tools/verify_jpeg_color_source.c) reproduces those original integer samples; compile it manually with libjpeg and run it on `tests/data/inputs/formats/snake-64-embedded-esrgb.jpg`. The [fixture generator](../../../tools/generate_color_fixtures.py) solves the [JFIF equations](https://www.w3.org/Graphics/JPEG/jfif.pdf) with 60-digit Decimal arithmetic, interpolates the embedded Artifex esRGB profile's stored curves, applies its stored matrix, and converts physical ICC D50 XYZ to independently derived sRGB. The [provenance record](../../../tests/data/color-numeric/jpeg.json) fixes the JPEG/profile hashes, source values, equations, expected values, and history. The source-only 0.0000005 and unchanged CMS 0.000001 bounds include the decoder's rounded inverse-YCbCr coefficients and float storage; neither expected array is copied from current libsixel output. The generator of the older literals introduced in `39c99395f` remains unresolved, and those literals no longer own the numeric contract.

### Source-domain ICC value tests

The Gray tests use analytic constant DCT blocks at 8/12 bits and adjacent predictor-1 samples at 16 bits with an exact gamma-two Gray profile. The CMYK/YCCK [fixture generator](../../../tools/generate_builtin_jpeg_icc_tests.py) writes constant-coefficient sequential/progressive blocks and adjacent lossless samples directly, without invoking an image encoder. Its [provenance record](../../../tests/data/color-numeric/builtin-jpeg-icc.json) contains the original integers, normalized device coordinates, profile hash, and independent 80-digit Decimal PCS-to-linear-sRGB values. These fixtures intentionally distinguish CMYK polarity, YCCK matrix reconstruction, source bit depth, and profile-domain mismatch. They do not measure perceptual similarity.

Builtin CMYK bounds are `max(abs(expected RGB)) * 64 / 16777216`, accounting for float source storage and ICC arithmetic. Native engines additionally allow the existing affine profile's propagated XYZ u1.15 budget. Gray bounds follow the shared [libjpeg Gray contract](../libjpeg.md#test-coverage), including ColorSync's `0.0001` absolute budget. Unmanaged float CMYK controls allow `2e-7` for the legacy multiply path; byte controls are exact. Every available engine must produce a successful source-domain apply trace; a backend fallback cannot qualify as that engine's pass. General evidence requirements are owned by the [testing guide](../../testing/guide.md#assertions-and-evidence).

| ID | Numeric/path contract | Owning test |
| --- | --- | --- |
| JPGI-01 | gray8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0001_bj_gray8_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0001_bj_gray8_builtin_numeric.t) |
| JPGI-02 | gray8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0002_bj_gray8_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0002_bj_gray8_lcms2_numeric.t) |
| JPGI-03 | gray8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0003_bj_gray8_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0003_bj_gray8_colorsync_numeric.t) |
| JPGI-04 | gray12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0004_bj_gray12_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0004_bj_gray12_builtin_numeric.t) |
| JPGI-05 | gray12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0005_bj_gray12_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0005_bj_gray12_lcms2_numeric.t) |
| JPGI-06 | gray12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0006_bj_gray12_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0006_bj_gray12_colorsync_numeric.t) |
| JPGI-07 | gray16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0007_bj_gray16_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0007_bj_gray16_builtin_numeric.t) |
| JPGI-08 | gray16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0008_bj_gray16_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0008_bj_gray16_lcms2_numeric.t) |
| JPGI-09 | gray16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0009_bj_gray16_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0009_bj_gray16_colorsync_numeric.t) |
| JPGI-10 | plain8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0010_bj_plain8_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0010_bj_plain8_builtin_numeric.t) |
| JPGI-11 | plain8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0011_bj_plain8_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0011_bj_plain8_lcms2_numeric.t) |
| JPGI-12 | plain8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0012_bj_plain8_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0012_bj_plain8_colorsync_numeric.t) |
| JPGI-13 | adobe8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0013_bj_adobe8_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0013_bj_adobe8_builtin_numeric.t) |
| JPGI-14 | adobe8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0014_bj_adobe8_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0014_bj_adobe8_lcms2_numeric.t) |
| JPGI-15 | adobe8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0015_bj_adobe8_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0015_bj_adobe8_colorsync_numeric.t) |
| JPGI-16 | ycck8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0016_bj_ycck8_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0016_bj_ycck8_builtin_numeric.t) |
| JPGI-17 | ycck8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0017_bj_ycck8_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0017_bj_ycck8_lcms2_numeric.t) |
| JPGI-18 | ycck8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0018_bj_ycck8_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0018_bj_ycck8_colorsync_numeric.t) |
| JPGI-19 | plain12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0019_bj_plain12_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0019_bj_plain12_builtin_numeric.t) |
| JPGI-20 | plain12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0020_bj_plain12_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0020_bj_plain12_lcms2_numeric.t) |
| JPGI-21 | plain12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0021_bj_plain12_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0021_bj_plain12_colorsync_numeric.t) |
| JPGI-22 | adobe12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0022_bj_adobe12_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0022_bj_adobe12_builtin_numeric.t) |
| JPGI-23 | adobe12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0023_bj_adobe12_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0023_bj_adobe12_lcms2_numeric.t) |
| JPGI-24 | adobe12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0024_bj_adobe12_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0024_bj_adobe12_colorsync_numeric.t) |
| JPGI-25 | ycck12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0025_bj_ycck12_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0025_bj_ycck12_builtin_numeric.t) |
| JPGI-26 | ycck12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0026_bj_ycck12_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0026_bj_ycck12_lcms2_numeric.t) |
| JPGI-27 | ycck12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0027_bj_ycck12_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0027_bj_ycck12_colorsync_numeric.t) |
| JPGI-28 | plain16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0028_bj_plain16_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0028_bj_plain16_builtin_numeric.t) |
| JPGI-29 | plain16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0029_bj_plain16_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0029_bj_plain16_lcms2_numeric.t) |
| JPGI-30 | plain16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0030_bj_plain16_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0030_bj_plain16_colorsync_numeric.t) |
| JPGI-31 | adobe16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0031_bj_adobe16_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0031_bj_adobe16_builtin_numeric.t) |
| JPGI-32 | adobe16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0032_bj_adobe16_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0032_bj_adobe16_lcms2_numeric.t) |
| JPGI-33 | adobe16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0033_bj_adobe16_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0033_bj_adobe16_colorsync_numeric.t) |
| JPGI-34 | ycck16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0034_bj_ycck16_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0034_bj_ycck16_builtin_numeric.t) |
| JPGI-35 | ycck16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0035_bj_ycck16_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0035_bj_ycck16_lcms2_numeric.t) |
| JPGI-36 | ycck16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0036_bj_ycck16_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0036_bj_ycck16_colorsync_numeric.t) |
| JPGI-37 | progressive8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0037_bj_progressive8_builtin_numeric.t](../../../tests/loader/builtin/jpeg/0037_bj_progressive8_builtin_numeric.t) |
| JPGI-38 | progressive8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0038_bj_progressive8_lcms2_numeric.t](../../../tests/loader/builtin/jpeg/0038_bj_progressive8_lcms2_numeric.t) |
| JPGI-39 | progressive8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0039_bj_progressive8_colorsync_numeric.t](../../../tests/loader/builtin/jpeg/0039_bj_progressive8_colorsync_numeric.t) |
| JPGI-40 | ink none byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0040_bj_ink_none_byte_numeric.t](../../../tests/loader/builtin/jpeg/0040_bj_ink_none_byte_numeric.t) |
| JPGI-41 | ink absent byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0041_bj_ink_absent_byte_numeric.t](../../../tests/loader/builtin/jpeg/0041_bj_ink_absent_byte_numeric.t) |
| JPGI-42 | ink rejected: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0042_bj_ink_rejected_numeric.t](../../../tests/loader/builtin/jpeg/0042_bj_ink_rejected_numeric.t) |
| JPGI-43 | ink apply failure: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0043_bj_ink_apply_failure_numeric.t](../../../tests/loader/builtin/jpeg/0043_bj_ink_apply_failure_numeric.t) |
| JPGI-44 | ink mismatch: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0044_bj_ink_mismatch_numeric.t](../../../tests/loader/builtin/jpeg/0044_bj_ink_mismatch_numeric.t) |
| JPGI-45 | ink none float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0045_bj_ink_none_float_numeric.t](../../../tests/loader/builtin/jpeg/0045_bj_ink_none_float_numeric.t) |
| JPGI-46 | ink absent float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0046_bj_ink_absent_float_numeric.t](../../../tests/loader/builtin/jpeg/0046_bj_ink_absent_float_numeric.t) |
| JPGI-47 | gray none byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0047_bj_gray_none_byte_numeric.t](../../../tests/loader/builtin/jpeg/0047_bj_gray_none_byte_numeric.t) |
| JPGI-48 | gray absent byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0048_bj_gray_absent_byte_numeric.t](../../../tests/loader/builtin/jpeg/0048_bj_gray_absent_byte_numeric.t) |
| JPGI-49 | gray rejected: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0049_bj_gray_rejected_numeric.t](../../../tests/loader/builtin/jpeg/0049_bj_gray_rejected_numeric.t) |
| JPGI-50 | gray none12 float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0050_bj_gray_none12_float_numeric.t](../../../tests/loader/builtin/jpeg/0050_bj_gray_none12_float_numeric.t) |
| JPGI-51 | gray none16 float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader/builtin/jpeg/0051_bj_gray_none16_float_numeric.t](../../../tests/loader/builtin/jpeg/0051_bj_gray_none16_float_numeric.t) |
| JPGI-52 | 8-bit CMYK source-buffer allocation failures release all owned memory; successful optional-profile fallback retains a valid complete frame. | [tests/loader/builtin/jpeg/0052_bj_ink8_allocation.t](../../../tests/loader/builtin/jpeg/0052_bj_ink8_allocation.t) |
| JPGI-53 | 16-bit CMYK source-buffer allocation failures release all owned memory; successful optional-profile fallback retains a valid complete frame. | [tests/loader/builtin/jpeg/0053_bj_ink16_allocation.t](../../../tests/loader/builtin/jpeg/0053_bj_ink16_allocation.t) |

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

Coverage audit note: direct owners now cover sequential RGB bytes, progressive 4:2:0 output, sub-eight-bit Gray16 values, all seven lossless predictors with a point transform, Gray and RGB restart boundaries, YCbCr 4:4:4, CMYK, YCCK, builtin ICC samples, orientation, and two failure classes. These representatives still do not establish every sampling-factor combination, progressive scan script, restart interval/MCU placement, point transform, or the full Gray/RGB/YCbCr/CMYK/YCCK × precision matrix.

DNL defensive coverage in the [builtin test inventory](../../testing/builtin-loader-coverage.md#jpeg) includes missing/zero/short/truncated markers, wrong placement, duplicate markers, known-height disagreement, extra or missing MCU rows, early termination at a restart boundary, and truncated amplitude bits. These are parser/value checks, not MS-SSIM quality thresholds.
