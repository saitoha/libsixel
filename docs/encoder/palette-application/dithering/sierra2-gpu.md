# Dithering: `sierra:variant=2` on the GPU

The GPU component of `sierra:variant=2` (`dither/sierra2.gpu`) runs the two-row Sierra kernel as a spatial error-diffusion solver for RGB8 images in gamma RGB. It is a separate component from the CPU `sierra:variant=2` dither and from every other GPU diffusion component; it shares no solver code with them. The [dithering reference](README.md) owns the general separation between dither and palette lookup.

## Selection and supported inputs

```sh
img2sixel --gpu-policy=auto --precision-policy=8bit \
  --lookup-policy=ordered -d sierra:variant=2 image.png
```

The encoder selects this component when the GPU policy admits an available GPU, the image is RGB8 gamma, and the request has no perturbation and no `clamp=none` carry. Otherwise `sierra:variant=2` stays on its CPU component. The dither keeps `method_for_diffuse` as `sierra:variant=2` and marks the frame's diffusion as running on the GPU; the trace reports `diffuse=sierra2` with `diffuse_on_gpu=1`. The component accepts raster and serpentine scans and keeps the CPU raster default.

Metal, gamma RGB8 source storage and the indexed output component path are required. A GPU policy of `off`, an unavailable device, or an incompatible lookup returns an error before output rather than substituting another dither. All named GPU lookup policies accept its fractional corrected queries under their [declared search semantics](../lookup-policy/README.md).

## Kernel and carry

The kernel weights over the denominator 16 are baked into the component's shader:

| dx | dy | weight |
| --- | --- | --- |
| 1 | 0 | 4/16 |
| 2 | 0 | 3/16 |
| -2 | 1 | 1/16 |
| -1 | 1 | 2/16 |
| 0 | 1 | 3/16 |
| 1 | 1 | 2/16 |
| 2 | 1 | 1/16 |

A fixed 64×64 blue-noise rank mask assigns pixels to four equally sized phases, and two cycles perform eight subpasses with two state generations. Each pixel gathers the residuals of the senders of its taps: a sender lies `dy` rows above and `dx` columns behind along the sender row's scan direction, mirrored on odd rows only for a serpentine scan. Out-of-image senders are omitted, and transparent pixels absorb incoming error and transmit zero residual.

Incoming error follows the CPU carry contract: a correction whose RGB length is below the deadzone is discarded, the corrected sample is limited by the soft (direction-preserving) or hard clamp with the configured threshold, and it is rounded to an integer. The lookup sees the byte projection of that sample, and the residual is measured from the rounded sample. Because the phased schedule and the fractional lookup differ from the sequential CPU scan, output is not byte-identical to the CPU kernel.

The final display decision against retained history, the deferred solve and resolve facets, and the plan lifetime follow the same contracts as the [GPU LSO2 component](lso2-gpu.md#delta-and-execution-ownership), implemented independently inside this component.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| SIERRA2G-01 | The component accepts only `sierra:variant=2` with bounded carry limits, rejects perturbation and `clamp=none`, reduces to the plain lookup in either scan when every carry is discarded, and differs from it with live carry. | [tests/dither/sierra2-gpu/0001_sierra2_gpu_carry.t](../../../../tests/dither/sierra2-gpu/0001_sierra2_gpu_carry.t) |

### Quality and verification limits

The fixture checks the configuration contract and that the carry changes the solve; it does not compare output with the CPU kernel, establish a perceptual-quality threshold, or rank throughput. The GPU case skips when Metal is unavailable.
