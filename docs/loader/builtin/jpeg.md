# Builtin JPEG Loader

## Format lineage

JPEG names both a family of continuous-tone compression processes and the marker-coded interchange syntax standardized as ISO/IEC 10918-1 and [ITU-T T.81](https://www.itu.int/rec/T-REC-T.81). JFIF later standardized common file interchange conventions around that syntax; Exif and Adobe APP markers introduced other widely deployed metadata and component conventions. JPEG 2000, JPEG-LS, and JPEG XL are independent formats despite sharing the name.

The builtin decoder entered libsixel with stb_image 1.33 in commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd) in 2014. It remains an adapted stb-derived core in [`stb_image.h`](../../../src/stb_image.h), but it is no longer stock behavior. Commit [`158bb61ea`](https://github.com/saitoha/libsixel/commit/158bb61ea) added a float output path in 2026. Commits [`cb7fb59bb`](https://github.com/saitoha/libsixel/commit/cb7fb59bb) and [`81184726a`](https://github.com/saitoha/libsixel/commit/81184726a) added and hardened high-depth lossless JPEG. Metadata extraction and frame routing live in [`loader-builtin.c`](../../../src/loader-builtin.c).

## Marker container and coding processes

A JPEG interchange stream begins with SOI and is a sequence of markers and entropy-coded scan data ending at EOI. SOF defines dimensions, components, precision, and sampling; DQT and DHT define quantization and Huffman tables; DAC supplies arithmetic conditioning; DRI establishes restart intervals; SOS begins a scan. APPn and COM segments carry conventions or metadata outside the coded sample process.

| SOF marker | Process | Builtin precision |
| --- | --- | --- |
| `SOF0` | Baseline sequential DCT | 8 bits |
| `SOF1` | Extended sequential DCT | 8 through 12 bits |
| `SOF2` | Progressive DCT | 8 through 12 bits |
| `SOF3` | Huffman lossless predictive | 2 through 16 bits |
| `SOF9`, `SOF10` | Arithmetic sequential / progressive DCT | 8 or 12 bits |
| `SOF11` | Arithmetic lossless predictive | 2 through 16 bits |
| `SOF5`, `SOF6`, `SOF7` | Huffman differential sequential / progressive / lossless | DCT: 8 or 12; lossless: 2 through 16 bits |
| `SOF13`, `SOF14`, `SOF15` | Arithmetic differential sequential / progressive / lossless | DCT: 8 or 12; lossless: 2 through 16 bits |

Lossy modes level-shift samples, divide the image into component blocks, apply an 8×8 DCT and quantization, reorder coefficients, and entropy-code DC differences and AC runs with Huffman or arithmetic coding. Progressive scans deliver coefficient bands or refinements over several scans. Lossless JPEG does not use DCT or quantization: it predicts each sample from neighboring samples with predictor 1 through 7 and entropy-codes the difference, with a point transform below the sample precision.

The decoder accepts 8- and 16-bit quantization tables, up to four Huffman tables of each class, restart markers, and multiple scans. Horizontal and vertical sampling factors are 1 through 4 and must divide the maximum factor so component upsampling has an integral ratio.

DNL may define a zero SOF height or redefine a nonzero height immediately after the first scan, as specified in T.81 B.2.5. The builtin loader buffers its input and resolves DNL before component allocation, retaining ordinary byte/float output selection. The prepass skips metadata, stuffed entropy bytes, restart markers, and marker fill bytes. Missing, misplaced, duplicate, truncated, or inconsistent DNL is rejected; bounded extension bytes are skipped. Huffman scans must contain exactly the required MCU rows and final one-bit padding. Arithmetic termination instead supplies zero bytes after a real marker; physical EOF is an error. Entropy coding does not detect every possible damaged marker-terminated stream. The private stb callback-input API does not implement the DNL prepass; all public builtin loader inputs use the buffered path.

Lossless Huffman and arithmetic decoding share one sample-reconstruction routine. Huffman category 16 represents the half-range difference 32768 with no appended amplitude bits. Prediction uses point-transformed unsigned neighbors, preserves overflow/underflow until modulo reconstruction, and rounds negative half-differences by arithmetic right shift. The first sample of a scan or restart interval uses the midpoint at effective precision `P-Pt`; the remainder of that first row uses the left neighbor. Only after reconstruction are `Pt` low zero bits restored. These rules apply independently to every component in interleaved or separate scans.

Arithmetic decoding implements the T.81 Annex D decision engine and the DCT/progressive/lossless coding models. DAC defaults are DC `L=0,U=1` and AC `K=5`; explicit conditioning supports four destinations, `0 <= L <= U <= 15`, and `1 <= K <= 63`. Statistics reset at scan and restart boundaries. Progressive scans preserve coefficient bands and refinement order. Lossless prediction uses the effective precision after point transformation, independent component histories, and modulo addition rather than saturating prediction differences.

Hierarchical images start with DHP, which describes final dimensions, precision, component identities, and sampling. Each component begins in a non-differential frame; subsequent differential frames reconstruct it from the retained reference plus signed differences. EXP selects horizontal, vertical, or both factor-two expansions. Interpolation truncates horizontally before vertically and replicates the far edge; odd final dimensions crop that expanded edge. Component groups can arrive in separate frames. The decoder enforces consistent entropy processes (with the standard baseline-Huffman exception), source precision, component sampling, and DCT/lossless transitions. Only the completed DHP image reaches color interpretation, CMS, and the frame callback.

Differential DCT omits both DC prediction and the IDCT level shift. Its signed intermediate samples survive subsequent frames and expansion; the final lossy reconstruction is saturated to its source sample range before chroma resampling. Sixteen-bit lossless reconstruction retains all unsigned sample values and modulo-65536 addition. New arithmetic and hierarchical DCT paths use 32-bit dequantized coefficients and 64-bit IDCT accumulators, avoiding truncation through short coefficient storage.

## Component and color interpretation

One-, three-, and four-component images are accepted. Hierarchical frames may also carry a two-component subset of the final image. One component becomes grayscale RGB. Three components are direct RGB when the identifiers are `R`, `G`, `B`, or when the applicable Adobe/JFIF signaling selects RGB; otherwise samples are interpreted as YCbCr and converted to RGB. Chroma-subsampled planes are upsampled before the matrix conversion.

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

JPEG-LS, JPEG 2000, reserved SOF8, and extension processes outside the table above are unsupported. The optional `libjpeg` loader is a separate component whose feature set depends on its linked library; this page describes `-Lbuiltin!` only.

## Decode and precision pipeline

The dispatcher recognizes SOI, scans markers to decide whether the byte API is safe, and extracts Exif/ICC only for the policies that need them. The fast byte path is used only for 8-bit non-hierarchical SOF0/SOF1/SOF2/SOF9/SOF10 when there is no collected ICC and the encoder has not requested a float loader boundary. CMS being enabled alone does not promote an unprofiled image; disabling CMS also disables ICC collection. Predictive and hierarchical JPEG always take the float path, including eight-bit sources. Arithmetic support is in-tree and does not require or fall back to the optional libjpeg loader.

The encoder implicitly requests float loader data when resizing is active, its working colorspace is not gamma, default quantization clusters in a non-gamma space, or another encoder precision policy requests float. That request is distinct from the loader suboption `prefer_8bit`, which does not override this JPEG adapter's byte/float decode or source-domain CMS boundary.

| Decode condition | Initial frame |
| --- | --- |
| Eligible 8-bit DCT, no collected ICC, no float request | Gamma `RGB888`. |
| Predictive/hierarchical input, precision above 8, collected ICC, or encoder float request | Gamma `RGBFLOAT32` before any successful Gray/CMYK conversion. |
| Applicable RGB ICC with CMS enabled | Profile conversion runs on RGB float samples; the existing gamma `RGBFLOAT32` boundary is retained. |
| Applicable Gray or CMYK ICC with CMS enabled | Extended `LINEARRGBFLOAT32`, tagged linear, without an intermediate byte buffer. |

Float output prevents an avoidable byte rounding between JPEG component conversion, resize, and later color transforms. It cannot restore information already discarded by lossy quantization or chroma subsampling.

### Implementation and test map

![A vertical implementation map of the builtin JPEG loader. It follows SOI routing and byte-versus-float planning into marker parsing, sequential, progressive, or lossless entropy decode, component and colorspace conversion, ICC application, and orientation-aware typed-frame output. Every node carries a coverage ID used by the tables below.](figures/jpeg.svg)

The byte and float paths share marker and entropy machinery but diverge before component conversion. In particular, high-depth/lossless samples enter `load_jpeg_image_float_high_precision()`; documenting only the final `RGB` label would hide the precision boundary that resize and CMS tests depend on.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `JPG-01` | `sixel_builtin_load_jpeg_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | SOI chunk plus encoder/CMS precision request → byte or float JPEG API | The fast byte API is legal only for eligible eight-bit DCT input when no downstream condition requests float. |
| `JPG-02` | `stbi__decode_jpeg_header()`, `stbi__process_marker()`, and `stbi__process_scan_header()` in [`stb_image.h`](../../../src/stb_image.h) | Marker segments → frame/components, tables, restart state, and scan plan | Validate SOF precision/process, DQT/DHT/DAC, DHP/EXP transitions, and Adobe/JFIF hints before trusting entropy output. |
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
- Loader color contract: [Color-Space Handling](../../encoder/colorspace-handling/README.md)
- Broader non-owning JPEG regression suite: [`tests/loader-jpeg`](../../../tests/loader-jpeg)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [JPEG coverage inventory](../../testing/builtin-loader-coverage.md#jpeg) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| JPG-02 | Arithmetic JPEG succeeds through the explicitly selected builtin loader. | [tests/loader-jpeg/decode/0010_loader_builtin_arithmetic_jpeg.t](../../../tests/loader-jpeg/decode/0010_loader_builtin_arithmetic_jpeg.t) |
| JPG-01 | Eligible eight-bit sequential RGB input takes the byte path and produces the fixed complete RGB buffer. | [tests/loader-jpeg/decode/0015_loader_builtin_jpeg_rgb8_sequential_digest.t](../../../tests/loader-jpeg/decode/0015_loader_builtin_jpeg_rgb8_sequential_digest.t) |
| JPG-03 | A 16-bit lossless stream produces a gamma `RGBFLOAT32` frame with fixed numeric samples. | [tests/loader-jpeg/decode/0017_loader_builtin_jpeg_rgb16_lossless_numeric.t](../../../tests/loader-jpeg/decode/0017_loader_builtin_jpeg_rgb16_lossless_numeric.t) |
| JPG-04 | Adobe YCCK component interpretation produces the fixed complete RGB buffer for the representative stream. | [tests/loader-jpeg/cmyk/0007_loader_builtin_jpeg_ycck8_rgb_digest.t](../../../tests/loader-jpeg/cmyk/0007_loader_builtin_jpeg_ycck8_rgb_digest.t) |
| JPG-05 | Builtin ICC conversion preserves gamma float typing and matches independently calculated JFIF/ICC samples within the existing 0.000001 bound. | [tests/loader-jpeg/decode/0016_loader_builtin_jpeg_icc_numeric.t](../../../tests/loader-jpeg/decode/0016_loader_builtin_jpeg_icc_numeric.t) |
| JPG-06 | The orientation option controls APP1 Exif geometry/pixel transformation. | [tests/loader-jpeg/decode/0014_loader_builtin_jpeg_orientation_toggle.t](../../../tests/loader-jpeg/decode/0014_loader_builtin_jpeg_orientation_toggle.t) |
| JPG-07 | Gray16 lossless samples that do not lie on eight-bit increments remain exact in a gamma `RGBFLOAT32` frame. | [tests/loader-jpeg/gray/0004_loader_builtin_jpeg_gray16_sub8bit_numeric.t](../../../tests/loader-jpeg/gray/0004_loader_builtin_jpeg_gray16_sub8bit_numeric.t) |
| JPG-08 | A representative progressive 4:2:0 stream produces the fixed complete RGB buffer after scan refinement. | [tests/loader-jpeg/decode/0018_loader_builtin_jpeg_progressive_420_digest.t](../../../tests/loader-jpeg/decode/0018_loader_builtin_jpeg_progressive_420_digest.t) |
| JPG-09 | High-precision lossless samples immediately around a restart boundary remain fixed. | [tests/loader-jpeg/codestream/0002_loader_builtin_jpeg_gray16_restart_numeric.t](../../../tests/loader-jpeg/codestream/0002_loader_builtin_jpeg_gray16_restart_numeric.t) |
| JPG-10 | A sequential YCbCr 4:4:4 stream produces the fixed complete RGB buffer. | [tests/loader-jpeg/decode/0019_loader_builtin_jpeg_ycbcr444_digest.t](../../../tests/loader-jpeg/decode/0019_loader_builtin_jpeg_ycbcr444_digest.t) |
| JPG-11 | A sequential Adobe CMYK stream produces the fixed complete RGB buffer independently of YCCK coverage. | [tests/loader-jpeg/cmyk/0008_loader_builtin_jpeg_cmyk8_digest.t](../../../tests/loader-jpeg/cmyk/0008_loader_builtin_jpeg_cmyk8_digest.t) |
| JPG-12 | Minimal in-memory lossless streams fix predictors 1 through 7 and point-transform reconstruction at exact float samples. | [tests/loader-jpeg/decode/0020_loader_builtin_jpeg_lossless_predictors_numeric.t](../../../tests/loader-jpeg/decode/0020_loader_builtin_jpeg_lossless_predictors_numeric.t) |
| JPG-13 | A three-component lossless stream fixes independent predictor resets across two restart markers. | [tests/loader-jpeg/codestream/0003_loader_builtin_jpeg_rgb_restart_numeric.t](../../../tests/loader-jpeg/codestream/0003_loader_builtin_jpeg_rgb_restart_numeric.t) |
| JPG-14 | Unmanaged float samples match independently decoded YCbCr and JFIF conversion before ICC interpretation. | [tests/loader-jpeg/decode/0021_loader_builtin_jpeg_source_numeric.t](../../../tests/loader-jpeg/decode/0021_loader_builtin_jpeg_source_numeric.t) |
| JPG-15 | Resolve delayed height without leaving the RGB888 path. | [tests/loader-jpeg/codestream/0004_jpeg_dnl_byte_numeric.t](../../../tests/loader-jpeg/codestream/0004_jpeg_dnl_byte_numeric.t) |
| JPG-16 | Resolve delayed height with explicit float32 output. | [tests/loader-jpeg/codestream/0005_jpeg_dnl_float_numeric.t](../../../tests/loader-jpeg/codestream/0005_jpeg_dnl_float_numeric.t) |
| JPG-17 | Preserve twelve-bit extended sequential samples with DNL. | [tests/loader-jpeg/codestream/0006_jpeg_dnl_12bit_numeric.t](../../../tests/loader-jpeg/codestream/0006_jpeg_dnl_12bit_numeric.t) |
| JPG-18 | Continue progressive AC scans after the first-scan DNL. | [tests/loader-jpeg/codestream/0007_jpeg_dnl_progressive_numeric.t](../../../tests/loader-jpeg/codestream/0007_jpeg_dnl_progressive_numeric.t) |
| JPG-19 | Preserve lossless predictor samples after delayed height. | [tests/loader-jpeg/codestream/0008_jpeg_dnl_lossless_numeric.t](../../../tests/loader-jpeg/codestream/0008_jpeg_dnl_lossless_numeric.t) |
| JPG-20 | Resolve delayed height across lossless RGB restart markers. | [tests/loader-jpeg/codestream/0009_jpeg_dnl_restart_numeric.t](../../../tests/loader-jpeg/codestream/0009_jpeg_dnl_restart_numeric.t) |
| JPG-21 | Skip stuffed FF bytes while finding the first-scan DNL. | [tests/loader-jpeg/codestream/0010_jpeg_dnl_stuffed_numeric.t](../../../tests/loader-jpeg/codestream/0010_jpeg_dnl_stuffed_numeric.t) |
| JPG-22 | Preserve the fixed progressive 4:2:0 pixel digest with DNL. | [tests/loader-jpeg/codestream/0011_jpeg_dnl_420_digest.t](../../../tests/loader-jpeg/codestream/0011_jpeg_dnl_420_digest.t) |
| JPG-23 | Skip bounded DNL extension bytes without decoding them. | [tests/loader-jpeg/codestream/0022_jpeg_dnl_extension_numeric.t](../../../tests/loader-jpeg/codestream/0022_jpeg_dnl_extension_numeric.t) |
| JPG-24 | Preserve known-height JPEG with a matching DNL. | [tests/loader-jpeg/codestream/0023_jpeg_dnl_known_numeric.t](../../../tests/loader-jpeg/codestream/0023_jpeg_dnl_known_numeric.t) |
| JPG-25 | Accept repeated FF fill bytes before the DNL marker. | [tests/loader-jpeg/codestream/0026_jpeg_dnl_fill_numeric.t](../../../tests/loader-jpeg/codestream/0026_jpeg_dnl_fill_numeric.t) |

The profiled-JPEG numeric oracle starts with libjpeg-turbo 3.2.0 YCbCr output (`JDCT_ISLOW`, fancy upsampling), independently of libsixel and without a CMS. [verify_jpeg_color_source.c](../../../tools/verify_jpeg_color_source.c) reproduces those original integer samples; compile it manually with libjpeg and run it on `tests/data/inputs/formats/snake-64-embedded-esrgb.jpg`. The [fixture generator](../../../tools/generate_color_fixtures.py) solves the [JFIF equations](https://www.w3.org/Graphics/JPEG/jfif.pdf) with 60-digit Decimal arithmetic, interpolates the embedded Artifex esRGB profile's stored curves, applies its stored matrix, and converts physical ICC D50 XYZ to independently derived sRGB. The [provenance record](../../../tests/data/color-numeric/jpeg.json) fixes the JPEG/profile hashes, source values, equations, expected values, and history. The source-only 0.0000005 and unchanged CMS 0.000001 bounds include the decoder's rounded inverse-YCbCr coefficients and float storage; neither expected array is copied from current libsixel output. The generator of the older literals introduced in `39c99395f` remains unresolved, and those literals no longer own the numeric contract.

### Source-domain ICC value tests

The Gray tests use analytic constant DCT blocks at 8/12 bits and adjacent predictor-1 samples at 16 bits with an exact gamma-two Gray profile. The CMYK/YCCK [fixture generator](../../../tools/generate_builtin_jpeg_icc_tests.py) writes constant-coefficient sequential/progressive blocks and adjacent lossless samples directly, without invoking an image encoder. Its [provenance record](../../../tests/data/color-numeric/builtin-jpeg-icc.json) contains the original integers, normalized device coordinates, profile hash, and independent 80-digit Decimal PCS-to-linear-sRGB values. These fixtures intentionally distinguish CMYK polarity, YCCK matrix reconstruction, source bit depth, and profile-domain mismatch. They do not measure perceptual similarity.

Builtin CMYK bounds are `max(abs(expected RGB)) * 64 / 16777216`, accounting for float source storage and ICC arithmetic. Native engines additionally allow the existing affine profile's propagated XYZ u1.15 budget. Gray bounds follow the shared [libjpeg Gray contract](../libjpeg.md#test-coverage), including ColorSync's `0.0001` absolute budget. Unmanaged float CMYK controls allow `2e-7` for the legacy multiply path; byte controls are exact. Every available engine must produce a successful source-domain apply trace; a backend fallback cannot qualify as that engine's pass. General evidence requirements are owned by the [testing guide](../../testing/guide.md#assertions-and-evidence).

| ID | Numeric/path contract | Owning test |
| --- | --- | --- |
| JPGI-01 | gray8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0005_bj_gray8_builtin_numeric.t](../../../tests/loader-jpeg/gray/0005_bj_gray8_builtin_numeric.t) |
| JPGI-02 | gray8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0006_bj_gray8_lcms2_numeric.t](../../../tests/loader-jpeg/gray/0006_bj_gray8_lcms2_numeric.t) |
| JPGI-03 | gray8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0007_bj_gray8_colorsync_numeric.t](../../../tests/loader-jpeg/gray/0007_bj_gray8_colorsync_numeric.t) |
| JPGI-04 | gray12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0008_bj_gray12_builtin_numeric.t](../../../tests/loader-jpeg/gray/0008_bj_gray12_builtin_numeric.t) |
| JPGI-05 | gray12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0009_bj_gray12_lcms2_numeric.t](../../../tests/loader-jpeg/gray/0009_bj_gray12_lcms2_numeric.t) |
| JPGI-06 | gray12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0010_bj_gray12_colorsync_numeric.t](../../../tests/loader-jpeg/gray/0010_bj_gray12_colorsync_numeric.t) |
| JPGI-07 | gray16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0011_bj_gray16_builtin_numeric.t](../../../tests/loader-jpeg/gray/0011_bj_gray16_builtin_numeric.t) |
| JPGI-08 | gray16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0012_bj_gray16_lcms2_numeric.t](../../../tests/loader-jpeg/gray/0012_bj_gray16_lcms2_numeric.t) |
| JPGI-09 | gray16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0013_bj_gray16_colorsync_numeric.t](../../../tests/loader-jpeg/gray/0013_bj_gray16_colorsync_numeric.t) |
| JPGI-10 | plain8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0009_bj_plain8_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0009_bj_plain8_builtin_numeric.t) |
| JPGI-11 | plain8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0010_bj_plain8_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0010_bj_plain8_lcms2_numeric.t) |
| JPGI-12 | plain8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0011_bj_plain8_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0011_bj_plain8_colorsync_numeric.t) |
| JPGI-13 | adobe8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0012_bj_adobe8_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0012_bj_adobe8_builtin_numeric.t) |
| JPGI-14 | adobe8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0013_bj_adobe8_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0013_bj_adobe8_lcms2_numeric.t) |
| JPGI-15 | adobe8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0014_bj_adobe8_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0014_bj_adobe8_colorsync_numeric.t) |
| JPGI-16 | ycck8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0015_bj_ycck8_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0015_bj_ycck8_builtin_numeric.t) |
| JPGI-17 | ycck8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0016_bj_ycck8_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0016_bj_ycck8_lcms2_numeric.t) |
| JPGI-18 | ycck8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0017_bj_ycck8_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0017_bj_ycck8_colorsync_numeric.t) |
| JPGI-19 | plain12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0018_bj_plain12_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0018_bj_plain12_builtin_numeric.t) |
| JPGI-20 | plain12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0019_bj_plain12_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0019_bj_plain12_lcms2_numeric.t) |
| JPGI-21 | plain12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0020_bj_plain12_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0020_bj_plain12_colorsync_numeric.t) |
| JPGI-22 | adobe12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0021_bj_adobe12_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0021_bj_adobe12_builtin_numeric.t) |
| JPGI-23 | adobe12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0022_bj_adobe12_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0022_bj_adobe12_lcms2_numeric.t) |
| JPGI-24 | adobe12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0023_bj_adobe12_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0023_bj_adobe12_colorsync_numeric.t) |
| JPGI-25 | ycck12 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0024_bj_ycck12_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0024_bj_ycck12_builtin_numeric.t) |
| JPGI-26 | ycck12 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0025_bj_ycck12_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0025_bj_ycck12_lcms2_numeric.t) |
| JPGI-27 | ycck12 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0026_bj_ycck12_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0026_bj_ycck12_colorsync_numeric.t) |
| JPGI-28 | plain16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0027_bj_plain16_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0027_bj_plain16_builtin_numeric.t) |
| JPGI-29 | plain16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0028_bj_plain16_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0028_bj_plain16_lcms2_numeric.t) |
| JPGI-30 | plain16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0029_bj_plain16_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0029_bj_plain16_colorsync_numeric.t) |
| JPGI-31 | adobe16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0030_bj_adobe16_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0030_bj_adobe16_builtin_numeric.t) |
| JPGI-32 | adobe16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0031_bj_adobe16_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0031_bj_adobe16_lcms2_numeric.t) |
| JPGI-33 | adobe16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0032_bj_adobe16_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0032_bj_adobe16_colorsync_numeric.t) |
| JPGI-34 | ycck16 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0033_bj_ycck16_builtin_numeric.t](../../../tests/loader-jpeg/cmyk/0033_bj_ycck16_builtin_numeric.t) |
| JPGI-35 | ycck16 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0034_bj_ycck16_lcms2_numeric.t](../../../tests/loader-jpeg/cmyk/0034_bj_ycck16_lcms2_numeric.t) |
| JPGI-36 | ycck16 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0035_bj_ycck16_colorsync_numeric.t](../../../tests/loader-jpeg/cmyk/0035_bj_ycck16_colorsync_numeric.t) |
| JPGI-37 | progressive8 builtin: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/decode/0022_bj_progressive8_builtin_numeric.t](../../../tests/loader-jpeg/decode/0022_bj_progressive8_builtin_numeric.t) |
| JPGI-38 | progressive8 lcms2: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/decode/0023_bj_progressive8_lcms2_numeric.t](../../../tests/loader-jpeg/decode/0023_bj_progressive8_lcms2_numeric.t) |
| JPGI-39 | progressive8 colorsync: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/decode/0024_bj_progressive8_colorsync_numeric.t](../../../tests/loader-jpeg/decode/0024_bj_progressive8_colorsync_numeric.t) |
| JPGI-40 | ink none byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0036_bj_ink_none_byte_numeric.t](../../../tests/loader-jpeg/cmyk/0036_bj_ink_none_byte_numeric.t) |
| JPGI-41 | ink absent byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0037_bj_ink_absent_byte_numeric.t](../../../tests/loader-jpeg/cmyk/0037_bj_ink_absent_byte_numeric.t) |
| JPGI-42 | ink rejected: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/codestream/0029_bj_ink_rejected_numeric.t](../../../tests/loader-jpeg/codestream/0029_bj_ink_rejected_numeric.t) |
| JPGI-43 | ink apply failure: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0038_bj_ink_apply_failure_numeric.t](../../../tests/loader-jpeg/cmyk/0038_bj_ink_apply_failure_numeric.t) |
| JPGI-44 | ink mismatch: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0039_bj_ink_mismatch_numeric.t](../../../tests/loader-jpeg/cmyk/0039_bj_ink_mismatch_numeric.t) |
| JPGI-45 | ink none float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0040_bj_ink_none_float_numeric.t](../../../tests/loader-jpeg/cmyk/0040_bj_ink_none_float_numeric.t) |
| JPGI-46 | ink absent float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/cmyk/0041_bj_ink_absent_float_numeric.t](../../../tests/loader-jpeg/cmyk/0041_bj_ink_absent_float_numeric.t) |
| JPGI-47 | gray none byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0014_bj_gray_none_byte_numeric.t](../../../tests/loader-jpeg/gray/0014_bj_gray_none_byte_numeric.t) |
| JPGI-48 | gray absent byte: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0015_bj_gray_absent_byte_numeric.t](../../../tests/loader-jpeg/gray/0015_bj_gray_absent_byte_numeric.t) |
| JPGI-49 | gray rejected: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/codestream/0030_bj_gray_rejected_numeric.t](../../../tests/loader-jpeg/codestream/0030_bj_gray_rejected_numeric.t) |
| JPGI-50 | gray none12 float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0016_bj_gray_none12_float_numeric.t](../../../tests/loader-jpeg/gray/0016_bj_gray_none12_float_numeric.t) |
| JPGI-51 | gray none16 float: exact frame metadata, independently calculated pixel intervals, and actual CMS apply/skip evidence. | [tests/loader-jpeg/gray/0017_bj_gray_none16_float_numeric.t](../../../tests/loader-jpeg/gray/0017_bj_gray_none16_float_numeric.t) |
| JPGI-52 | 8-bit CMYK source-buffer allocation failures release all owned memory; successful optional-profile fallback retains a valid complete frame. | [tests/loader-jpeg/decode/0025_bj_ink8_allocation.t](../../../tests/loader-jpeg/decode/0025_bj_ink8_allocation.t) |
| JPGI-53 | 16-bit CMYK source-buffer allocation failures release all owned memory; successful optional-profile fallback retains a valid complete frame. | [tests/loader-jpeg/decode/0026_bj_ink16_allocation.t](../../../tests/loader-jpeg/decode/0026_bj_ink16_allocation.t) |

### Arithmetic, hierarchy, and DNL value tests

The [process fixture manifest](../../../tests/data/color-numeric/jpeg-process-source.json) freezes the encoded bytes, hashes, original samples or DCT coefficients, and encoder provenance. [generate_jpeg_process_tests.py](../../../tools/generate_jpeg_process_tests.py) independently derives expectations, including 80-digit Decimal inverse DCT for AC cases; it does not read decoder output. The fixtures cover libjpeg-turbo 3.2.0 arithmetic DCT, independently constructed Huffman hierarchy, and arithmetic lossless streams from the recorded reference-encoder revision. Its point-transform fixture is encoded at effective precision before changing only the equivalent SOF/SOS parameters; this avoids relying on the external encoder's incorrect initial predictor for nonzero point transforms.

Byte results are exact; normalized float results allow only four float epsilons relative to the expected value, with black exact. CMYK cases reuse the source-domain ICC oracle above and require an actual successful builtin CMS trace. Coefficient-preserving entropy-transcode digests are equivalence regressions, separate from these independently derived value contracts and from MS-SSIM tests.

| ID | Contract | Owning test |
| --- | --- | --- |
| JPGP-01 | DNL redefines an already nonzero SOF height. | [tests/loader-jpeg/codestream/0024_jpeg_dnl_redefine_numeric.t](../../../tests/loader-jpeg/codestream/0024_jpeg_dnl_redefine_numeric.t) |
| JPGP-02 | Arithmetic grayscale without a float requirement retains exact RGB888 samples. | [tests/loader-jpeg/gray/0018_jp_arith_gray_byte.t](../../../tests/loader-jpeg/gray/0018_jp_arith_gray_byte.t) |
| JPGP-03 | Arithmetic progressive DC/AC refinement and restart boundaries preserve analytic samples. | [tests/loader-jpeg/precision/0003_jp_arith_prog_ac.t](../../../tests/loader-jpeg/precision/0003_jp_arith_prog_ac.t) |
| JPGP-04 | Custom DAC destination 3 and conditioning preserve twelve-bit progressive samples. | [tests/loader-jpeg/precision/0006_jp_arith_12_prog.t](../../../tests/loader-jpeg/precision/0006_jp_arith_12_prog.t) |
| JPGP-05 | Sixteen-bit arithmetic lossless point transformation preserves the original high bits. | [tests/loader-jpeg/precision/0013_jp_arith_loss_p4_16_3.t](../../../tests/loader-jpeg/precision/0013_jp_arith_loss_p4_16_3.t) |
| JPGP-06 | Horizontal then vertical hierarchical expansion uses the prescribed integer rounding. | [tests/loader-jpeg/precision/0023_jp_hier_loss_exp17.t](../../../tests/loader-jpeg/precision/0023_jp_hier_loss_exp17.t) |
| JPGP-07 | Arithmetic differential progressive DCT omits DC prediction and the level shift across multiple blocks. | [tests/loader-jpeg/precision/0045_jp_hier_arith_multiblock_prog.t](../../../tests/loader-jpeg/precision/0045_jp_hier_arith_multiblock_prog.t) |
| JPGP-08 | Arithmetic differential lossless samples reconstruct the independent source values. | [tests/loader-jpeg/precision/0026_jp_hier_arith_loss.t](../../../tests/loader-jpeg/precision/0026_jp_hier_arith_loss.t) |
| JPGP-09 | Lossless hierarchy preserves category-16 differences and modulo-65536 wrap. | [tests/loader-jpeg/precision/0032_jp_hier_loss16_wrap.t](../../../tests/loader-jpeg/precision/0032_jp_hier_loss16_wrap.t) |
| JPGP-10 | Separate RG and B initial frames reconstruct final RGB in DHP order. | [tests/loader-jpeg/precision/0033_jp_hier_rgb_groups.t](../../../tests/loader-jpeg/precision/0033_jp_hier_rgb_groups.t) |
| JPGP-11 | Signed intermediate DCT references survive until subsequent refinement. | [tests/loader-jpeg/precision/0039_jp_hier_signed_reference.t](../../../tests/loader-jpeg/precision/0039_jp_hier_signed_reference.t) |
| JPGP-12 | A following frame cannot retroactively change progressive dequantization. | [tests/loader-jpeg/precision/0040_jp_hier_quant_redefinition.t](../../../tests/loader-jpeg/precision/0040_jp_hier_quant_redefinition.t) |
| JPGP-13 | Wide progressive dequantization saturates final samples without short overflow. | [tests/loader-jpeg/precision/0042_jp_arith_wide_prog.t](../../../tests/loader-jpeg/precision/0042_jp_arith_wide_prog.t) |
| JPGP-14 | Plain CMYK arithmetic input reaches builtin CMS with ordinary ink polarity. | [tests/loader-jpeg/precision/0027_jp_arith_plain_cms.t](../../../tests/loader-jpeg/precision/0027_jp_arith_plain_cms.t) |
| JPGP-15 | Hierarchical YCCK reaches builtin CMS from reconstructed device inks. | [tests/loader-jpeg/cmyk/0045_jp_hier_ycck_cms.t](../../../tests/loader-jpeg/cmyk/0045_jp_hier_ycck_cms.t) |

### Lossless Huffman boundary values

The same [process manifest](../../../tests/data/color-numeric/jpeg-process-source.json) records literal SOF3 source samples, precision, point transform, predictor, component layout, restart rows, and encoded difference categories. Expectations retain the original unsigned samples with only the specified low bits cleared. Every output sample, frame size, float representation, and colorspace tag is checked. The [exhaustive inventory](../../testing/builtin-loader-coverage.md#jpeg) includes all seven predictors, 2/12/16-bit boundaries, odd negative half-differences, extreme point transforms, row restarts, Gray/RGB, separate scans, one-row/column images, and DNL; these are value checks distinct from the quality regressions below.

| ID | Contract | Owning test |
| --- | --- | --- |
| JPGS-01 | Category 16 consumes no amplitude bits and preserves subsequent samples. | [tests/loader-jpeg/codestream/0093_sof3_category16_numeric.t](../../../tests/loader-jpeg/codestream/0093_sof3_category16_numeric.t) |
| JPGS-02 | Modulo reconstruction preserves wrap boundaries even without category 16. | [tests/loader-jpeg/codestream/0094_sof3_modulo_numeric.t](../../../tests/loader-jpeg/codestream/0094_sof3_modulo_numeric.t) |
| JPGS-03 | Predictor 5 rounds negative half-differences in point-transformed precision. | [tests/loader-jpeg/codestream/0102_sof3_pt3_pred5_numeric.t](../../../tests/loader-jpeg/codestream/0102_sof3_pt3_pred5_numeric.t) |
| JPGS-04 | Each restart resets the midpoint and the entire first row's horizontal predictor. | [tests/loader-jpeg/codestream/0109_sof3_restart_pt3_numeric.t](../../../tests/loader-jpeg/codestream/0109_sof3_restart_pt3_numeric.t) |
| JPGS-05 | Separate RGB scans maintain independent reconstruction and restart state. | [tests/loader-jpeg/codestream/0113_sof3_planar_restart_pt1_numeric.t](../../../tests/loader-jpeg/codestream/0113_sof3_planar_restart_pt1_numeric.t) |
| JPGS-06 | DNL height resolution preserves sixteen-bit category-16 reconstruction. | [tests/loader-jpeg/codestream/0116_sof3_dnl_zero_numeric.t](../../../tests/loader-jpeg/codestream/0116_sof3_dnl_zero_numeric.t) |

### Quality regression tests

| ID | Quality floor protected | Owning test |
| --- | --- | --- |
| JPGQ-01 | An eligible eight-bit sequential RGB image retains its end-to-end visual floor. | [tests/loader-jpeg/decode/0005_loader_builtin_jpeg_rgb_8bit_seq444_r0_expected_lsqa.t](../../../tests/loader-jpeg/decode/0005_loader_builtin_jpeg_rgb_8bit_seq444_r0_expected_lsqa.t) |
| JPGQ-02 | Adobe YCCK conversion remains visually close to the external reference after encoding. | [tests/loader-jpeg/cmyk/0002_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t](../../../tests/loader-jpeg/cmyk/0002_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t) |
| JPGQ-03 | Embedded ICC conversion remains visually close to the fixed converted PNM after encoding. | [tests/loader-jpeg/decode/0001_builtin_jpeg_embedded_icc_matches_reference_pnm.t](../../../tests/loader-jpeg/decode/0001_builtin_jpeg_embedded_icc_matches_reference_pnm.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| JPG-90 | A corrupt JPEG stream fails the builtin decoder rather than yielding a partial frame. | [tests/loader-jpeg/codestream/0001_loader_builtin_rejects_corrupt_jpeg.t](../../../tests/loader-jpeg/codestream/0001_loader_builtin_rejects_corrupt_jpeg.t) |

Coverage audit note: direct owners now cover sequential RGB bytes, progressive 4:2:0 output, sub-eight-bit Gray16 values, all seven lossless predictors with a point transform, Gray and RGB restart boundaries, YCbCr 4:4:4, CMYK, YCCK, builtin ICC samples, orientation, and malformed inputs. These representatives still do not establish every sampling-factor combination, progressive scan script, restart interval/MCU placement, point transform, or the full Gray/RGB/YCbCr/CMYK/YCCK × precision matrix.

DNL defensive coverage in the [builtin test inventory](../../testing/builtin-loader-coverage.md#jpeg) includes missing/zero/short/truncated markers, wrong placement, duplicate markers, extra or missing MCU rows, early termination at a restart boundary, and truncated amplitude bits. These are parser/value checks, not MS-SSIM quality thresholds.

Arithmetic/hierarchy defensive coverage additionally exercises DAC/DQT bounds and truncation, duplicate components, illegal progressive ordering, restart identity and presence, missing/inconsistent DHP, EXP placement and dimensions, incomplete images, and every allocation failure in representative progressive, predictive, and hierarchical paths. The [inventory](../../testing/builtin-loader-coverage.md#jpeg) keeps these parser/resource observations separate from public value and quality contracts.
