# PSD Compatibility Roadmap (Builtin Loader)

This document defines how `src/loader-builtin.c` will move from the current
`stb_image`-limited PSD path to a spec-aligned composite-image decoder.

## Scope

- In scope:
  - PSD (`8BPS`, version 1) composite image decode for single-frame inputs.
  - Feature parity for "basic subformats": color mode, bit depth, compression,
    alpha/background behavior, and embedded ICC usage.
- Out of scope for initial milestone:
- Full layered reconstruction when merged/composite image is absent
  (beyond minimal single-layer 8-bit RGB/Gray/Duotone fallback).
  - PSB (`8BPB`, version 2) large-document support.

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
    (`LINEARRGBFLOAT32` for 16/32-bit; ICC skipped for non-RGB outputs)
  - Lab 8/16/32-bit: Raw/RLE/ZIP/ZIP+Prediction (`CIELABFLOAT32`, ICC skipped)
  - Multichannel (mode 7):
    - `channels==3`: mapped to RGB decode mode
    - `channels==4`: mapped to CMYK decode mode
    - all mapped paths support 8/16/32-bit + Raw/RLE/ZIP/ZIP+Prediction
- Explicit unsupported policy (fixed):
  - Multichannel channel counts other than `3/4`
  - Multichannel bit depths other than `8/16/32`
  - Bitmap 1-bit with ZIP+Prediction
- Composite-missing policy:
  - Minimal fallback is supported for 8/16-bit RGB/Gray/Duotone layer-only PSDs:
    single full-canvas layer with decodable base-color channels.
  - Layer-only PSD layouts outside that minimal fallback surface a deterministic
    unsupported trace (`unsupported layer fallback layout`).
  - When image data exists but raw/RLE payload is too short, return malformed
    (do not conflate truncation with layer-only PSD policy).
- Existing regression includes ICC and alpha combinations on Raw/ZIP/ZIP+Prediction
  paths, 32-bit decode matrix coverage including RLE (RGB/Gray/CMYK/Lab),
  CMYK16/Lab16 decode coverage, and mode7 coverage across mapped paths:
  - `3ch->RGB` and `4ch->CMYK` for `8/16/32-bit`
  - `Raw/RLE/ZIP/ZIP+Prediction` decode regressions
  - policy traces for invalid channel-count/depth combinations
  - CMYK-mapped mode7 ICC trace coverage (valid profile no-false-failure,
    invalid profile, and malformed resource section)
  - mode7 missing-composite policy traces across mapped depth variants
    (`RGB8/16/32`, `CMYK8/16/32`)
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

- Optional handling of merged-composite absence via layer section.
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

1. Keep ICC conversion on RGB channels only, never mutate transparent masks.
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
   - minimal layer reconstruction path
     (single-layer 8-bit RGB/Gray/Duotone).
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

1. Keep unsupported-policy matrix synchronized as decode scope changes:
   every policy rejection path must keep deterministic trace coverage.
2. Keep defensive-branch policy explicit:
   unit-test directly reachable validate guards, and document
   policy-bounded guards as defensive-only.
3. Keep PSB (`8BPB`) out of scope for this milestone, but maintain a migration
   boundary from PSD parser/decoder primitives.
