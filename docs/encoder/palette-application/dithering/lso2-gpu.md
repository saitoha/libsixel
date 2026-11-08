# Dithering: `lso2` on the GPU

The GPU component of `lso2` (`dither/lso2.gpu`) is a spatial error-diffusion solver for RGB8 images in gamma RGB. It uses four spatial phases and two cycles to distribute quantization error within the current image. It has no residual carried between video frames. The CPU `lso2` component remains the CPU form; the encoder selects this component only when the GPU policy admits it. The fixed-coefficient methods have their own GPU components, listed in the [dithering reference](README.md), which also owns the general separation between dither and palette lookup.

## Selection and supported inputs

```sh
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d lso2 image.png
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d lso2:gain=small-error image.png
```

`gain=standard` is the default; `gain=small-error` reduces small residuals. `SIXEL_DITHER_LSO2_GAIN` supplies the environment default. Explicit suboptions override that default. The GPU LSO2 schedule is serpentine; automatic scan selection resolves to serpentine, while an explicit raster scan keeps `lso2` on the CPU and the GPU component rejects it. GPU LSO2 does not use a gradient map, add color noise, or implement the CPU methods' deadzone and perturbation controls.

When the encoder admits a GPU, the dither keeps `method_for_diffuse` as `lso2` and marks the frame's diffusion as running on the GPU; the trace reports `diffuse=lso2` with `diffuse_on_gpu=1`.

Metal, gamma RGB8 source storage and the indexed output component path are required. Float32 images, extended working spaces and legacy-only output modes are unsupported. A GPU policy of `off`, unavailable device, or incompatible lookup returns an error before output rather than substituting another dither. The GPU `auto` small-image cutoff cannot select CPU because this component has no CPU capability.

All named GPU lookup policies accept this method's fractional corrected queries under their [declared search semantics](../lookup-policy/README.md). Lookup `auto` selects `ordered`; approximate lookup remains approximate, and FHEDT retains its GPU setting restrictions. Explicit byte-only injected providers are rejected. Fractional code-value queries in `[0,255]^3` do not imply float32 image support. Residuals use the actual emitted RGB8 colors, and the reserved transparency slot is excluded.

## Spatial update and coefficients

Initialization asks the selected lookup for an emitted color without noise. Each pixel stores its selected index and the fractional residual of its corrected query. A fixed 64×64 blue-noise rank mask assigns pixels to four equally sized phases. The mask selects update order only; it is not added to RGB and does not change between frames.

Each subpass gathers six contributions from sender residuals. Horizontal orientation reverses on alternating rows; the far horizontal and vertical taps have radius four. Sender channel-error magnitude, rounded to the nearest integer with half values upward and clamped to 255, selects that channel's six coefficients. Out-of-image taps are omitted. Transparent pixels absorb incoming error and transmit zero residual; a farther tap can cross a transparent pixel.

Two state generations prevent same-phase readers from observing incomplete writes. Earlier phases in a cycle are already updated; the current and later phases use their preceding generation. The next query is the original RGB8 sample plus gathered error, clipped to `[0,255]`. Two complete cycles perform eight subpasses. Incoming error is reconstructed each time instead of repeatedly accumulated, and each application starts with fresh initialization.

[`src/lso2-gpu.key`](../../../../src/lso2-gpu.key) owns the directional knots and the small-error gain knots. [`tools/gen-lso2-gpu.sh`](../../../../tools/gen-lso2-gpu.sh) generates [`src/lso2-gpu-coefficients.h`](../../../../src/lso2-gpu-coefficients.h) using the rational interpolation generator. Edit knots and regenerate; do not edit generated rows. These inputs are independent of the CPU LSO2 table so changing one method does not silently retune the other.

The standard curve uses the generated rational sender ratios. The small-error multiplier interpolates `(0,0.6)`, `(8,0.6)`, `(32,1)` and `(255,1)` in absolute channel-error units. Each ratio is rounded to float32 first, multiplied by the interpolated double multiplier, then rounded once to float32. Small-error attenuation can reduce grain and output size, but it can increase banding; it is not a universally better quality setting.

## Delta and execution ownership

The spatial solve does not consult retained history. After the last subpass, the corrected fractional query is reconstructed and the final display decision compares it with the retained RGB and the selected emitted color. Threshold and refresh selection come from the existing [delta configuration](../../output/delta-encoding.md), including partial rectangles. A refresh-selected T0 pixel is reevaluated; it is not forced to paint when the retained color remains no worse. `compare=target` and `compare=query` have the same corrected-query basis for this spatial method. Final skips do not feed back into the completed solve, so `error=skip` and `error=diffuse` do not alter its iteration.

The GPU preparer freezes configuration, selects a fractional lookup capability and compiles its own program. The GPU plan exclusively owns two image-state buffers and its pass order. Completed plans return storage to a method-private idle pool, bounded to two bundles of at most 128 MiB each. Reuse changes allocation cost, not initialization or frame history. It consumes lookup through the independent prepare/program/binding interfaces without inspecting a policy name or private table. No shader source files are read at runtime; Metal sources are embedded during the build.

Point-query providers execute inside each fused dither kernel. A tile-batch provider instead requires ordered GPU dispatches for corrected-query generation, collective lookup and residual update. The dither owns the temporary `float4` query and `uint32` index planes, adding 20 bytes per pixel only on this path. Inactive phase pixels and transparency do not enter the tile bounds. All same-phase queries are generated before any same-phase state write, preserving the two-generation schedule. There is no intermediate CPU readback. The [execution map](../../execution-map/README.md) owns the batch ABI and buffer lifetime.

PaletteApply owns publication. The plan drains GPU work and finishes every acquired lookup binding before returning. Only a successful completed result is validated and published as a whole image; CPU SIXEL encoding cannot consume intermediate subpasses. The [retained publication contract](../../output/delta-encoding.md#indexed-retained-state-transactions) distinguishes receipt-checked direct output from independent playback history advance. Reconfiguration does not mutate an existing plan, and failed preparation leaves the prior PaletteApply plan usable.

The optional deferred facet splits the same algorithm into a history-free solve and a history-dependent resolve. Solve retains the iteration index `k`, reconstructs the fractional query `q = error + emitted_palette[k]`, and computes its corrected lookup candidate `j`. Both candidates are necessary: without valid history the existing algorithm uses `k`; with history it compares `q` and `j`. The dead second iteration plane stores the corrected query and candidate. Resolve performs no lookup acquisition or search, and uses the current baseline supplied by the R owner only. This adds a solve-tail dispatch and a later command buffer; it does not change coefficients or carry residuals between frames.

A completed solution owns its source-derived state, frozen wire palette and geometry. Solve drains and releases the lookup lease before returning, so a ready solution does not obstruct another job sharing that lookup plan. Its prepared facet survives release or reconfiguration of the outer PaletteApply object. The plan remains exclusive until the last solution reference is released. Argument rejection before execution leaves it ready; execution, including cancellation, consumes it once. PaletteApply seals a failed resolve without publishing rows. [Animation threading](../../../threading/animation.md) owns admission, bounded handoffs and ordered disposal; direct encoder calls retain synchronous application.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| LG-01 | Standard output matches independently checked scatter-reference indices. | [tests/dither/lso2-gpu/0001_standard_indexes.t](../../../../tests/dither/lso2-gpu/0001_standard_indexes.t) |
| LG-02 | Small-error output matches its separate scatter-reference indices. | [tests/dither/lso2-gpu/0002_small_error_indexes.t](../../../../tests/dither/lso2-gpu/0002_small_error_indexes.t) |
| LG-03 | Repeated application starts with fresh image state. | [tests/dither/lso2-gpu/0003_scratch_reset.t](../../../../tests/dither/lso2-gpu/0003_scratch_reset.t) |
| LG-04 | Reconfiguring the preparer does not change a prepared plan. | [tests/dither/lso2-gpu/0004_configuration_snapshot.t](../../../../tests/dither/lso2-gpu/0004_configuration_snapshot.t) |
| LG-05 | GPU off cannot substitute a CPU dither. | [tests/dither/lso2-gpu/0005_gpu_off_rejected.t](../../../../tests/dither/lso2-gpu/0005_gpu_off_rejected.t) |
| LG-06 | A byte-only lookup is rejected before execution. | [tests/dither/lso2-gpu/0006_byte_lookup_rejected.t](../../../../tests/dither/lso2-gpu/0006_byte_lookup_rejected.t) |
| LG-07 | GPU auto executes small images despite the CPU cutoff. | [tests/dither/lso2-gpu/0007_auto_small_image.t](../../../../tests/dither/lso2-gpu/0007_auto_small_image.t) |
| LG-08 | Float32 source configuration is rejected. | [tests/dither/lso2-gpu/0008_float_image_rejected.t](../../../../tests/dither/lso2-gpu/0008_float_image_rejected.t) |
| LG-09 | Explicit raster scan is rejected for LSO2. | [tests/dither/lso2-gpu/0009_raster_rejected.t](../../../../tests/dither/lso2-gpu/0009_raster_rejected.t) |
| LG-10 | Invalid gain configuration preserves the previous configuration. | [tests/dither/lso2-gpu/0010_invalid_gain_atomic.t](../../../../tests/dither/lso2-gpu/0010_invalid_gain_atomic.t) |
| LG-11 | T0 final selection matches retained-color reference decisions. | [tests/dither/lso2-gpu/0011_delta_t0.t](../../../../tests/dither/lso2-gpu/0011_delta_t0.t) |
| LG-12 | A supplied refresh selection lowers T48 to T0 only at selected pixels. | [tests/dither/lso2-gpu/0012_delta_refresh.t](../../../../tests/dither/lso2-gpu/0012_delta_refresh.t) |
| LG-13 | Partial rectangles use the retained canvas origin and row stride. | [tests/dither/lso2-gpu/0013_partial_rectangle.t](../../../../tests/dither/lso2-gpu/0013_partial_rectangle.t) |
| LG-14 | Applying the dither does not mutate retained RGB. | [tests/dither/lso2-gpu/0014_retained_immutable.t](../../../../tests/dither/lso2-gpu/0014_retained_immutable.t) |
| LG-15 | Pre-dispatch cancellation publishes no rows. | [tests/dither/lso2-gpu/0015_cancel_before.t](../../../../tests/dither/lso2-gpu/0015_cancel_before.t) |
| LG-16 | Post-dispatch cancellation publishes no rows. | [tests/dither/lso2-gpu/0016_cancel_after.t](../../../../tests/dither/lso2-gpu/0016_cancel_after.t) |
| LG-17 | A cancelled lookup lease can be acquired again. | [tests/dither/lso2-gpu/0017_cancel_reuse.t](../../../../tests/dither/lso2-gpu/0017_cancel_reuse.t) |
| LG-18 | Failed preparation preserves the usable plan. | [tests/dither/lso2-gpu/0018_prepare_failure_atomic.t](../../../../tests/dither/lso2-gpu/0018_prepare_failure_atomic.t) |
| LG-19 | Full RGB palette output matches the scatter reference. | [tests/dither/lso2-gpu/0019_palette256.t](../../../../tests/dither/lso2-gpu/0019_palette256.t) |
| LG-20 | Reprepared pooled storage starts without previous residuals. | [tests/dither/lso2-gpu/0020_reprepare_reset.t](../../../../tests/dither/lso2-gpu/0020_reprepare_reset.t) |
| LG-21 | PaletteApply retains the supplied dither preparer after caller release. | [tests/dither/lso2-gpu/0021_preparer_lifetime.t](../../../../tests/dither/lso2-gpu/0021_preparer_lifetime.t) |
| LG-22 | Deferred standard retains the sequential pixels. | [tests/dither/lso2-gpu/0022_deferred_standard.t](../../../../tests/dither/lso2-gpu/0022_deferred_standard.t) |
| LG-23 | Deferred small error retains the sequential pixels. | [tests/dither/lso2-gpu/0023_deferred_small_error.t](../../../../tests/dither/lso2-gpu/0023_deferred_small_error.t) |
| LG-24 | Deferred t0 retains the sequential pixels. | [tests/dither/lso2-gpu/0024_deferred_t0.t](../../../../tests/dither/lso2-gpu/0024_deferred_t0.t) |
| LG-25 | Deferred refresh retains the sequential pixels. | [tests/dither/lso2-gpu/0025_deferred_refresh.t](../../../../tests/dither/lso2-gpu/0025_deferred_refresh.t) |
| LG-26 | Deferred partial retains the sequential pixels. | [tests/dither/lso2-gpu/0026_deferred_partial.t](../../../../tests/dither/lso2-gpu/0026_deferred_partial.t) |
| LG-27 | Changing source pixels after solve cannot change the solution. | [tests/dither/lso2-gpu/0027_deferred_source_owned.t](../../../../tests/dither/lso2-gpu/0027_deferred_source_owned.t) |
| LG-28 | A solution survives release of its preparer and deferred facet. | [tests/dither/lso2-gpu/0028_deferred_facet_lifetime.t](../../../../tests/dither/lso2-gpu/0028_deferred_facet_lifetime.t) |
| LG-29 | A second resolve is rejected. | [tests/dither/lso2-gpu/0029_deferred_repeat_rejected.t](../../../../tests/dither/lso2-gpu/0029_deferred_repeat_rejected.t) |
| LG-30 | Rejected geometry leaves the solution ready for a valid resolve. | [tests/dither/lso2-gpu/0030_deferred_bad_description_retry.t](../../../../tests/dither/lso2-gpu/0030_deferred_bad_description_retry.t) |
| LG-31 | Discarding a ready solution permits reuse of the prepared plan. | [tests/dither/lso2-gpu/0031_deferred_discard_reuse.t](../../../../tests/dither/lso2-gpu/0031_deferred_discard_reuse.t) |
| LG-32 | A ready solution excludes monolithic use of the same plan. | [tests/dither/lso2-gpu/0032_deferred_run_excluded.t](../../../../tests/dither/lso2-gpu/0032_deferred_run_excluded.t) |
| LG-33 | Outer reprepare leaves an existing solution and its lookup lease independent. | [tests/dither/lso2-gpu/0033_deferred_prepare_independent.t](../../../../tests/dither/lso2-gpu/0033_deferred_prepare_independent.t) |
| LG-34 | Successful deferred resolution publishes and seals the complete image. | [tests/dither/lso2-gpu/0034_deferred_publication.t](../../../../tests/dither/lso2-gpu/0034_deferred_publication.t) |
| LG-35 | Playback no delta matches sequential output. | [tests/dither/lso2-gpu/0035_playback_no_delta.t](../../../../tests/dither/lso2-gpu/0035_playback_no_delta.t) |
| LG-36 | Playback t0 matches sequential output. | [tests/dither/lso2-gpu/0036_playback_t0.t](../../../../tests/dither/lso2-gpu/0036_playback_t0.t) |
| LG-37 | Playback refresh matches sequential output. | [tests/dither/lso2-gpu/0037_playback_refresh.t](../../../../tests/dither/lso2-gpu/0037_playback_refresh.t) |
| LG-38 | Playback small error matches sequential output. | [tests/dither/lso2-gpu/0038_playback_small_error.t](../../../../tests/dither/lso2-gpu/0038_playback_small_error.t) |
| LG-39 | Playback single slot matches sequential output. | [tests/dither/lso2-gpu/0039_playback_single_slot.t](../../../../tests/dither/lso2-gpu/0039_playback_single_slot.t) |
| LG-40 | Solve N+1 completes while the writer of N is blocked. | [tests/dither/lso2-gpu/0040_playback_solve_overlap.t](../../../../tests/dither/lso2-gpu/0040_playback_solve_overlap.t) |
| LG-41 | Cancellation discards an already ready later frame. | [tests/dither/lso2-gpu/0041_playback_cancel_ready.t](../../../../tests/dither/lso2-gpu/0041_playback_cancel_ready.t) |
| LG-42 | Writer failure drains the ready job before ordered completion. | [tests/dither/lso2-gpu/0042_playback_failure_drain.t](../../../../tests/dither/lso2-gpu/0042_playback_failure_drain.t) |
| LG-43 | A new admission after cancellation remains eligible. | [tests/dither/lso2-gpu/0043_playback_cancel_new_admission.t](../../../../tests/dither/lso2-gpu/0043_playback_cancel_new_admission.t) |
| LG-44 | The completion callback can read its exact stage sample. | [tests/dither/lso2-gpu/0044_playback_callback_sample.t](../../../../tests/dither/lso2-gpu/0044_playback_callback_sample.t) |
| LG-45 | RGB241 resolution matches monolithic output. | [tests/dither/lso2-gpu/0045_deferred_rgb241.t](../../../../tests/dither/lso2-gpu/0045_deferred_rgb241.t) |
| LG-46 | Missing delta history retains monolithic first-frame selection. | [tests/dither/lso2-gpu/0046_deferred_missing_history.t](../../../../tests/dither/lso2-gpu/0046_deferred_missing_history.t) |
| LG-47 | Cancelled resolution seals FAILED without publishing rows. | [tests/dither/lso2-gpu/0047_deferred_cancel_seals.t](../../../../tests/dither/lso2-gpu/0047_deferred_cancel_seals.t) |
| LG-48 | A stale stage sequence returns FALSE without changing the caller buffer. | [tests/dither/lso2-gpu/0048_playback_sample_stale.t](../../../../tests/dither/lso2-gpu/0048_playback_sample_stale.t) |
| LG-49 | An undersized stage sample is rejected without changing the buffer. | [tests/dither/lso2-gpu/0049_playback_sample_short.t](../../../../tests/dither/lso2-gpu/0049_playback_sample_short.t) |
| LG-50 | A larger sample preserves its declared size and unknown tail. | [tests/dither/lso2-gpu/0050_playback_sample_tail.t](../../../../tests/dither/lso2-gpu/0050_playback_sample_tail.t) |
| LG-51 | Changing frame dimensions preserves synchronous output bytes. | [tests/dither/lso2-gpu/0051_playback_resize_bytes.t](../../../../tests/dither/lso2-gpu/0051_playback_resize_bytes.t) |
| LG-52 | A geometry transition breaks the completion interval. | [tests/dither/lso2-gpu/0052_playback_resize_interval.t](../../../../tests/dither/lso2-gpu/0052_playback_resize_interval.t) |
| LG-53 | A blocked resolve does not prevent the next solve from completing. | [tests/dither/lso2-gpu/0053_playback_solve_without_credit.t](../../../../tests/dither/lso2-gpu/0053_playback_solve_without_credit.t) |

| LG-54 | Cancelled origin updates do not leak into later admissions. | [tests/dither/lso2-gpu/0054_playback_cancel_origin.t](../../../../tests/dither/lso2-gpu/0054_playback_cancel_origin.t) |
| LG-55 | Later admissions inherit the last successfully applied origin. | [tests/dither/lso2-gpu/0055_playback_inherited_origin.t](../../../../tests/dither/lso2-gpu/0055_playback_inherited_origin.t) |

| LGLUT-01 | `none` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0068_none_refresh.t](../../../../tests/dither/lso2-gpu/0068_none_refresh.t) |
| LGLUT-02 | `delaunay` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0069_delaunay_refresh.t](../../../../tests/dither/lso2-gpu/0069_delaunay_refresh.t) |
| LGLUT-03 | `vptree` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0070_vptree_refresh.t](../../../../tests/dither/lso2-gpu/0070_vptree_refresh.t) |
| LGLUT-04 | `rbc` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0071_rbc_refresh.t](../../../../tests/dither/lso2-gpu/0071_rbc_refresh.t) |
| LGLUT-05 | `mahalanobis` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0072_mahalanobis_refresh.t](../../../../tests/dither/lso2-gpu/0072_mahalanobis_refresh.t) |
| LGLUT-06 | `certlut` preserves the independent GPU LSO2 reference indexes with T48 and spatial T0 refresh selection. | [tests/dither/lso2-gpu/0073_certlut_refresh.t](../../../../tests/dither/lso2-gpu/0073_certlut_refresh.t) |

### Quality and verification limits

The fixed reference fixture checks spatial arithmetic and final decisions, not a perceptual-quality threshold or all palettes. It does not establish a universal MS-SSIM, CAMBI, output-size or frame-time advantage. Video evaluation must include scene transitions, banding-sensitive regions and output-size distributions. GPU cases skip when Metal is unavailable; portable configuration tests still run.

The fractional cases use bounded fixtures at fixed settings. Approximate-policy references check their documented rule rather than exhaustive-nearest equivalence. Device-dependent cases skip without Metal. These checks do not rank perceived quality or establish throughput.

| ID | Contract | Owning test |
| --- | --- | --- |
| FRAC-01 | Gridlut refresh. | [tests/dither/lso2-gpu/0074_gridlut_refresh.t](../../../../tests/dither/lso2-gpu/0074_gridlut_refresh.t) |
