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
  - PSB high-offset section-window coverage now includes
    `mode=4` and `mode7(4ch->CMYK)` CMYK16/CMYK32
    `normal` and `normal_rle` decode baselines, plus malformed traces for:
    `layer info end overrun`, `channel section-window overrun`, and
    `RLE payload window overrun`.
    Large-window representative coverage is fixed with `high_offset_xxlarge`
    (4 MiB section shift) on top of the baseline `high_offset` set.
    `>1MiB` PSB fixtures are generated on demand at test runtime
    (autotools/meson) instead of being kept as static repository assets.
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
  - TySh EngineData fallback parsing now complements descriptor/SXFL paths for
    no-pixel non-pixel layers:
    - direct `FillColor [...]`,
    - `FillColor << /Values [...] >>`, and
    - `StyleRun/StyleSheetData`-nested
      `FillColor << /ColorSpace /{Gray|RGB|HSB|CMYK|Lab} /Values [...] >>`
      parsing.
    - named values arrays such as
      `FillColor << /Values [/CMYK ...] >>` and
      `FillColor << /Values [/HSB ...] >>`.
    - `/Color << /Values [/DeviceCMYK|DeviceRGB|DeviceGray|CIELab ...] >>`
      parsing on native CMYK8 and mode7-mapped CMYK8 paths.
    - `/DefaultStyleSheet << /Color << /Values
      [/DeviceCMYK|DeviceRGB|DeviceGray|CIELab ...] >> >>` parsing with the
      same precedence contract (`StyleRun` > `StyleSheetData` >
      `DefaultStyleSheet` > top-level `FillColor` > loose fallback), including
      representative decode+LSQA coverage on PSD/PSB CMYK16/CMYK32 paths.
    - mode7 CMYK16 `DefaultStyleSheet /Color /Values [/DeviceCMYK ...]`
      ICC representative contract now includes `valid/bad/malformed`
      trace/decode coverage on PSD/PSB paths.
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
    TySh EngineData no-pixel fallback surfaces for native CMYK and
    mode7-mapped CMYK paths:
    - `FillColor [...]` / `FillColor << /Values [...] >>` on CMYK8, and
    - `StyleRun/StyleSheetData` `FillColor /Values` CMYK surfaces on
      `8/16/32bpc`.
  - named HSB EngineData coverage now includes:
    - decode/LSQA matrix on PSD and PSB (`mode=4`, `mode7(4ch->CMYK)`,
      `8/16/32bpc`) for both direct `/Values` and
      `StyleRun/StyleSheetData` `FillColor /Values` forms,
    - valid ICC no-false-failure matrix coverage, and
    - PSB bad/malformed ICC rotation coverage on 8bpc.
  - `/Color` named-space ICC contract coverage now includes representative
    no-pixel TySh surfaces for PSD/PSB:
    - valid ICC no-false-failure traces (`mode=4`, `mode7(4ch->CMYK)`),
    - bad ICC conversion-failure trace (`mode7(4ch->CMYK)`), and
    - malformed-resource skip trace (`mode=4`).
  - TySh EngineData named-space short payloads now keep deterministic malformed
    skip behavior (no synthetic fill render) for both direct `/Color /Values`
    and `StyleRun/StyleSheetData` `FillColor /Values` paths.
  - TySh EngineData FillColor LSQA density now includes CMYK16/32 representative
    decode+trace coverage for both `mode=4` and `mode7(4ch->CMYK)` paths.
  - TySh `/DefaultStyleSheet /Color /Values [/DeviceCMYK ...]` decode+LSQA
    coverage now includes `mode7(4ch->CMYK16)` on both PSD and PSB paths.
  - TySh EngineData `FillFlag` semantics are now fixed for no-pixel fallback:
    `FillFlag=false` skips synthetic fill rendering with deterministic info
    trace (`skipping non-pixel fill payload due to FillFlag=false`), while
    `FillFlag=true` preserves existing fill-render behavior.
  - TySh EngineData `StrokeFlag` / `StrokeColor` semantics are now fixed for
    no-pixel fallback:
    when `FillFlag=false` and `StrokeFlag=true` with decodable `StrokeColor`,
    fallback renders a synthetic solid layer from stroke color and emits
    deterministic info trace
    (`rendering non-pixel stroke payload in layer fallback`);
    otherwise `FillFlag=false` keeps skip behavior.
  - TySh EngineData opacity semantics are now fixed for no-pixel fallback:
    - `FillOpacity` scales synthetic fill-layer opacity when fill rendering is
      selected,
    - `StrokeOpacity` scales synthetic stroke-layer opacity when
      `FillFlag=false` and `StrokeFlag=true`,
    - malformed opacity values are ignored with deterministic info traces
      (`malformed TySh FillOpacity; ignoring` and
      `malformed TySh StrokeOpacity; ignoring`).
  - TySh EngineData `StyleRun` opacity now follows the same
    `/RunLengthArray` weighted contract as color:
    valid run opacities are weighted by run length, malformed run opacities are
    ignored per-run with deterministic malformed-opacity traces, and when no
    valid run opacity exists the parser falls back to
    `StyleSheetData > DefaultStyleSheet > top-level` precedence.
  - TySh EngineData `StyleRun` payloads that provide
    `/RunLengthArray + /RunArray` now use RunLength-weighted color
    compositing for all decodable runs before falling back to generic
    `StyleSheetData` precedence when no valid weighted run exists.
  - TySh EngineData `StrokeColor` in `StyleRun` now follows the same
    RunLength-weighted contract for no-pixel fallback when
    `FillFlag=false` and `StrokeFlag=true`.
  - TySh EngineData `StyleRun` payloads that resolve stylesheet payloads via
    `/StyleSheetSet` (instead of inline `RunArray` `StyleSheetData`) follow
    the same RunLength-weighted contract.
  - TySh EngineData `StyleRun` payloads that use `RunStyle` references now
    resolve through `/StyleSheetSet` with the same weighted-run contract.
  - TySh weighted-run LSQA density now includes CMYK16/32 representative
    decode+trace coverage across PSD and PSB mixed surfaces
    (`mode=4` and `mode7(4ch->CMYK)`).
  - `/DefaultStyleSheet /Color /Values` named-space ICC contract coverage now
    includes representative PSD/PSB CMYK surfaces:
    valid ICC no-false-failure traces (`mode=4`, `mode7(4ch->CMYK)`),
    bad ICC failure trace (`mode7(4ch->CMYK)`), and
    malformed-resource skip trace (PSB native CMYK).
  - PSB malformed boundary parity for CMYK32 now covers both native
    `mode=4` and `mode7(4ch->CMYK)` paths with fixed trace contracts for:
    `layer/mask length`, `layer info length`, `layer channel length`
    (`overflow`/`UINT64_MAX`), `RLE row table (too short)`, and
    `RLE row length`.
  - PSB large-size boundary handling in layer fallback now uses a shared
    layer-info window validator (`8BPB+version=2` and PSD path share
    overflow/overrun gate logic).
  - descriptor malformed regression now includes:
    - structurally malformed fill additional-block length contracts
      (`malformed layer extra data`), and
    - descriptor-invalid payload contracts that keep decode success with
      deterministic skip trace (`malformed non-pixel fill payload; skipping layer`)
      on both PSD and PSB RGB8 fallback paths (`SoCo/GdFl/PtFl`).
  - `psd-tools` duotone hardcase (`colormodes/4x4_8bit_duotone.psd`) now uses
    coregraphics hybrid expected (`.six`) instead of `psd-tools composite`
    baseline to avoid unsupported-duotone parity noise; `1293` is no longer XFAIL.
  - Additional MIT-licensed `psd-tools` hardcases are imported with static
    fixture/expected assets and covered by PASS LSQA parity tests:
    `blend-and-clipping.psd`, `layers-minimal/pattern-fill.psd`,
    and `effects/shape-fx.psd`.
  - `psd-tools` hybrid asset generation now enforces MIT metadata guards:
    official upstream repository match, per-fixture `license=MIT`, and
    upstream LICENSE text checks against MIT-required clauses.
  - Additional MIT-licensed `psd-tools` fixtures are imported for
    transparency/vector-mask/effects hardcases:
    `transparency/fill-opacity.psd`, `transparency/clip-opacity.psd`,
    `vector-mask.psd`, `vector-mask-disabled.psd`,
    `effects/shape-fx2.psd`, and `effects/stroke-composite.psd`.
  - New hardcase LSQA TAPs `1313..1318` are added.
    `1314..1316` are PASS and `1313/1317/1318` are currently fixed as XFAIL
    until remaining opacity/effects parity work is completed.
  - Layer-fallback clipping traversal now adds an overlap-aware tie-break
    when orphan clipping counts are equal, reducing clip-base mismatches on
    alternating base/clip layer records.
  - `knko` is now evaluated by payload value (`0:none`, `1:shallow`,
    `>=2:deep`) instead of tag-presence only.
  - Representative trace guards were added as PASS tests:
    `1327` (no clipping orphan trace), `1328` (knko=0 no ignore trace),
    `1329` (clbl parse), `1330` (infx parse), `1331` (ebbl parse),
    `1349` (clbl=0 deferred-effect trace), `1350` (vstk parse trace +
    no unexpected suppression), `1378` (stroke-composite merged-to-
    fallback preference trace), and `1379` (vector-stroke preference trace).
  - PASS LSQA representatives now include shape-fx parse+quality contracts
    for `clbl`/`infx` (`1347`, `1348`) while keeping hardcase XFAIL gates
    (`1317`, `1318`) unchanged until full effect parity is completed.
  - Stroke suppression on vector-mask non-pixel fill layers is now gated by
    `clbl=0` to avoid over-suppressing `clbl=1` effect stacks.
  - Merged-vs-fallback selection now includes a narrow `clbl=1 + infx=0`
    vector/effects subset so stroke-composite parity work can proceed
    without broad fallback expansion.
  - Effect-object trace coverage is extended for stroke-composite parity work:
    `OrGl/IrGl/ChFX/DrSh/IrSh` are traced as distinct parsed objects, and
    `ebbl/bevl` now records bevel highlight/shadow channels separately
    (inactive-object traces included for deterministic diagnostics).
  - PASS trace representatives are expanded for the effect subset contract:
    `1380` (`clbl=1` no `clbl=0` defer trace), `1381` (`infx=0` keeps stroke),
    `1384` (`clbl=1` deferred interior overlays keep explicit stroke), and
    `1410..1413` (bevel highlight/shadow split, OrGl/ChFX distinct parsing,
    `infx=0` interior-only suppression contract, and `clbl=1` deferred-stroke
    preservation contract).
  - Legacy `lrFX` supplementation is now enabled when `lfx2` keeps
    `OrGl/ChFX/bevl` inactive: missing active channels are backfilled in the
    effect model, and trace contract `1452` fixes the
    `merging legacy lrFX effects missing from lfx2` behavior.
  - `GrFl` effect scale now normalizes descriptor percent values
    (`Scl=100` -> `1.0`) in the same way as fill-gradient parsing,
    improving stroke-composite gradient parity (`1318` score uplift
    while keeping XFAIL).
  - New trace representative `1453` fixes active `GrFl` parse coverage on
    `effects/stroke-composite.psd` and guards against regressing to
    inactive-only gradient effect parsing.
  - New trace representative `1454` fixes bevel-apply coverage on
    `effects/stroke-composite.psd`, ensuring highlight/shadow application
    stays active after `lfx2` inactive + `lrFX` complement.
  - Clipping-group base suppression for synthesized `vstk` stroke is now
    limited to `clbl=0` layers, and trace representative `1459` fixes the
    `clbl=1` deferred-stroke preservation contract on
    `effects/stroke-composite.psd`.
  - Deferred clipping-group effects now apply clip-weighted opacity/coverage
    in the composite pass (`SoFi`, `GrFl`, `FrFX`), and PASS trace
    representatives `1464..1467` lock clip-weighted contracts while
    `1318` remains XFAIL.
  - Legacy `lrFX` inactive completion now synchronizes named glow effects
    (`OrGl/IrGl/ChFX/DrSh/IrSh/bevl`) to proxy glow fields
    (`outer_glow`/`inner_glow`) when proxy slots are unset/inactive.
    For `clbl=1` stroke-composite stacks, `infx=0` interior-glow suppression
    is narrowed to `clbl=0` subsets only; PASS trace representatives
    `1468..1470` lock the bridge/suppression contracts.
  - Legacy `lrFX` glow parsing (`oglw/iglw`) now uses payload-length/version
    fallback for enabled/opacity tail bytes and v2 alternative color blocks.
    Inactive glow targets seen in `lfx2` can now opt into legacy completion
    (while keeping active `lfx2` channels intact), and PASS trace
    representatives `1472..1474` lock v2 `iglw` inactive safety,
    shape-fx2 merge/no-ignore, and active-channel preservation contracts.
    Inactive `ebbl` targets now follow the same completion gate, guarded by
    trace representative `1475`.
  - `psd-tools` hardcase LSQA TAPs `1289..1292` now use PASS-first wording
    (legacy TODO/XFAIL text removed), and decode-level parity is guarded by
    builtin loader frame-match checks in `0014` for:
    `2layer_8ele_tblocks`, `layer-name-emoji`,
    `transparentbg-gimp`, and `group-divider-blend-mode`.
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

1. Extend non-pixel semantics beyond current fill/TySh color extraction support while
   preserving deterministic degrade traces for unsupported semantics
   (for example richer `TySh` text-style payload interpretation beyond
   `FillColor`/`Color` value extraction).
   - Priority hardcase targets: `1313` (fill-opacity parity) and
     `1317/1318` (effects parity for shape-fx2/stroke-composite).
2. Extend PSB (`8BPB+version=2`) from parser-compatible partial support toward
   large-document parity:
   - after mode4/mode7 CMYK16/CMYK32 `high_offset` + `high_offset_xxlarge`
     valid/malformed parity, expand to larger-than-4MiB section-window layouts
     and higher-offset cases (requires allocator ceiling policy decision), and
   - design/implement large-size section boundary handling beyond the current
     parser/fallback surface.
3. Increase cross-mode visual quality regression density for non-pixel payload
   rendering (LSQA), including Lab/Gray style payload baselines, while keeping
   current ICC/trace contracts fixed.
