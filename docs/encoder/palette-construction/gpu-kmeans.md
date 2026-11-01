# GPU K-means

## Selection and platform requirements

`-Qkmeans:backend=cpu|gpu` selects the implementation of the primary Lloyd iteration. The default is `cpu`; `backend=gpu` (compact spelling `Ggpu`) is an explicit opt-in. `SIXEL_PALETTE_KMEANS_BACKEND=cpu|gpu` supplies the equivalent environment setting, with the suboption taking precedence. Other quantizers do not accept this suboption. `--gpu-policy` is the upper-level control for GPU execution, including this construction stage and palette application. Its default is `off`, so `backend=gpu` alone does not enable GPU execution. CLI GPU policy overrides `SIXEL_GPU_POLICY` independently of option order.

The GPU backend requires a build with Metal enabled and an available Metal device. Both the Autotools and Meson builds use their existing Metal feature detection. Builds without Metal keep the same CLI grammar. The effective behavior is:

| GPU policy | `backend=cpu` (default) | `backend=gpu` |
| --- | --- | --- |
| `off` (default) | CPU | CPU, without a GPU attempt |
| `auto` | CPU | GPU when supported; otherwise CPU from unchanged initial centers |
| `force` | CPU | GPU required; unsupported policy, platform, numeric range or failed GPU execution returns an error |

`force` does not select GPU k-means by itself, and it retains the existing GPU requirement for palette application. For an end-to-end forced GPU conversion, use a supported application configuration such as `--lookup-policy=none -dnone`. The `:palette_threshold` suboption governs only AUTO palette application; it does not gate an explicitly selected GPU k-means iteration. A forced construction failure also prevents the legacy byte dispatcher from silently trying another CPU quantizer.

```sh
img2sixel --gpu-policy=auto -Qkmeans:backend=gpu:seed=1 image.png
# Inspect the backend that actually ran, rather than only its configuration.
SIXEL_TRACE_TOPIC=kmeans_backend img2sixel -Gauto -Qkmeans:Ggpu -o /dev/null image.png
```

The trace emits one `LSXKMEANS1` record per primary restart, with `requested=cpu|gpu`, `effective=cpu|metal`, the actual iteration count and the GPU attempt status. `status=0x1000` means no supported GPU attempt completed, including an intentional policy or platform fallback; it is not the conversion's exit status. A successful GPU attempt reports `status=0x0000`.

## Pipeline boundary

The backend consumes the existing weighted point set in the selected `-X` clustering coordinates. Spatial sampling, transparency exclusion, color conversion, binning and initialization remain on CPU. A point set is uploaded once per restart; assignment, weighted reduction, empty-cluster repair and convergence decisions remain on GPU for the entire primary iteration. The CPU reads back only the resulting centers and iteration count. Final assignment, restart scoring, Ward merging, post-merge polishing, cover and export retain their existing implementation.

The Metal solver uses exhaustive nearest-center assignment, deterministic two-stage weighted sums and the same farthest-weighted-residual empty-cluster repair strategy as CPU Lloyd. It does not allocate a point-by-center distance matrix or require floating-point atomics. `prune` selects the CPU implementation, including CPU fallback; GPU execution uses a full center scan regardless of that setting. The iteration cap, minimum iteration count, threshold, primary polish count and restart policy remain active. Byte solvers retain bounded half-up rounding of centers; native float coordinates retain signed and extended values.

Residual histogram feedback (`feedback=1`) and active output-grid snap during the quantizer-iteration stage retain CPU execution with `auto`, and return an error with `force` plus `backend=gpu`. These policies are not silently disabled. Snap at other stages does not prevent primary GPU iteration. In particular, final output snap and CPU merge polishing remain independent of this selection.

The current GPU resource and numeric domain is at most 1048576 weighted points and 1024 internal centers, at most 116 primary iterations including polish, finite coordinate magnitudes no greater than 65536, nonnegative weights no greater than 4294967295 with at least one positive weight, and a finite nonnegative threshold no greater than 4294967295. Nonzero coordinates and weights smaller than `FLT_MIN` are unsupported. Limits apply to the prepared point set and internal center count, which can exceed the requested output palette size when final merging is enabled. Unsupported requests leave the initial centers unchanged; AUTO falls back and FORCE returns an error.

## Numerical and performance expectations

The GPU stores and accumulates FP32 coordinates and weights; the CPU uses double arithmetic. Arithmetic order and precision can change assignments near Voronoi boundaries, convergence and final palette entries. Fixed seeds do not imply bit-identical CPU/GPU output. The first use in a process also compiles Metal pipelines, so small images and short-lived CLI invocations can be slower than CPU execution. Compare cold and warm operation separately and measure the complete encoding as well as primary iteration. Point-set size, internal center count and iteration count determine the available work more directly than source image dimensions.

The exact numeric fixtures use representable integer or binary-fraction coordinates and weights whose sums remain exactly representable in FP32. Their assignment margins are large and their expected centroids follow independent weighted-mean calculations, so exact comparisons are justified. The weighted fixture crosses reduction-tile boundaries. Image output is tested separately against the standard `snake_64.png` input at 256 colors and MS-SSIM 0.98, following the [quality measurement policy](../../quality/measurement-policy.md) and [testing guide](../../testing/guide.md).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GK-01 | Weighted signed coordinates and fractional centroids across reduction tiles. | [tests/palette/kmeans/0001_weighted_lloyd.t](../../../tests/palette/kmeans/0001_weighted_lloyd.t) |
| GK-02 | Half-up byte-center rounding. | [tests/palette/kmeans/0002_byte_rounding.t](../../../tests/palette/kmeans/0002_byte_rounding.t) |
| GK-03 | Farthest-residual repair of an empty cluster. | [tests/palette/kmeans/0003_empty_cluster.t](../../../tests/palette/kmeans/0003_empty_cluster.t) |
| GK-04 | Minimum iteration count despite zero movement. | [tests/palette/kmeans/0004_iteration_floor.t](../../../tests/palette/kmeans/0004_iteration_floor.t) |
| GK-05 | Unsupported numeric input leaves centers and iteration output unchanged. | [tests/palette/kmeans/0005_range_fallback_atomicity.t](../../../tests/palette/kmeans/0005_range_fallback_atomicity.t) |
| GK-06 | Invalid backend syntax leaves the previously accepted option intact. | [tests/palette/kmeans/0006_option_atomicity.t](../../../tests/palette/kmeans/0006_option_atomicity.t) |
| GK-07 | An ordinary k-means request executes CPU by default. | [tests/palette/kmeans/0007_default_cpu.t](../../../tests/palette/kmeans/0007_default_cpu.t) |
| GK-08 | Explicit CPU selection overrides a GPU environment setting. | [tests/palette/kmeans/0008_cli_precedence.t](../../../tests/palette/kmeans/0008_cli_precedence.t) |
| GK-09 | AUTO executes an opted-in Metal iteration even below the palette-apply threshold. | [tests/palette/kmeans/0009_gpu_dispatch.t](../../../tests/palette/kmeans/0009_gpu_dispatch.t) |
| GK-10 | Residual feedback retains CPU execution. | [tests/palette/kmeans/0010_feedback_fallback.t](../../../tests/palette/kmeans/0010_feedback_fallback.t) |
| GK-11 | Active iteration snap retains CPU execution. | [tests/palette/kmeans/0011_snap_fallback.t](../../../tests/palette/kmeans/0011_snap_fallback.t) |
| GK-12 | An unavailable Metal device retains CPU execution. | [tests/palette/kmeans/0012_platform_fallback.t](../../../tests/palette/kmeans/0012_platform_fallback.t) |
| GK-14 | Convergence state is isolated between requests. | [tests/palette/kmeans/0014_request_isolation.t](../../../tests/palette/kmeans/0014_request_isolation.t) |
| GK-15 | Native float Oklab palette generation executes Metal. | [tests/palette/kmeans/0015_native_float_dispatch.t](../../../tests/palette/kmeans/0015_native_float_dispatch.t) |
| GK-16 | Compact and environment backend settings reach the same stored option and image output. | [tests/cli/quantizer-options/0152_quantize_kmeans_backend_image_regression.t](../../../tests/cli/quantizer-options/0152_quantize_kmeans_backend_image_regression.t) |
| GK-17 | CLI OFF vetoes the GPU backend and a FORCE environment default. | [tests/palette/kmeans/0016_policy_off.t](../../../tests/palette/kmeans/0016_policy_off.t) |
| GK-18 | FORCE rejects feedback without a byte median-cut fallback. | [tests/palette/kmeans/0017_force_feedback.t](../../../tests/palette/kmeans/0017_force_feedback.t) |
| GK-19 | FORCE rejects an unavailable GPU without a byte median-cut fallback. | [tests/palette/kmeans/0018_force_platform.t](../../../tests/palette/kmeans/0018_force_platform.t) |
| GK-20 | FORCE executes a supported GPU construction and application. | [tests/palette/kmeans/0019_force_dispatch.t](../../../tests/palette/kmeans/0019_force_dispatch.t) |
| GK-21 | The default OFF policy vetoes an explicit GPU backend. | [tests/palette/kmeans/0020_policy_default_off.t](../../../tests/palette/kmeans/0020_policy_default_off.t) |
| GK-22 | An AUTO environment default enables the GPU backend. | [tests/palette/kmeans/0021_policy_env_auto.t](../../../tests/palette/kmeans/0021_policy_env_auto.t) |
| GK-23 | FORCE leaves an explicitly selected CPU k-means backend on CPU. | [tests/palette/kmeans/0022_force_cpu_backend.t](../../../tests/palette/kmeans/0022_force_cpu_backend.t) |
| GK-24 | FORCE rejects active quantizer-iteration snap. | [tests/palette/kmeans/0023_force_iteration_snap.t](../../../tests/palette/kmeans/0023_force_iteration_snap.t) |

### Quality regression tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GK-13 | An observed Metal execution meets MS-SSIM 0.98 at 256 colors. | [tests/palette/kmeans/0013_gpu_quality.t](../../../tests/palette/kmeans/0013_gpu_quality.t) |

### Coverage boundary

GPU-specific tests skip only when a Metal device is unavailable, and fail when an available device cannot execute the implementation. Platform fallback is exercised separately by a build or machine without Metal. The fixtures establish their stated contracts, not bitwise agreement with CPU arithmetic on arbitrary images or universal GPU speedups. Runtime resource exhaustion and driver command failures follow the same AUTO fallback or FORCE error rule without publishing partial GPU output, but are not deterministically injected by this suite.
