# Dithering: `jajuni` on the GPU

The GPU component of `jajuni` (`dither/jajuni.gpu`) runs the Jarvis, Judice and Ninke kernel as a spatial error-diffusion solver for RGB8 images in gamma RGB. It is a separate component from the CPU `jajuni` dither and from every other GPU diffusion component; it shares no solver code with them. The [dithering reference](README.md) owns the general separation between dither and palette lookup.

## Selection and supported inputs

```sh
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d jajuni image.png
```

The encoder selects this component when the GPU policy admits an available GPU, the image is RGB8 gamma, and the request has no perturbation and no `clamp=none` carry. Otherwise `jajuni` stays on its CPU component. The dither keeps `method_for_diffuse` as `jajuni` and marks the frame's diffusion as running on the GPU; the trace reports `diffuse=jajuni` with `diffuse_on_gpu=1`. The component accepts raster and serpentine scans and keeps the CPU raster default.

Metal, gamma RGB8 source storage and the indexed output component path are required. A GPU policy of `off`, an unavailable device, or an incompatible lookup returns an error before output rather than substituting another dither. All named GPU lookup policies accept its fractional corrected queries under their [declared search semantics](../lookup-policy/README.md).

## Kernel and carry

The kernel weights over the denominator 48 are baked into the component's shader:

| dx | dy | weight |
| --- | --- | --- |
| 1 | 0 | 7/48 |
| 2 | 0 | 5/48 |
| -2 | 1 | 3/48 |
| -1 | 1 | 5/48 |
| 0 | 1 | 7/48 |
| 1 | 1 | 5/48 |
| 2 | 1 | 3/48 |
| -2 | 2 | 1/48 |
| -1 | 2 | 3/48 |
| 0 | 2 | 5/48 |
| 1 | 2 | 3/48 |
| 2 | 2 | 1/48 |

A fixed 64×64 blue-noise rank mask assigns pixels to four equally sized phases, and two cycles perform eight subpasses with two state generations. Each pixel gathers the residuals of the senders of its taps: a sender lies `dy` rows above and `dx` columns behind along the sender row's scan direction, mirrored on odd rows only for a serpentine scan. Out-of-image senders are omitted, and transparent pixels absorb incoming error and transmit zero residual.

Incoming error follows the CPU carry contract: a correction whose RGB length is below the deadzone is discarded, the corrected sample is limited by the soft (direction-preserving) or hard clamp with the configured threshold, and it is rounded to an integer. The lookup sees the byte projection of that sample, and the residual is measured from the rounded sample. Because the phased schedule and the fractional lookup differ from the sequential CPU scan, output is not byte-identical to the CPU kernel.

The final display decision against retained history, the deferred solve and resolve facets, and the plan lifetime follow the same contracts as the [GPU LSO2 component](lso2-gpu.md#delta-and-execution-ownership), implemented independently inside this component.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| JAJUNIG-01 | Raster scan accepts soft clamp threshold 1.01 and deadzone 5. | [tests/dither/jajuni-gpu/0001_accept_raster_soft.t](../../../../tests/dither/jajuni-gpu/0001_accept_raster_soft.t) |
| JAJUNIG-02 | Serpentine scan accepts hard clamp threshold 1 and zero deadzone. | [tests/dither/jajuni-gpu/0002_accept_serpentine_hard.t](../../../../tests/dither/jajuni-gpu/0002_accept_serpentine_hard.t) |
| JAJUNIG-03 | A different fixed-tap method is rejected with `SIXEL_BAD_ARGUMENT`. | [tests/dither/jajuni-gpu/0003_reject_other_method.t](../../../../tests/dither/jajuni-gpu/0003_reject_other_method.t) |
| JAJUNIG-04 | `lso2` is rejected with `SIXEL_BAD_ARGUMENT`. | [tests/dither/jajuni-gpu/0004_reject_lso2_method.t](../../../../tests/dither/jajuni-gpu/0004_reject_lso2_method.t) |
| JAJUNIG-05 | Unbounded clamp is rejected with `SIXEL_FEATURE_ERROR`. | [tests/dither/jajuni-gpu/0005_reject_unbounded_clamp.t](../../../../tests/dither/jajuni-gpu/0005_reject_unbounded_clamp.t) |
| JAJUNIG-06 | Perturbation is rejected with `SIXEL_FEATURE_ERROR`. | [tests/dither/jajuni-gpu/0006_reject_perturbation.t](../../../../tests/dither/jajuni-gpu/0006_reject_perturbation.t) |
| JAJUNIG-07 | Clamp threshold 0.5 is rejected with `SIXEL_BAD_ARGUMENT`. | [tests/dither/jajuni-gpu/0007_reject_low_clamp.t](../../../../tests/dither/jajuni-gpu/0007_reject_low_clamp.t) |
| JAJUNIG-08 | A negative deadzone is rejected with `SIXEL_BAD_ARGUMENT`. | [tests/dither/jajuni-gpu/0008_reject_negative_deadzone.t](../../../../tests/dither/jajuni-gpu/0008_reject_negative_deadzone.t) |
| JAJUNIG-09 | Discarding every carry gives equal raster and serpentine indexes. | [tests/dither/jajuni-gpu/0009_deadzone_scan_parity.t](../../../../tests/dither/jajuni-gpu/0009_deadzone_scan_parity.t) |
| JAJUNIG-10 | Live carry changes indexes relative to the same fixture with carry discarded. | [tests/dither/jajuni-gpu/0010_live_carry_changes_indexes.t](../../../../tests/dither/jajuni-gpu/0010_live_carry_changes_indexes.t) |

### Quality and verification limits

The fixture checks the configuration contract and that the carry changes the solve; it does not compare output with the CPU kernel, establish a perceptual-quality threshold, or rank throughput. The GPU case skips when Metal is unavailable.
