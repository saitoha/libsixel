# GPU policy

This reference is for CLI users and library integrators configuring `-G` / `--gpu-policy` in `img2sixel` and `sixel2png`. It owns the policy values, the size thresholds, which speed checks `auto` applies at each GPU stage, and when a stage falls back to the CPU or reports an error. The stage documents own what each GPU stage computes: [GPU k-means](../encoder/palette-construction/gpu-kmeans.md), [lookup policy](../encoder/palette-application/lookup-policy/README.md#options), [dequantization](../decoder/dequantization/README.md), and the [encoder execution map](../encoder/execution-map/README.md#gpu-construction-and-load-headroom), which owns the Vulkan load headroom rule.

## Options

```sh
img2sixel -G auto image.png
img2sixel -G auto:P100000:B0 animation.gif
img2sixel --gpu-policy=offload animation.gif
sixel2png -G auto:dequant_threshold=300 -d lso_undither:Vlight < image.six > image.png
```

| Value | Library constant | Meaning |
| --- | --- | --- |
| `off` (default) | `SIXEL_GPU_POLICY_OFF` | Every stage runs on the CPU. |
| `auto` | `SIXEL_GPU_POLICY_AUTO` | A stage uses the GPU when it supports the input and its speed checks expect the GPU to be faster; otherwise it runs on the CPU. |
| `offload` | `SIXEL_GPU_POLICY_OFFLOAD` | A stage uses the GPU whenever it supports the input, even when the GPU is slower, so that the CPU is left free for other work; unsupported input and GPU failures run on the CPU. |
| `force` | `SIXEL_GPU_POLICY_FORCE` | The selected stages must run on the GPU; unsupported input and GPU failures are errors. |

`SIXEL_GPU_POLICY` sets the default value; `-G` takes precedence. The encoder accepts two size suboptions and the decoder one. Each is a minimum pixel count for `auto` and has an environment counterpart that the suboption overrides:

| Suboption | Short form | Environment | Default | Stage |
| --- | --- | --- | --- | --- |
| `palette_threshold=PIXELS` | `PPIXELS` | `SIXEL_GPU_PALETTE_THRESHOLD` | 262144 | Positional palette application and progressive refinement |
| `body_threshold=PIXELS` | `BPIXELS` | `SIXEL_GPU_BODY_THRESHOLD` | 262144 | SIXEL body spelling in thread-disabled WebGPU builds |
| `dequant_threshold=PIXELS` | `DPIXELS` | `SIXEL_GPU_DEQUANT_THRESHOLD` | 262144 | Decoder dequantization |

## Contract

`auto` and `offload` differ only in how a supported stage chooses the device. Both fall back to the CPU, without an error and without changing the output contract of the CPU path, when the stage does not support the input, no device is available, or a GPU attempt fails before it publishes output. `force` reports those cases as errors; the [GPU k-means](../encoder/palette-construction/gpu-kmeans.md) and [dequantization](../decoder/dequantization/README.md) documents list the stage-specific rejections.

`offload` ignores every speed check in the next section, including explicitly set size thresholds: a suboption such as `offload:B1048576` is accepted and has no effect. `offload` and `force` resolve an automatic working space and precision to gamma RGB888 byte application, because the GPU palette stages accept only that representation; an explicit precision, working colorspace, native output or GPU k-means construction is preserved. With high-bit-depth input this can make `offload` output differ from `auto` output.

## Speed checks

Under `auto`, each GPU stage applies the checks in this table before it uses the device. `offload` skips every one of them. `force` skips some and keeps the others, as its column shows.

| Stage | Checks under `auto` | Under `force` |
| --- | --- | --- |
| Palette application, progressive refinement | At least `palette_threshold` pixels | Skipped |
| Decoder dequantization | At least `dequant_threshold` pixels | Skipped |
| Solid sample-region scan | At least 1048576 clipped pixels on WebGPU and 524288 elsewhere, or the `-j` `sample_min` value; Vulkan also requires load headroom | Kept |
| Heckbert median cut (Vulkan) | Load headroom | Skipped |
| Ward merge-pair selection | At least 384 clusters, a prepared device pipeline, and at least 64 merges | Skipped |
| K-means with `backend=auto` | A workload large enough to amortize submission and an already initialized device pipeline | Kept |
| SIXEL body (Vulkan) | Load headroom decides between spelling the body on the device and composing it only | Kept |
| SIXEL body (WebGPU) | At least `body_threshold` pixels, and the CPU/GPU chain comparison described in [encoder threading](../threading/encoder.md#gpu-band-composition) | Skipped |

Load headroom is the [Vulkan device load rule](../encoder/execution-map/README.md#gpu-construction-and-load-headroom): the stage uses the device only while the measured device load stays low. Under `offload` it always selects the device.

## Limitations

The fused resident palette path described in the [encoder execution map](../encoder/execution-map/README.md#forced-resident-palette-path) runs only under `force`. Under `offload` the same frames use the ordinary GPU stages. Stages without a GPU implementation for the current backend run on the CPU under every policy; the [acceleration ledger](../acceleration/ledger.md) lists them.

## Implementation map

- [`include/sixel.h.in`](../../include/sixel.h.in) defines the policy constants; the [option registry](../../src/options-registry.c) maps the values and suboptions.
- [`src/gpu-statistics.c`](../../src/gpu-statistics.c) implements the load headroom step, which selects the device at once under `offload`.
- [`src/gpu-palette.c`](../../src/gpu-palette.c), [`src/gpu-dequant.c`](../../src/gpu-dequant.c), [`src/progressive.c`](../../src/progressive.c) and [`src/progressive-gpu.c`](../../src/progressive-gpu.c) apply the size thresholds and fallback rules.
- [`src/filter-sample.c`](../../src/filter-sample.c), [`src/palette-common-merge.c`](../../src/palette-common-merge.c) and [`src/palette-kmeans-build.c`](../../src/palette-kmeans-build.c) apply the sampling, Ward and k-means checks.
- [`src/encoder-core-indexed.inc.h`](../../src/encoder-core-indexed.inc.h) admits GPU body spelling; [`src/encoder.c`](../../src/encoder.c) resolves byte application and the body threshold.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GP-01 | `offload` selects the device before any load observation and keeps it under full device load. | [tests/palette/sampling/0107_gpu_headroom.t](../../tests/palette/sampling/0107_gpu_headroom.t) |
| GP-02 | `force` and `offload` send a zero body threshold to the body stage; the `auto` default is 262144. | [tests/cli/option-syntax/0095_gpu_body_threshold_image_regression.t](../../tests/cli/option-syntax/0095_gpu_body_threshold_image_regression.t) |
| GP-03 | WebGPU palette application under `offload` runs on the device below the palette threshold and matches CPU bytes. | [tests/platform/webgpu/0001_palette_byte_equivalence.t](../../tests/platform/webgpu/0001_palette_byte_equivalence.t) |
| GP-04 | `auto` and `offload` fall back to CPU-identical output for outputs the WebGPU path does not support. | [tests/platform/webgpu/0004_auto_output_fallback.t](../../tests/platform/webgpu/0004_auto_output_fallback.t) |
| GP-05 | WebGPU body spelling under `force` and `offload` runs on every eligible frame regardless of size and threshold, while `auto` measures the CPU chain. | [tests/platform/webgpu/0005_body_spell_equivalence.t](../../tests/platform/webgpu/0005_body_spell_equivalence.t) |

### Coverage boundary

The WebGPU tests run only with a thread-disabled WebGPU build and a WebGPU runtime, and skip otherwise. Metal and Vulkan stage choices under `offload` are covered by the headroom unit test and by each stage's own suite, not by a device-level test per stage.
