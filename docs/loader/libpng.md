# libpng Image Loader

The `libpng` loader reads PNG and APNG through the external libpng library and libsixel's adapter in [`loader-libpng.c`](../../src/loader-libpng.c). libpng reconstructs PNG scanlines; libsixel chooses the frame representation, applies supported color management, resolves alpha and background, reads Exif orientation, and implements APNG playback. APNG support does not require an APNG-patched libpng.

This document first separates the desired contract from current implementation gaps, then describes the adapter that exists today. A branch in the source or an existing output fixture is not, by itself, evidence that a surprising result was intended. The [image loader architecture](README.md) owns candidate selection and fallback, the [builtin PNG reference](builtin/png.md) describes the independent in-tree decoder, and the [PNG writer](../writers/png.md) covers output files.

## Desired behavior and current gaps

The governing principle is that storage and decoder optimizations may change representation and cost, but must preserve the requested color interpretation, visible coverage, and animation semantics. Selecting libpng should select a PNG codec; it should not silently substitute a different meaning for a shared alpha, background, or CMS option. This is semantic consistency, not a requirement for identical compressed SIXEL bytes or identical rounding across all CMS engines.

The table distinguishes requirements already established by format/shared-policy contracts from proposed precision and architecture improvements. It is a repair target, not a claim that the current implementation passes it. The current [coverage inventory](#test-coverage) records narrower observations separately.

| Area | Desired behavior | Current gap and assessment |
| --- | --- | --- |
| Alpha and CMS | Apply CMS to color samples while carrying alpha unchanged. Resolve alpha according to `-A`; preserve zero-alpha coverage for `clear`/`keep`, and use `keep` when `composite` has no background. | CMS enablement disables the key-color path, after which static composition or APNG alpha stripping can lose coverage. **Conflict with the shared alpha contract.** CMS cannot itself justify making a transparent pixel opaque. |
| Indexed expansion | Carry palette alpha into component alpha or a mask when expanding indices. Changing `reqcolors`, resizing, or disabling palette fusion may change color approximation, but must not expose hidden RGB or discard transparent coverage. | The indexed RGB fallback strips alpha. **Representation-conversion defect.** Retaining an index and expanding that same index must describe the same source pixel before later processing. |
| Background priority | Apply the selected `file_first` or `explicit_first` consistently to the available file/external candidates. Retain an explicit “no background” state. | The static resolver always prefers an external background, and APNG does not use the file-background resolver. **Missing integration of the shared policy.** Inventing black when no background exists also conflicts with transparent fallback. |
| APNG transparency metadata | Decode each rectangle with its applicable palette and transparency metadata. | The scanner recognizes `tRNS` but omits it from reconstructed PNGs. **Metadata propagation defect.** Detecting that a chunk exists is insufficient if its samples never reach the decoder. |
| APNG default image | Count and emit only animation frames; include `IDAT` in animation only when its preceding `fcTL` makes it a frame. | The scanner counts a separate static default image and can reject a valid animation. **Animation-structure defect.** A permissive smoke fixture cannot define the correct count. |
| APNG CMS | Use the same supported source-metadata interpretation as static PNG. Convert color before color-managed frame composition and retain alpha through that work. | The RGBA rectangle decoder omits static CMS processing. **Missing functionality in a CMS-capable loader.** The option currently changes alpha-path selection without establishing the requested color conversion. |
| Color metadata precedence | Use the highest-priority supported, usable interpretation, with explicit recovery for invalid/unsupported metadata. | `iCCP+sRGB+cHRM` disables ICC through a special-case branch. **Legacy behavior requiring correction/audit, not an established design rationale.** The presence of extra chunks must not silently authorize an unmanaged result. |
| Source precision | Retain sixteen-bit distinctions until an explicit precision-reduction boundary. A transparency optimization should not discard visible low bits. | The key-color path and all APNG rectangles truncate to eight bits. **Precision-design gap.** PNG does not dictate the callback storage type; the proposed target follows libsixel's precision-preserving design. |
| Blend arithmetic | Resolve source/background interpretation separately, and compose in linear light with enough precision. Choose the emitted CMS representation after composition. | Retained-palette gamma arithmetic and APNG byte `OVER` differ from static float composition. **Numerical-design gap.** `background_colorspace=gamma` should describe the background's encoding, not implicitly select a lower-accuracy blender. |
| Original-stream integrity | Validate original structure and relevant original CRCs before relying on reconstructed data; do not let new CRCs conceal corruption in the source stream. | APNG reconstructs CRCs without validating the replaced originals; static decode does not finish with `png_read_end()`. **Validation gap.** The exact recoverable-ancillary/error policy needs direct tests rather than a blanket claim of strict validation. |
| Static work and replay cache | Classify static PNG without building an animation canvas. Cache hits, cache misses, and cache exhaustion must preserve pixels, masks, timing, and callback order. | Static PNG incurs the APNG prepass. **Performance defect, separate from pixel correctness.** The replay cache itself is a reasonable optimization; it needs equivalence and boundary coverage. |

### Format rules versus project decisions

PNG defines `iCCP` above `sRGB`, with `cHRM`/`gAMA` below them; supported `cICP` has higher priority. It also prohibits simultaneous `iCCP` and `sRGB`. Such a contradictory file needs an explicit recovery policy, not an invented three-chunk exception. See [color chunk priority](https://www.w3.org/TR/png-3/#4Concepts.ColourSpaces) and [iCCP](https://www.w3.org/TR/png-3/#11iCCP). For this adapter, the proposed best-effort recovery is to use a valid supported ICC profile when available, otherwise a valid lower-priority interpretation, and expose the reason in diagnostics. This proposal must be verified with both Little CMS and no-Little-CMS builds; it does not mean forcibly treating bytes rejected by libpng as trusted profile data.

The PNG [`bKGD` recommendation](https://www.w3.org/TR/png-3/#11bKGD) normally yields to a user-selected background. Thus explicit-background-first behavior is not intrinsically a PNG bug. libsixel additionally offers a user-selectable `file_first` policy; once selected, that application policy should work in every participating PNG path. Changing the project-wide default is a separate decision and is not required to repair libpng's ignored option.

APNG's [default-image rule](https://www.w3.org/TR/png-3/#apng-frame-based-animation) and [`fdAT` semantics](https://www.w3.org/TR/png-3/#fdAT-chunk) govern frame participation and inherited image properties. PNG's [alpha processing guidance](https://www.w3.org/TR/png-3/#13Alpha-channel-processing) supplies the linear-light composition reference. A float canvas is a proposed implementation choice for meeting the project's precision goals; it is not a PNG-mandated C representation. Additional HDR interpretation, arbitrary metadata export, and private `CgBI` support remain separately scoped features rather than prerequisites for fixing SDR PNG/APNG correctness.

### Proposed processing boundaries

1. Read a bounded PNG header and classify static versus animated input. Keep metadata selection and structural validation explicit; do not scan `IDAT` payload bytes for apparent chunk names.
2. Decode source samples with source depth and alpha intact. Share static/APNG color interpretation and background selection policy without forcing both paths into the same storage format.
3. Apply the selected source color transform to color only. Maintain alpha separately; do not treat successful, failed, or disabled CMS as a transparency-policy decision.
4. For animation, compose frame rectangles on a linear canvas with sufficient precision, preserving fractional alpha between frames. Apply `SOURCE`, `OVER`, and disposal there. A binary transparency mask alone is insufficient for that intermediate canvas.
5. On emission, apply orientation consistently, resolve the chosen external/file background according to alpha policy, then convert to the requested typed output. Preserve a mask or transparent index whenever the frame remains transparent. Do not discard fractional alpha before its final composition boundary.
6. Let palette fusion and caching optimize only cases where these semantics are equivalent. If a fast path cannot honor a policy, choose a correct slower path and retain an observable reason.

This decomposition is a design proposal. It does not require exporting a new public RGBA-float API or merging the builtin and libpng codec implementations. The shared policy can be reused while codec reads, buffer ownership, and animation state remain explicit.

### Acceptance criteria and repair order

Fix source meaning and coverage first: APNG frame participation and `tRNS`, alpha preservation across CMS and palette expansion, and background selection. Next establish shared static/APNG color handling and precision-aware composition. Address the independent static prepass cost with representation/metadata invariance checks, and keep integrity/error-boundary work separately reviewable. These concerns should not be folded into one optimization patch.

The regression target should use small direct callback tests with independently calculated values. Existing MS-SSIM floors remain useful integration checks, but cannot establish these invariants:

| Boundary to prove | Required observation |
| --- | --- |
| Alpha across CMS and representations | Exact zero-alpha mask/index coverage with CMS on/off, palette retained/expanded, requested colors above/below `PLTE`, and background present/absent; explicit `clear`, `keep`, and `composite` expectations. |
| Background selection | All combinations of file/external candidates and both priorities; distinct colors make the selected source observable. Verify gamma/linear interpretation numerically. |
| Metadata and source depth | Independent expected colors for supported metadata and explicit invalid-profile recovery; preserve adjacent 16-bit values and compare the full sixteen-bit transparent key before reducing depth. |
| APNG semantics | Both default-image layouts, palette and non-indexed `tRNS`, multiple `IDAT`/`fdAT`, exact canvas pixels/alpha for blend/disposal, zero denominator and delay rounding, start-frame pre-roll, and loop-local numbering. |
| Failure and integrity | Original critical/control/data CRC corruption, truncation, invalid ordering, allocation failure, cancellation, and callback failure; assert status and callback count, including the absence of duplicate fallback output after delivery begins. |
| Fast paths and cache | Equal semantic output for static prepass removal, cache hit/miss, and cache budget exhaustion. Record allocations and phase activity separately from wall-clock timing. |

Retain graceful recovery for optional metadata where the common CMS contract permits it. Do not redefine every recoverable ancillary error as fatal, or turn every invalid APNG into a silent static success. A proposed delivery rule is to allow any documented recovery only before the first callback; after output begins, preserve errors without another decoder appending replacement frames. Enforcing that rule across the loader chain would require a manager integration change, not just editing `load_with_libpng()`.

## Availability and selection

| Build system | Enable explicitly | Disable | Default |
| --- | --- | --- | --- |
| Autotools | `./configure --with-png` or `--with-png=PREFIX` | `--without-png` | Detect automatically. |
| Meson | `meson setup BUILD -Dpng=enabled` | `-Dpng=disabled` | `-Dpng=auto`. |

Run build tools with the repository's `.local/bin` first in `PATH`. The implementations are in [`configure.ac`](../../configure.ac), [`meson.build`](../../meson.build), and [`meson_options.txt`](../../meson_options.txt). Detection defines `HAVE_LIBPNG` and registers `libpng` at the front of the compiled default loader order. Disabling it still leaves the builtin PNG decoder available. The same build option also selects the libpng PNG writer; runtime `-L` selects loaders only.

```sh
# Use only libpng, so another decoder cannot hide its result.
img2sixel -Llibpng! input.png

# Prefer libpng, then allow the remaining compiled default loaders.
img2sixel -Llibpng input.png

# Interpret supported static PNG metadata with builtin CMS.
img2sixel -Llibpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0! input.png

# Extract an animation frame, including preceding canvas updates.
img2sixel -Llibpng! -S --start-frame=1 animation.png
```

Quote a loader argument containing `!` when the interactive shell interprets history expansion. Check `img2sixel --help` for the installed binary's loader list; the presence of a `.png` input or a libpng package on the host does not establish which component decoded it. `SIXEL_TRACE_TOPIC=loader` exposes candidate selection. With an open chain, a libpng decode failure may be followed by a successful builtin or framework decode.

## Image types and the decode boundary

The predicate recognizes the eight-byte PNG signature, not the filename extension. Standard PNG raster decoding is delegated to libpng's traditional read API, including zlib/DEFLATE, row filters, and Adam7 interlace. The [PNG specification](https://www.w3.org/TR/png-3/#6Colour-values) defines these color-type/depth combinations:

| PNG color type | Samples | Bit depths |
| --- | --- | --- |
| 0 | Grayscale | 1, 2, 4, 8, 16 |
| 2 | RGB | 8, 16 |
| 3 | Palette indices with `PLTE` | 1, 2, 4, 8 |
| 4 | Grayscale and alpha | 8, 16 |
| 6 | RGB and alpha | 8, 16 |

Effective support also depends on the linked libpng build. The adapter does not implement builtin's Apple `CgBI` compatibility path. The [libpng manual](https://github.com/pnggroup/libpng/blob/libpng16/libpng-manual.txt) describes the underlying read transforms; it does not define libsixel's CMS or frame contract.

| Stage | Static PNG today | APNG today |
| --- | --- | --- |
| Inspect | Read optional Exif orientation, then run the APNG scanner. | Read optional Exif orientation, then parse animation chunks. |
| Decode | On `SIXEL_FALSE`, enter `load_png()` and select a palette, byte, or float path. | Reconstruct each rectangle as a PNG and decode it to RGBA8. |
| Process | Apply the selected static color/alpha branch and set the typed frame. | Blend/dispose on the RGBA8 canvas and copy it for emission. |
| Deliver | Apply orientation and remaining alpha finalization, then invoke the frame callback. | Apply orientation and alpha finalization to the emitted canvas, then invoke the frame callback. |

`load_with_libpng()` tries the animation scanner before static decode. Only `SIXEL_FALSE` selects `load_png()`; other results return to the caller. This is distinct from the loader manager subsequently choosing another component. An ordinary static PNG currently still incurs APNG canvas allocation, `IDAT` copying, and reconstructed-chunk CRC work before that scanner returns `SIXEL_FALSE`.

### Static frame representation

These are loader callback representations, before encoder preprocessing. The encoder can subsequently expand a palette, convert colorspaces, resize, or separate alpha into a binary mask.

| Effective path | Representation and precision |
| --- | --- |
| Indexed input, palette fusion allowed, and `PLTE` fits the requested color budget | `PAL1`, `PAL2`, `PAL4`, or `PAL8`, with an RGB byte palette. Transparent-index remapping or orientation can expand packed indices to `PAL8`. |
| Indexed input with palette fusion disabled or too many palette entries | Expanded `RGB888`; the encoder constructs a new palette later. |
| Opaque grayscale up to eight bits | `G8` where the palette/color-budget path permits it; otherwise RGB expansion. |
| Opaque RGB8 | `RGB888`. |
| Opaque grayscale/RGB16 | `RGBFLOAT32`, with source values normalized by 65535 before any applicable color conversion. |
| Non-indexed alpha or `tRNS`, key-color path inactive | Background composition into `LINEARRGBFLOAT32`, including eight-bit source images. |
| Non-indexed alpha or `tRNS`, key-color path active | `RGBA8888` with `alpha_zero_is_transparent`; sixteen-bit sources are stripped to eight bits on this path. |
| Successful CMS conversion on an eligible non-indexed path | The selected typed CMS target, subject to the alpha-composition boundary and `prefer_8bit`. |

The palette condition tests the declared palette size against `reqcolors`, not the number of colors visible in the final image. The encoder's palette-fusion decision can also be disabled by resizing, high-color output, or other processing. Palette expansion is therefore a normal branch, not a PNG decoding failure.

`--precision` controls later encoder work; it does not choose the PNG decoder's source precision. Likewise, `prefer_8bit=0` cannot recover low bits already removed by the key-color path, and `prefer_8bit=1` does not universally truncate untransformed RGB16. See [pixel-format precision](../concepts/pixelformat-precision.md).

## Static PNG color management

The CLI defaults to `cms_engine=auto`. The lower-level component constructor starts with CMS disabled and receives resolved options from the loader manager, so a direct component test and a default CLI invocation need not take the same path. `auto` selects an available CMS backend; it does not promise that every profile is supported or that a transform ran. Decoder selection and CMS-engine selection are independent: libpng can use builtin CMS. See [Loader CMS](color-management.md) for engine availability, intent fallback, and unsupported-profile behavior.

The adapter inspects both libpng-exposed metadata and raw chunk-presence flags. Its current ordering includes a legacy exception to a simple “ICC always wins” description; the desired handling is discussed above:

| Metadata condition | Current static interpretation with CMS enabled |
| --- | --- |
| `iCCP`, explicit `sRGB`, and explicit `cHRM` coexist | Suppress the embedded ICC conversion. This source branch does not establish an intended compatibility guarantee. |
| Usable `iCCP` without that coexistence condition | Attempt the embedded profile transform through the selected CMS facilities. |
| `sRGB` without an applied higher-priority transform | Use the sRGB interpretation without an additional source-profile conversion. |
| Applicable `gAMA`, optionally with `cHRM` | Apply transfer conversion and, when usable, chromaticity conversion. The Little CMS build uses a constructed profile; the no-Little-CMS branch also has explicit gamma/matrix helpers. |
| `cHRM` alone, absent metadata, or unusable conversion | No general guarantee of conversion to the requested target. Retain the path's fallback interpretation. |

The Little CMS and no-Little-CMS branches are not identical implementations, particularly for unusable metadata. Do not infer exact cross-engine parity from the precedence table. The metadata matrix tests below protect perceptual reference floors, not every malformed-profile or contradictory-chunk outcome.

On retained indexed paths, applicable CMS work changes the byte palette rather than expanding every index into a float pixel. On non-indexed alpha paths, source and applicable file-background colors are prepared for linear-light composition, and the resulting frame remains `LINEARRGBFLOAT32`; requesting a perceptual CMS target does not turn that final compositor into a Lab or Oklab blender. A target request alone also does not relabel a no-op `sRGB` source.

## Transparency and background

### Key-color selection

`trns_keycolor=0|1` (`K0`/`K1`) defaults to `1`; its environment equivalent is `SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR`. Despite the historical name, the enabled mode covers ordinary alpha-channel PNGs as well as `tRNS`. The fast path is gated off whenever CMS is enabled or an external background reaches the component. Thus the current CLI CMS default normally disables this path even though the key-color option itself defaults to enabled.

For a non-indexed image, the active path preserves RGBA8 and marks exact alpha zero for downstream omission. For a retained indexed image, the first fully transparent palette entry becomes the transparent index; additional fully transparent indices are remapped to it. Partial palette alpha is composited into palette colors. A palette that no longer fits the color budget instead takes the RGB expansion branch, which strips alpha; it does not retain the indexed transparent-key contract.

The option is not the SIXEL alpha policy. [`-A auto|composite|clear|keep`](alpha-policy.md) governs the later omission and `P2` request. Fractional alpha is not transmitted in SIXEL. Tests named “opt-in” or “cms=1” may predate the current defaults; inspect their explicit engine and key-color arguments rather than deriving defaults from their filenames.

### Source selection and arithmetic

For static PNG, `png_resolve_background_unit()` chooses an external background passed to the component first, then `bKGD`, then black. It does not consult `background_policy`. Consequently, the common `background_policy=file_first` option does not make this adapter prefer `bKGD` over a supplied background. The [background policy](background-policy.md) documents the shared control; its builtin source-priority behavior must not be assumed here.

`background_colorspace=gamma|linear` defaults to `gamma`. On non-indexed static alpha paths, composition is in linear light for either value; the option changes the interpretation of the background samples. For example, a supplied `#808080` denotes an sRGB-encoded value under `gamma` and a linear value of `128/255` under `linear`. On retained indexed paths, `gamma` uses byte palette arithmetic, while `linear` decodes the foreground, blends in linear light, and encodes back to byte palette values.

There are current transparency limitations. With key-color disabled and no resolved background, the non-indexed static compositor can flatten fully transparent pixels to black rather than retaining a mask. Indexed expansion also differs from indexed retention. These behaviors are implementation limitations, not desired guarantees of the common alpha policy. The [builtin/libpng comparison](builtin/png.md#background-source-interpretation-and-blend-arithmetic) records controlled examples and separates source selection, blend arithmetic, and coverage loss.

## Metadata beyond color

| Metadata | Adapter use |
| --- | --- |
| `PLTE` and `tRNS` | Static palette, palette alpha, or grayscale/RGB transparent key, subject to the representation branches above. |
| `bKGD` | Static background candidate; APNG emission does not use the static background resolver. |
| `eXIf` | Parse TIFF/Exif orientation from the chunk payload. Apply values 2 through 8 when orientation is enabled; absent/unusable orientation leaves the default orientation. |
| `acTL`, `fcTL`, `fdAT` | APNG control, frame rectangles, timing, and compressed frame data. |
| Text, timestamps, physical resolution, and other descriptive metadata | No general metadata object is returned to the frame callback. |
| `cICP`, `mDCv`, `cLLi`, and HDR controls | No dedicated HDR interpretation in this adapter, even if the linked libpng can expose these chunks. |

`orientation=0|1` (`O0`/`O1`) defaults to `1`. `SIXEL_LOADER_LIBPNG_ORIENTATION` supplies a backend-specific environment default, with `SIXEL_LOADER_ORIENTATION` as the common fallback; an explicit suboption wins. Orientation is applied to the decoded static frame or completed APNG canvas. Packed palette/grayscale storage is expanded when necessary before rotation or reflection. The existing orientation test exercises orientation 6; it does not numerically establish every Exif transform.

## APNG playback and limitations

`load_apng_frames()` implements animation around ordinary libpng image reads. It saves shared chunks, rewrites the frame `IHDR` dimensions, converts `fdAT` payloads into `IDAT`, supplies CRCs for reconstructed chunks, and decodes each rectangle with `decode_png_rgba()`.

| Concern | Current behavior |
| --- | --- |
| Canvas precision | RGBA8 throughout rectangle decode and composition. Sixteen-bit samples are stripped to eight bits; later float conversion cannot restore them. |
| Blend | `SOURCE` replaces the rectangle; `OVER` uses integer straight-alpha arithmetic on byte component values. It is separate from static PNG's linear-light background composition. |
| Disposal | `NONE` retains the canvas, `BACKGROUND` clears the rectangle to transparent zero, and `PREVIOUS` restores the saved canvas after emitting the current frame. |
| Frame callback | Emit a complete canvas with delay, frame number, loop number, and multiframe state, after orientation and applicable alpha finalization. |
| Timing | Convert the fraction in seconds to integer centiseconds. A zero denominator uses 100; a positive delay that rounds to zero is raised to one centisecond. `-g` controls presentation waiting later. |
| Looping | `-l disable` delivers one pass; `-l auto` uses the declared play count, with zero meaning indefinite replay. A one-frame animation stops after one pass. |
| Static extraction | `-S` stops after the selected frame is emitted; it still uses the animation compositor. It does not necessarily validate the rest of the animation. |
| Start frame | `--start-frame` / `-T` overrides `SIXEL_LOADER_ANIMATION_START_FRAME_NO`. Nonnegative indices count from the beginning and negative indices from the declared end. Earlier frames still update the canvas. The first loop skips their callbacks; subsequent loops replay from the beginning. |
| Frame numbering | Emitted numbering begins at zero and is local to each loop, including a first loop that skipped source frames. |
| Ordinary static PNG | Animation start-frame values are resolved lazily after `acTL`, so static input ignores invalid or out-of-range animation environment settings. |

Several boundaries differ from the static path and from the builtin APNG implementation:

- **Color management:** `decode_png_rgba()` does not run the static ICC/gamma/chromaticity conversion pipeline. `enable_cms` still gates the key-color behavior, so equal output with key-color enabled/disabled under CMS does not demonstrate an APNG color transform.
- **Palette and transparency:** APNG ignores palette fusion and requested colors during rectangle decode. Its scanner records `tRNS` for key-color gating but does not add that chunk to the reconstructed shared-chunk buffer. Static `tRNS` pixel semantics must therefore not be assumed for APNG.
- **File background:** the emitted canvas is finalized with the external background; the static `bKGD` resolver is not called. Disposal-to-background means transparent clearing, not painting the PNG file's background color.
- **Default image:** the current scanner counts and emits an initial `IDAT` image even when no `fcTL` preceded it. A standards-conforming APNG can exclude that static default image from the animation count; this adapter can consequently reject such a file on a count mismatch. The existing “default image first” smoke fixture demonstrates acceptance of its particular layout, not correct handling of every default-image exclusion case.
- **Validation timing:** sequence, rectangle, operation, and frame-count checks happen while frames are processed. A later error can occur after callbacks have already emitted output. Successful `-S` extraction is not whole-file APNG validation.

These are limitations of libsixel's APNG adapter, not evidence that the external libpng codec implements the same policy.

## Integrity, resources, and performance

Static raster reads retain libpng's default CRC behavior; the adapter does not call `png_set_crc_action()` to change it. However, it calls `png_read_image()` without `png_read_end()`, and its independent raw scans do not verify stored CRCs. In APNG, reconstructed `IHDR`/`IDAT` CRCs are newly calculated, while copied shared chunks keep their original CRCs. Successful decoding therefore must not be described as validation of every original PNG/APNG chunk checksum.

The component consumes an in-memory input chunk and allocates raster buffers and row pointers. APNG also needs a canvas, a backup canvas, reconstructed compressed data, a subframe, and emitted frames. Its optional replay cache budgets retained frame storage at 64 MiB and disables itself if a frame cannot be retained within that budget; later loops can decode again. This is an internal cache budget, not a total-memory limit or a public `cache_max_bytes` option for libpng. The adapter has no equivalent of libwebp's `max_output_frames` suboption.

libpng errors are reported through the PNG error status and additional diagnostic text. APNG structural checks can report `SIXEL_BAD_INPUT`; allocation, cancellation, and overflow have separate statuses. The [loader manager's fallback classification](README.md#candidate-selection-and-fallback) determines which failures permit another component. Neither a closed chain nor the cache budget is an isolation mechanism.

For profiling, separate libpng raster decode from libsixel's chunk reconstruction, CRC computation, allocation, CMS, composition, and encoder work. The [recorded PNG loader measurements](builtin/png.md#measured-static-png-cost) show why complete-loader timings cannot establish a codec ranking. They are measurements at their recorded revision, not a current performance guarantee. `SIXEL_TRACE_TOPIC=apng_decode` exposes the scanner's chunk, sequence, timing, and loop decisions; it also exposes the static PNG prepass.

## Implementation landmarks

| Concern | Owning implementation |
| --- | --- |
| Component selection and options | `loader_can_try_libpng()`, `sixel_loader_libpng_setopt()`, and `load_with_libpng()` in [`loader-libpng.c`](../../src/loader-libpng.c); option spellings in [`options-registry.c`](../../src/options-registry.c). |
| Static transforms and precision | `load_png()`, `read_palette()`, `png_convert_rgb16_rows_to_rgbfloat32()`, and the `png_convert_*` / `png_apply_*` helpers in the same file. |
| Background source and orientation | `png_resolve_background_unit()` and `libpng_parse_exif_orientation()`; common transforms in [`loader-common.c`](../../src/loader-common.c). |
| Animation reconstruction and delivery | `load_apng_frames()`, `append_chunk()`, `decode_png_rgba()`, `apng_blend_rect()`, `emit_apng_frame()`, and `apng_replay_cache_*()` in [`loader-libpng.c`](../../src/loader-libpng.c). |
| CMS implementation and contracts | [Loader CMS](color-management.md), [builtin CMS architecture](builtin-cms.md), and [`cms.c`](../../src/cms.c). |

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The observations below distinguish direct frame assertions, planner/trace assertions, and encoded-output comparisons. They do not imply exhaustive PNG conformance.

| ID | Observation protected | Owning test |
| --- | --- | --- |
| LP-01 | Direct component callbacks report RGBA8 for the unmanaged alpha fixture and float RGB for the gray16 fixture. | [tests/loader/libpng/0009_loader_libpng_pixelformat.t](../../tests/loader/libpng/0009_loader_libpng_pixelformat.t) |
| LP-02 | A direct indexed loader probe expands to RGB when the requested budget is smaller than `PLTE`. | [tests/loader/libpng/0143_loader_libpng_indexed_png_reqcolors_fallback.t](../../tests/loader/libpng/0143_loader_libpng_indexed_png_reqcolors_fallback.t) |
| LP-03 | The unmanaged RGB16/sRGB fixture reaches the encoder planner as `rgb-f32` and keeps float working storage. | [tests/loader/libpng/0142_libpng_rgb16_srgb_only_float32_pipeline.t](../../tests/loader/libpng/0142_libpng_rgb16_srgb_only_float32_pipeline.t) |
| LP-04 | Default key-color selection changes unmanaged indexed output relative to explicit disable. | [tests/loader/libpng/0195_loader_libpng_trns_keycolor_default_enabled_for_palette.t](../../tests/loader/libpng/0195_loader_libpng_trns_keycolor_default_enabled_for_palette.t) |
| LP-05 | Under `cms_engine=auto`, toggling key-color produces identical SIXEL for the gray-key, RGBA16, and APNG fixtures. This observes gating, not successful CMS transformation. | [tests/loader/libpng/0156_loader_libpng_trns_keycolor_gating_conditions.t](../../tests/loader/libpng/0156_loader_libpng_trns_keycolor_gating_conditions.t), [tests/loader/libpng/0204_loader_libpng_trns_keycolor_rgba16_cms_auto_gating.t](../../tests/loader/libpng/0204_loader_libpng_trns_keycolor_rgba16_cms_auto_gating.t), [tests/loader/libpng/0198_apng_libpng_trns_keycolor_cms_gating.t](../../tests/loader/libpng/0198_apng_libpng_trns_keycolor_cms_gating.t) |
| LP-06 | Invalid and empty key-color environment values retain the enabled default's encoded output. | [tests/loader/libpng/0154_loader_libpng_trns_keycolor_env_invalid_defaults_enabled.t](../../tests/loader/libpng/0154_loader_libpng_trns_keycolor_env_invalid_defaults_enabled.t), [tests/loader/libpng/0211_loader_libpng_trns_keycolor_env_empty_matches_default.t](../../tests/loader/libpng/0211_loader_libpng_trns_keycolor_env_empty_matches_default.t) |
| LP-07 | An explicit loader key-color suboption overrides the environment. | [tests/loader/libpng/0161_loader_libpng_trns_keycolor_cli_env_override.t](../../tests/loader/libpng/0161_loader_libpng_trns_keycolor_cli_env_override.t) |
| LP-08 | Omitted background colorspace matches explicit gamma, while explicit linear changes the indexed transparency fixture. | [tests/loader/libpng/0145_loader_libpng_background_colorspace_default_equals_gamma.t](../../tests/loader/libpng/0145_loader_libpng_background_colorspace_default_equals_gamma.t), [tests/loader/libpng/0146_loader_libpng_background_colorspace_linear_changes_indexed_trns.t](../../tests/loader/libpng/0146_loader_libpng_background_colorspace_linear_changes_indexed_trns.t) |
| LP-09 | Orientation 6 changes static/APNG output; the static default matches enabled, and an untagged static image is unchanged by the switch. | [tests/loader/libpng/0213_loader_libpng_exif_orientation_static_and_apng.t](../../tests/loader/libpng/0213_loader_libpng_exif_orientation_static_and_apng.t) |
| LP-10 | Static PNG output is unchanged by invalid/out-of-range animation environment values or a CLI start-frame request. | [tests/loader/libpng/0216_loader_libpng_static_ignores_invalid_start_frame_env.t](../../tests/loader/libpng/0216_loader_libpng_static_ignores_invalid_start_frame_env.t), [tests/loader/libpng/0217_loader_libpng_static_ignores_start_frame_cli.t](../../tests/loader/libpng/0217_loader_libpng_static_ignores_start_frame_cli.t), [tests/loader/libpng/0218_loader_libpng_static_ignores_out_of_range_start_frame_env.t](../../tests/loader/libpng/0218_loader_libpng_static_ignores_out_of_range_start_frame_env.t) |
| LP-11 | Trace output reports the exact loop-local frame-number sequence for normal playback and first-loop start-frame selection. | [tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t](../../tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t), [tests/loader/libpng/0197_apng_libpng_start_frame_keycolor_sequence.t](../../tests/loader/libpng/0197_apng_libpng_start_frame_keycolor_sequence.t) |
| LP-12 | Switching builtin CMS rendering intent changes encoded output for an ICC fixture with distinct A2B intent tables. | [tests/loader/libpng/0215_loader_libpng_png_rgb_mab_a2b012_intent_switch_builtin.t](../../tests/loader/libpng/0215_loader_libpng_png_rgb_mab_a2b012_intent_switch_builtin.t) |

### Quality regression tests

| ID | Quality observation | Owning test |
| --- | --- | --- |
| LPQ-01 | PNGSuite RGBA8 and RGBA16 preserve their end-to-end MS-SSIM floors. | [tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t](../../tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t), [tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t](../../tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t) |
| LPQ-02 | Extracted APNG frames using `OVER` or background disposal stay at MS-SSIM 0.98 against their static references. | [tests/loader/libpng/0006_apng_libpng_blend_over.t](../../tests/loader/libpng/0006_apng_libpng_blend_over.t), [tests/loader/libpng/0007_apng_libpng_dispose_background.t](../../tests/loader/libpng/0007_apng_libpng_dispose_background.t) |
| LPQ-03 | Multiple zero-alpha palette entries and a normalized single-zero fixture produce matching MS-SSIM 1.0 output with CMS disabled; this is not a direct index-map assertion. | [tests/loader/libpng/0172_loader_libpng_palette_trns_multi_zero_remap_icc.t](../../tests/loader/libpng/0172_loader_libpng_palette_trns_multi_zero_remap_icc.t) |

### Defensive and malformed-input tests

The [libpng test category](../../tests/loader/libpng) also covers invalid APNG counts, rectangles, blend/disposal values, sequence gaps, and ordering. Supplementary smoke cases exercise static extraction, finite replay, zero delay denominators, `PREVIOUS` disposal, and shared indexed chunks. Their completion assertions do not prove exact pixel composition, delay rounding, or standards-wide acceptance. The same category contains PNGSuite shape/resize/background cases, grayscale/indexed/RGB color-metadata combinations, and additional key-color integration matrices.

### Coverage boundary

Run these tests in a build with `HAVE_LIBPNG=1`; a skip in a builtin-only build is not libpng validation. Tests using `img2sixel` or `lsqa` also require those programs. Most backend tests close the chain with `-Llibpng!` to avoid fallback masking a result.

The direct tests above establish selected frame types and palette expansion, not exhaustive exact-sample support for every PNG color type, bit depth, or interlace mode. The color-metadata matrices use MS-SSIM thresholds rather than exact ICC or transfer-function results. APNG CRC coverage, default-image exclusion, `tRNS` reconstruction, all eight orientation values, exact disposal/timing arithmetic, and replay-cache limits do not have complete direct behavioral coverage here. The implementation limitations described above must not be promoted to compatibility guarantees merely because an adjacent smoke or quality test passes.
