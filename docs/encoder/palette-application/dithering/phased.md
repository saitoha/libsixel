# Dithering: `phased`

`phased` is a GPU-only spatial error-diffusion method for RGB8 images in gamma RGB. It uses four spatial phases and two cycles to distribute quantization error within the current image. It has no residual carried between video frames and no CPU implementation. The [dithering reference](README.md) owns the general separation between dither and palette lookup.

## Selection and supported inputs

```sh
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d phased image.png
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d phased:gain=small-error image.png
```

`gain=standard` is the default; `gain=small-error` reduces small residuals. `SIXEL_DITHER_PHASED_GAIN` supplies the environment default. Explicit suboptions override that default. The spatial schedule is serpentine; automatic scan selection resolves to serpentine, while an explicit raster scan is rejected. The method does not use a gradient map, add color noise, or implement the CPU methods' deadzone and perturbation controls.

Metal, gamma RGB8 source storage and the indexed output component path are required. Float32 images, extended working spaces and legacy-only output modes are unsupported. A GPU policy of `off`, unavailable device, or incompatible lookup returns an error before output rather than substituting another dither. The GPU `auto` small-image cutoff cannot select CPU because this method has no CPU capability.

`ordered` accepts the fractional corrected queries used by this method. Lookup `auto` selects `ordered` specifically for `phased`; other methods retain their ordinary lookup resolution. Explicit byte-query-only policies are rejected. Fractional code-value queries in `[0,255]^3` do not imply support for float32 images. The method searches the actual emitted RGB8 palette and computes residuals against those same colors; the reserved transparency slot is excluded.

## Spatial update and coefficients

Initialization selects the nearest emitted color without noise. Each pixel stores its selected index and the fractional residual of its corrected query. A fixed 64×64 blue-noise rank mask assigns pixels to four equally sized phases. The mask selects update order only; it is not added to RGB and does not change between frames.

Each subpass gathers six contributions from sender residuals. Horizontal orientation reverses on alternating rows; the far horizontal and vertical taps have radius four. Sender channel-error magnitude, rounded to the nearest integer with half values upward and clamped to 255, selects that channel's six coefficients. Out-of-image taps are omitted. Transparent pixels absorb incoming error and transmit zero residual; a farther tap can cross a transparent pixel.

Two state generations prevent same-phase readers from observing incomplete writes. Earlier phases in a cycle are already updated; the current and later phases use their preceding generation. The next query is the original RGB8 sample plus gathered error, clipped to `[0,255]`. Two complete cycles perform eight subpasses. Incoming error is reconstructed each time instead of repeatedly accumulated, and each application starts with fresh initialization.

[`src/phased.key`](../../../../src/phased.key) owns the directional knots and the small-error gain knots. [`tools/gen-phased.sh`](../../../../tools/gen-phased.sh) generates [`src/phased-coefficients.h`](../../../../src/phased-coefficients.h) using the rational interpolation generator. Edit knots and regenerate; do not edit generated rows. These inputs are independent of the CPU LSO2 table so changing one method does not silently retune the other.

The standard curve uses the generated rational sender ratios. The small-error multiplier interpolates `(0,0.6)`, `(8,0.6)`, `(32,1)` and `(255,1)` in absolute channel-error units. Each ratio is rounded to float32 first, multiplied by the interpolated double multiplier, then rounded once to float32. Small-error attenuation can reduce grain and output size, but it can increase banding; it is not a universally better quality setting.

## Delta and execution ownership

The spatial solve does not consult retained history. After the last subpass, the corrected fractional query is reconstructed and the final display decision compares it with the retained RGB and the selected emitted color. Threshold and refresh selection come from the existing [delta configuration](../../output/delta-encoding.md), including partial rectangles. A refresh-selected T0 pixel is reevaluated; it is not forced to paint when the retained color remains no worse. `compare=target` and `compare=query` have the same corrected-query basis for this spatial method. Final skips do not feed back into the completed solve, so `error=skip` and `error=diffuse` do not alter its iteration.

The GPU preparer freezes configuration, selects a fractional lookup capability and compiles its own program. The GPU plan exclusively owns two image-state buffers and its pass order. Completed plans return storage to a method-private idle pool, bounded to two bundles of at most 128 MiB each. Reuse changes allocation cost, not initialization or frame history. It consumes lookup through the independent prepare/program/binding interfaces without inspecting a policy name or private table. No shader source files are read at runtime; Metal sources are embedded during the build.

PaletteApply owns publication. The plan drains GPU work and finishes every acquired lookup binding before returning. Only a successful completed result is validated and published as a whole image; CPU SIXEL encoding cannot consume intermediate subpasses. RetainedPlane commits history only after output acceptance. Reconfiguration does not mutate an existing plan, and failed preparation leaves the prior PaletteApply plan usable.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| PH-01 | Standard output matches independently checked scatter-reference indices. | [tests/dither/phased/0001_standard_indexes.t](../../../../tests/dither/phased/0001_standard_indexes.t) |
| PH-02 | Small-error output matches its separate scatter-reference indices. | [tests/dither/phased/0002_small_error_indexes.t](../../../../tests/dither/phased/0002_small_error_indexes.t) |
| PH-03 | Repeated application starts with fresh image state. | [tests/dither/phased/0003_scratch_reset.t](../../../../tests/dither/phased/0003_scratch_reset.t) |
| PH-04 | Reconfiguring the preparer does not change a prepared plan. | [tests/dither/phased/0004_configuration_snapshot.t](../../../../tests/dither/phased/0004_configuration_snapshot.t) |
| PH-05 | GPU off cannot substitute a CPU dither. | [tests/dither/phased/0005_gpu_off_rejected.t](../../../../tests/dither/phased/0005_gpu_off_rejected.t) |
| PH-06 | A byte-only lookup is rejected before execution. | [tests/dither/phased/0006_byte_lookup_rejected.t](../../../../tests/dither/phased/0006_byte_lookup_rejected.t) |
| PH-07 | GPU auto executes small images despite the CPU cutoff. | [tests/dither/phased/0007_auto_small_image.t](../../../../tests/dither/phased/0007_auto_small_image.t) |
| PH-08 | Float32 source configuration is rejected. | [tests/dither/phased/0008_float_image_rejected.t](../../../../tests/dither/phased/0008_float_image_rejected.t) |
| PH-09 | Explicit raster scan is rejected. | [tests/dither/phased/0009_raster_rejected.t](../../../../tests/dither/phased/0009_raster_rejected.t) |
| PH-10 | Invalid gain configuration preserves the previous configuration. | [tests/dither/phased/0010_invalid_gain_atomic.t](../../../../tests/dither/phased/0010_invalid_gain_atomic.t) |
| PH-11 | T0 final selection matches retained-color reference decisions. | [tests/dither/phased/0011_delta_t0.t](../../../../tests/dither/phased/0011_delta_t0.t) |
| PH-12 | A supplied refresh selection lowers T48 to T0 only at selected pixels. | [tests/dither/phased/0012_delta_refresh.t](../../../../tests/dither/phased/0012_delta_refresh.t) |
| PH-13 | Partial rectangles use the retained canvas origin and row stride. | [tests/dither/phased/0013_partial_rectangle.t](../../../../tests/dither/phased/0013_partial_rectangle.t) |
| PH-14 | Applying the dither does not mutate retained RGB. | [tests/dither/phased/0014_retained_immutable.t](../../../../tests/dither/phased/0014_retained_immutable.t) |
| PH-15 | Pre-dispatch cancellation publishes no rows. | [tests/dither/phased/0015_cancel_before.t](../../../../tests/dither/phased/0015_cancel_before.t) |
| PH-16 | Post-dispatch cancellation publishes no rows. | [tests/dither/phased/0016_cancel_after.t](../../../../tests/dither/phased/0016_cancel_after.t) |
| PH-17 | A cancelled lookup lease can be acquired again. | [tests/dither/phased/0017_cancel_reuse.t](../../../../tests/dither/phased/0017_cancel_reuse.t) |
| PH-18 | Failed preparation preserves the usable plan. | [tests/dither/phased/0018_prepare_failure_atomic.t](../../../../tests/dither/phased/0018_prepare_failure_atomic.t) |
| PH-19 | Full RGB palette output matches the scatter reference. | [tests/dither/phased/0019_palette256.t](../../../../tests/dither/phased/0019_palette256.t) |
| PH-20 | Reprepared pooled storage starts without previous residuals. | [tests/dither/phased/0020_reprepare_reset.t](../../../../tests/dither/phased/0020_reprepare_reset.t) |
| PH-21 | PaletteApply retains the supplied dither preparer after caller release. | [tests/dither/phased/0021_preparer_lifetime.t](../../../../tests/dither/phased/0021_preparer_lifetime.t) |

### Quality and verification limits

The fixed reference fixture checks spatial arithmetic and final decisions, not a perceptual-quality threshold or all palettes. It does not establish a universal MS-SSIM, CAMBI, output-size or frame-time advantage. Video evaluation must include scene transitions, banding-sensitive regions and output-size distributions. GPU cases skip when Metal is unavailable; portable configuration tests still run.
