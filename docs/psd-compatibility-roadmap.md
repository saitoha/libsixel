# PSD Compatibility Roadmap (Builtin Loader)

This document defines how `src/loader-builtin.c` will move from the current
`stb_image`-limited PSD path to a spec-aligned composite-image decoder.

## Scope

- In scope:
  - PSD (`8BPS`, version 1) composite image decode for single-frame inputs.
  - PSB header alias (`8BPB`, version 2) with parser-compatible decode paths:
    - composite-image decode (existing PSD matrix), and
    - missing-composite layer fallback where layer layout is compatible with the
      current fallback parser/compositor.
  - Feature parity for "basic subformats": color mode, bit depth, compression,
    alpha/background behavior, and embedded ICC usage.
- Out of scope for initial milestone:
  - Full Photoshop-parity layered reconstruction (non-pixel semantics beyond
    current fill support, vector/effects/knockout full semantics).
  - PSB large-document parity (full large-size/section coverage beyond the
    current parser/fallback surface).

## Normative Reference

- Adobe Photoshop File Formats Specification (Nov 2019):
  - <https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/>

Key points used by this roadmap:

- Header supports channels `1..56`, depth `1/8/16/32`, color modes
  `Bitmap/Grayscale/Indexed/RGB/CMYK/Multichannel/Duotone/Lab`.
- Image data compression supports `0:Raw`, `1:RLE`, `2:ZIP`,
  `3:ZIP with prediction`.
- ICC profile is an image resource (`0x040F`, decimal `1039`).
- If merged/composite image is missing (Maximize Compatibility off), layer data
  must be interpreted to reproduce final output.

## Current State (Snapshot)

- Decoder path is `src/frompsd.c` custom composite decoder (no PSD decode
  fallback to `stb_image`).
- Signature/version policy:
  - `8BPS` supports version `1/2` per existing parser policy.
  - `8BPB` is accepted only with `version=2`; `8BPB+version!=2` is rejected as
    malformed header.
- Header/metadata validation is centralized with explicit policy:
  - channels `1..56`
  - width/height `1..300000`
  - unsupported vs malformed diagnostics are separated
- Supported matrix (current implementation):
  - Bitmap 1-bit: Raw/RLE/ZIP (`ZIP+Prediction` is explicit unsupported)
  - Gray/Duotone 8-bit: Raw/RLE/ZIP/ZIP+Prediction
  - Gray/Duotone 16-bit: Raw/RLE/ZIP/ZIP+Prediction (`RGBFLOAT32`)
  - Gray/Duotone 32-bit: Raw/RLE/ZIP/ZIP+Prediction (`RGBFLOAT32`)
  - Indexed 8-bit: Raw/RLE/ZIP/ZIP+Prediction
  - RGB 8/16-bit: Raw/RLE/ZIP/ZIP+Prediction
  - RGB 32-bit: Raw/RLE/ZIP/ZIP+Prediction (`RGBFLOAT32`)
  - CMYK 8/16/32-bit: Raw/RLE/ZIP/ZIP+Prediction
    (`LINEARRGBFLOAT32` for 16/32-bit; ICC applied by output format policy)
  - Lab 8/16/32-bit: Raw/RLE/ZIP/ZIP+Prediction
    (`CIELABFLOAT32`; ICC applied on CIELAB path when backend supports it)
  - Multichannel (mode 7):
    - `channels==3`: mapped to RGB decode mode
    - `channels==4`: mapped to CMYK decode mode
    - all mapped paths support 8/16/32-bit + Raw/RLE/ZIP/ZIP+Prediction
- Explicit unsupported policy (fixed):
  - Multichannel channel counts other than `3/4`
  - Multichannel bit depths other than `8/16/32`
  - Bitmap 1-bit with ZIP+Prediction
- Composite-missing policy:
  - Pixel-layer fallback is supported for 8/16/32-bit RGB/Gray/Duotone/Lab/CMYK
    layer-only PSDs with merged/composite image missing.
    The fallback parser/compositor handles multi-layer stacking with:
    - layer geometry offsets,
    - blend-key mapping (known keys only),
    - per-layer opacity/visibility,
    - clipping groups (base + clipped stack),
    - raster mask channels (`-2`/`-3`) and extra alpha (`-1`).
  - For Multichannel (`mode=7`), fallback applies to mapped paths only:
    `channels==3` (`3ch->RGB8/16/32`) and
    `channels==4` (`4ch->CMYK8/16/32`).
  - Layer-only layouts outside this fallback surface a deterministic unsupported
    trace (`unsupported layer fallback layout`).
  - The same fallback policy now applies to PSB (`8BPB+version=2`) inputs when
    layer/mask structures are parser-compatible; malformed PSB layer metadata
    is reported as malformed (not silently downgraded).
  - PSB malformed boundary diagnostics are split into fixed trace contracts for:
    `layer/mask length`, `layer info length`, `layer channel length`,
    `RLE row table (too short)`, and `RLE row length`.
  - Non-pixel payload is tolerated in fallback:
    - layers with decodable pixel channels are composited normally
      (non-pixel payload ignored, info trace), and
    - layers without decodable pixel channels:
      - render synthetic fill for `SoCo` (descriptor/SXFL; descriptor color
        object supports RGB, CMYK, Grayscale, HSB, and Lab classes),
        `GdFl` (descriptor/SXFL, including nested `Grad` object form and
        descriptor color objects for RGB/CMYK/Grayscale/HSB/Lab), and
        `PtFl` (descriptor/SXFL; descriptor color objects for
        RGB/CMYK/Grayscale/HSB/Lab) payloads, or
      - skip malformed fill-descriptor payloads (`SoCo/GdFl/PtFl`) with
        deterministic trace (`malformed non-pixel fill payload; skipping layer`), or
      - skip layer when no supported fill payload is present.
  - Vector mask, layer effects, knockout, and unknown blend key use
    deterministic degrade behavior in fallback:
    - vector/effects/knockout are ignored with info trace, and
    - unknown blend key falls back to `Normal` with info trace.
  - When image data exists but raw/RLE payload is too short, return malformed
    (do not conflate truncation with layer-only PSD policy).
- Existing regression includes ICC and alpha combinations on Raw/ZIP/ZIP+Prediction
  paths, 32-bit decode matrix coverage including RLE (RGB/Gray/CMYK/Lab),
  CMYK16/Lab16 decode coverage, and mode7 coverage across mapped paths:
  - `3ch->RGB` and `4ch->CMYK` for `8/16/32-bit`
  - `Raw/RLE/ZIP/ZIP+Prediction` decode regressions
  - policy traces for invalid channel-count/depth combinations
  - CMYK-mapped mode7 ICC trace coverage (valid profile no-false-failure,
    invalid profile, and malformed resource section), including
    missing-composite multi-layer normal path for `4ch->CMYK8/16/32`
    (`CMYK8` bad-profile failure trace, `CMYK16/32` bad-profile decode without
    malformed-resource misclassification, malformed-resource trace for all).
  - Native CMYK (`mode=4`) missing-composite multi-layer ICC trace coverage
    with the same contract as mode7-mapped CMYK paths:
    valid profile no-false-failure, `CMYK8` bad-profile failure trace,
    `CMYK16/32` bad-profile decode without malformed-resource misclassification,
    and malformed-resource trace for `8/16/32`.
  - mode7 missing-composite policy traces across mapped depth variants
    (`RGB8/16/32`, `CMYK8/16/32`)
  - multi-layer missing-composite coverage for RGB8:
    normal blend decode, clipping-group decode, raster-mask decode,
    informational degrade traces for unknown blend/vector
    mask/layer effects/knockout, and non-pixel trace coverage
    (pixel-present ignore, no-pixel skip, and fill render cases).
  - multi-layer missing-composite normal-blend decode coverage for
    RGB16/RGB32/Lab16/CMYK8/CMYK16/CMYK32.
  - multi-layer missing-composite clipping-group and raster-mask decode
    coverage for RGB16/RGB32/CMYK8/CMYK16/CMYK32.
  - multi-layer missing-composite degrade trace coverage for
    CMYK8/CMYK16/CMYK32:
    unknown blend/vector mask/layer effects/knockout.
  - mode7 multi-layer missing-composite decode coverage:
    `3ch->RGB16/RGB32` and `4ch->CMYK16/CMYK32` for normal, clipping-group,
    and raster-mask paths.
  - mode7 multi-layer degrade trace coverage:
    unknown blend/vector mask/layer effects/knockout
    (RGB8 and CMYK8/CMYK16/CMYK32 mapped paths).
  - mode7 multi-layer non-pixel trace coverage
    (pixel-present ignore, no-pixel skip, and fill render cases).
  - TySh EngineData `FillColor [...]` fallback parsing now complements
    descriptor/SXFL paths for no-pixel non-pixel layers
    (CMYK8 and mode7-mapped CMYK8 coverage).
  - descriptor fill LSQA coverage now includes:
    - `SoCo` descriptor color-object paths (`RGB/CMYK/Grayscale/HSB/Lab`)
      across `RGB8`, `CMYK8`, and `mode7(4ch->CMYK8)` fallback surfaces,
    - `GdFl` descriptor stop-color paths (`CMYK/Grayscale/HSB/Lab`) and
      nested `Grad` form on the same surfaces, and
    - `PtFl` descriptor color-object paths (`CMYK/Grayscale/HSB/Lab`) on the
      same surfaces.
  - PSB (`8BPB+version=2`) non-pixel + ICC parity coverage now includes
    native `mode=4` CMYK and mode7-mapped CMYK (`channels=4`) paths for
    `8/16/32bpc` missing-composite multi-layer fallback:
    - valid ICC no-false-failure traces across TySh descriptor/wrapped and
      fill descriptor (`SoCo/GdFl/PtFl`) matrices,
    - bad ICC rotation contract (`CMYK8` keeps conversion-failure trace;
      `CMYK16/32` avoid malformed-resource misclassification), and
    - malformed resource rotation contract (deterministic malformed-resource
      trace with decode success).
  - PSB missing-composite non-pixel decode coverage also includes
    TySh EngineData `FillColor [...]` no-pixel fallback surfaces for
    native CMYK8 and mode7-mapped CMYK8 paths.
  - descriptor malformed regression now includes:
    - structurally malformed fill additional-block length contracts
      (`malformed layer extra data`), and
    - descriptor-invalid payload contracts that keep decode success with
      deterministic skip trace (`malformed non-pixel fill payload; skipping layer`)
      on both PSD and PSB RGB8 fallback paths (`SoCo/GdFl/PtFl`).
- Validation trace coverage includes:
  - unsupported bit-depth traces for Bitmap and Grayscale/Duotone `%s` path,
  - mode-specific malformed channel-count traces (`RGB/CMYK/Lab` minimums),
  - Multichannel policy traces for unsupported channel counts (`!=3/4`) and
    depth policy on both mapped paths (`3ch->RGB`, `4ch->CMYK`),
  - positive trace for missing merged/composite image policy.
- Defensive validate branches are split by reachability:
  - unit-tested directly: `malformed header/metadata`,
    `malformed image data offset` (plus `dimensions/depth overflow` on 32-bit),
  - documented as policy-bounded defensive guards:
    `raw plane size overflow`, `RLE row table overflow`
    (current `channels<=56`, `dimension<=300000` bounds make them unreachable).
- Alpha/background policy (implemented):
  - `--bgcolor` specified: decode path composites against background and does
    not emit a transparent mask.
  - `--bgcolor` omitted and alpha extra channel exists: decode output remains
    3ch (`RGB888` or existing float32 3ch), and transparency is stored in
    `sixel_frame.transparent_mask`.
  - When extra channels are multiple, the first extra channel is interpreted as
    alpha; remaining extra channels are ignored.
  - Key transparency targets only `alpha == 0`; partial alpha is precomposited
    against black before storing 3ch color.
  - Embedded ICC conversion applies to RGB channels only; transparency mask
    bytes are preserved unchanged.

## Target Compatibility Levels

### Level 1 (P1): Basic Composite Decode Parity

- PSD header and section parsing without relying on `stb_image` PSD parser.
- Composite decode support for color modes:
  - RGB, Grayscale, Indexed, CMYK, Lab
  - (Bitmap and Multichannel as explicit policy: support or clear error)
- Depth support:
  - 1, 8, 16, 32 (with explicit conversion rules to internal formats)
- Compression support:
  - Raw, RLE, ZIP, ZIP+Prediction
- Consistent alpha/background handling:
  - implemented with `3ch + transparent mask` output when `--bgcolor` is not
    provided, and opaque compositing when `--bgcolor` is provided.

### Level 2 (P2): Color Management and Diagnostics

- ICC application for all supported PSD pixel paths (including alpha-safe paths).
- Distinguish `ABSENT` vs `MALFORMED` resources consistently.
- Trace/additional message policy for unsupported features and decode fallback.

### Level 3 (P3): Robustness and Extended Compatibility

- Extend layer fallback beyond the current non-pixel payload ignore policy
  toward semantic support for non-pixel layer classes, plus
  vector/effects/knockout semantics, for higher Photoshop parity.
- PSB planning and parser extension.
- Performance tuning and memory-reuse optimization.

## Execution Plan

### Phase A: Parser Foundation

1. Add PSD section parser (header + section boundaries + compression metadata).
2. Centralize validation and reason codes (invalid signature/version, overflow,
   malformed section lengths, unsupported combinations).
3. Keep existing RGB path as fallback while parser is introduced.

Definition of done:

- Parser unit/integration tests for malformed length/offset cases.
- Existing PSD tests remain green.

### Phase B: Composite Plane Decoder (Raw/RLE)

1. Implement channel-plane reader for Raw and RLE.
2. Implement mode-specific packers to internal pixels:
   - RGB/RGBA
   - Gray/Gray+alpha
   - Indexed(+palette from Color Mode Data)
   - CMYK/Lab conversions to RGB family
3. Preserve extra channel policy (e.g. first transparency channel) explicitly.

Definition of done:

- RGB no longer depends on `stbi__psd_load`.
- New fixtures for Gray/Indexed/CMYK/Lab with Raw/RLE pass.

### Phase C: ZIP and ZIP+Prediction

1. Add deflate decode for composite image channel streams.
2. Implement PSD prediction undo where required.
3. Extend same mode/depth matrix for compression `2/3`.

Definition of done:

- Compression matrix (`0/1/2/3`) passes for supported color modes/depths.

### Phase D: ICC and Alpha Coherence

1. Apply ICC conversion by output format policy (`RGB`, `LINEARRGB`,
   `CIELAB`) and never mutate transparent masks.
2. Keep alpha policy stable:
   - no `--bgcolor`: `3ch + transparent mask`, key transparency at encode.
   - with `--bgcolor`: opaque composite output.
3. Continue regression coverage for `--bgcolor` + ICC and no-`--bgcolor` alpha
   keycolor paths.

Definition of done:

- No known `embedded ICC conversion failed` false positives for valid profiles.
- Alpha-sensitive visual regressions guarded by tests.

### Phase E: Composite-Missing Policy

1. Detect missing merged/composite image.
2. Decide policy:
   - explicit unsupported error (for unsupported layouts), and
   - multi-layer pixel reconstruction with deterministic degrade policy for
     partial non-pixel semantics (fill support + info traces).
3. Document behavior in README/changelog.

Definition of done:

- Behavior is deterministic and tested.

## Test Matrix (Required)

For each supported color mode and depth:

- Compression: Raw/RLE/ZIP/ZIP+Prediction
- Channels: base color only and base+alpha
- ICC: none/valid/invalid/malformed-resource
- Output conditions: with and without `--bgcolor`

Minimum fixture naming convention:

- `psd_<mode>_<depth>_<comp>_<alpha>_<icc>.psd`

## Immediate Next Tasks (Start Here)

1. Extend non-pixel semantics beyond current fill payload support while
   preserving deterministic degrade traces for unsupported semantics
   (for example richer `TySh` payload interpretation beyond color-descriptor fill).
2. Extend PSB (`8BPB+version=2`) from parser-compatible partial support toward
   large-document parity:
   - broaden missing-composite matrix/trace coverage to larger layouts, and
   - design/implement large-size section boundary handling beyond the current
     parser/fallback surface.
3. Increase cross-mode visual quality regression density for non-pixel payload
   rendering (LSQA) while keeping current ICC/trace contracts fixed.
