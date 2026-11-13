# GPU K-means

## Selection and platform requirements

`-Qkmeans:backend=cpu|gpu|auto` selects the implementation of the primary Lloyd iteration. The default is `cpu`. `backend=gpu` (compact spelling `Ggpu`) explicitly prefers GPU iteration; `backend=auto` (`Gauto`) additionally requires already initialized pipelines and sufficient construction workload. CPU remains the default: calibration establishes selected warm wins on one machine, not universal acceleration or playback validation. `SIXEL_PALETTE_KMEANS_BACKEND=cpu|gpu|auto` supplies the equivalent environment setting, with the suboption taking precedence. Other quantizers do not accept this suboption. `--gpu-policy` is the upper-level control for GPU execution, including this construction stage and palette application. Its default is `off`, so `backend=gpu` alone does not enable GPU execution. CLI GPU policy overrides `SIXEL_GPU_POLICY` independently of option order.

The GPU backend requires Metal or Vulkan support and an available device. Both Autotools and Meson use the selected backend's feature detection. Builds without either backend keep the same CLI grammar. The effective behavior is:

| GPU policy | `backend=cpu` (default) | `backend=gpu` | `backend=auto` |
| --- | --- | --- | --- |
| `off` (default) | CPU | CPU, without a GPU attempt | CPU |
| `auto` | CPU | GPU when supported; otherwise CPU from unchanged initial centers | GPU for initialized pipelines and eligible workloads above the construction floor; otherwise CPU |
| `force` | CPU | GPU required; incompatible policy, platform, numeric range or execution failure returns an error | Cold pipelines, small eligible workloads and unavailable devices stay on CPU; incompatible policies, unsafe numeric inputs and failed GPU attempts return an error |

`force` does not select GPU k-means by itself. GPU palette construction can feed CPU native float application, including named destinations and playback. The native representation and output conversion remain the CPU path. Byte palette application retains its own eligibility and FORCE rules. For an end-to-end forced GPU conversion, use a supported application configuration such as `--lookup-policy=none -dnone`. The `:palette_threshold` suboption governs only AUTO palette application; it does not gate an explicitly selected GPU k-means iteration. A forced construction failure also prevents the legacy byte dispatcher from silently trying another CPU quantizer.

```sh
img2sixel --gpu-policy=auto -Qkmeans:backend=gpu:seed=1 image.png
# Inspect the backend that actually ran, rather than only its configuration.
SIXEL_TRACE_TOPIC=kmeans_backend img2sixel -Gauto -Qkmeans:Ggpu -o /dev/null image.png
```

The `kmeans_backend` trace emits one `LSXKMEANS1` record per completed primary restart, or `effective=rejected` with a reason when FORCE rejects the request. `requested=cpu|gpu|auto` records the option; `effective=cpu|metal|vulkan` and `gpu_used=0|1` record execution. `reason` distinguishes `eligible`, `cpu-requested`, `policy-off`, `workload-small`, `pipeline-cold`, `feedback`, `iteration-snap`, `explicit-prune`, `numeric-domain`, `device-unavailable`, `gpu-init`, `buffer-allocation` and `command-or-numeric-failure`. `M` is the binned point count, `K` the internal center count, `restart` the one-based restart index, `prune` the effective assignment algorithm, `iterations` the executed count, `limit` the resolved cap, `cpu_workspace` whether CPU pruning storage has been allocated, and `sse` the CPU double score. `speedup=unmeasured` explicitly separates GPU use from evidence of acceleration. `status=0x1000` means no supported GPU attempt completed, including an intentional policy or platform fallback; it is not the conversion's exit status. A successful GPU attempt reports `status=0x0000`.

With `SIXEL_TRACE_TOPIC=kmeans_backend`, `LSXKMTIME1` reports the Metal attempt wall time, device, executed iterations and empty repairs. `LSXKMPHASE1` separates CPU initialization, allocation, upload, command encoding, submission, wait, validation, publication and release. `LSXKMSTAGE1` reports command-buffer `GPUStartTime`/`GPUEndTime` after completion; unavailable timestamps have `gpu_valid=0` and `gpu_ms=-1`. `LSXKMCALL1` includes request validation and the complete backend call; `LSXKMRESTART1` separates CPU seed selection, solving and double-precision restart scoring. No CPU center reduction or convergence readback occurs between iterations. Buffers are allocated per restart and released after the attempt; pipeline compilation is cached per process.

For Metal phase diagnosis, additionally set `SIXEL_KMEANS_GPU_PROFILE=stages`: each assignment, partial sum, reduction and repair/convergence dispatch gets its own command buffer, committed in queue order without explicit intermediate completion waits. Acquiring a command buffer can still block when the queue has no free command-buffer slot; stage `encode_ms` includes this acquisition time. This diagnostic mode changes submission overhead; its timings must not determine AUTO admission. The [measurement script](../../../tools/measure-gpu-kmeans.sh) first runs untraced calibration for throughput, then normal batch tracing and separate stage tracing, and prints per-phase totals while preserving per-iteration records. CPU encoding/submission overlaps GPU execution in stage mode, so CPU and GPU times are not additive. `--full` also runs staticcheck and the complete test suite; `--baseline COMMIT` measures an instrumented source archive inside the same build directory before the current sources. A missing device produces no calibration table and exits 77.

## Pipeline boundary

The backend consumes the existing weighted point set in the selected `-X` clustering coordinates. Spatial sampling, transparency exclusion, color conversion, binning and initialization remain on CPU. A point set is uploaded once per restart; assignment, weighted reduction, empty-cluster repair and convergence decisions remain on GPU for the entire primary iteration. The CPU reads back the resulting centers, memberships and iteration count. Final assignment, restart scoring, Ward merging, post-merge polishing, cover and export retain their existing implementation.

The Metal and Vulkan solvers use exhaustive nearest-center assignment, deterministic two-stage weighted sums and the same farthest-weighted-residual empty-cluster repair strategy as CPU Lloyd. Each 512-point tile builds ordered membership lists in 14 KiB of threadgroup storage, adds each weighted point once in ascending sample order, and writes one partial sum per center. The second reduction adds tiles in ascending order. Empty repair scans one maximum-residual candidate per tile and refreshes only the consumed donor's tile; equal residuals still select the lowest sample index, including when every residual is zero. It does not allocate a point-by-center distance matrix or require floating-point atomics. Explicit `prune=elkan|yinyang`, including environment settings, requires CPU iteration under AUTO policy and conflicts with GPU selection under FORCE. The default `prune=auto` resolves to Hamerly on CPU and does not block GPU selection; `none` and `hamerly` also permit GPU execution, which uses a full center scan. CPU pruning tables are allocated only when CPU iteration actually starts, including fallback and post-Ward polishing. The iteration cap, minimum iteration count, threshold, primary polish count and restart policy remain active. Byte solvers retain bounded half-up rounding of centers; native float coordinates retain signed and extended values.

Residual histogram feedback (`feedback=1`) and active output-grid snap during the quantizer-iteration stage retain CPU execution with `auto`, and return an error with `force` plus `backend=gpu|auto`. These policies are not silently disabled. Snap at other stages does not prevent primary GPU iteration. In particular, final output snap and CPU merge polishing remain independent of this selection.

The current GPU resource and numeric domain is at most 1048576 weighted points and 1024 internal centers, at most 116 primary iterations including polish, finite coordinate magnitudes no greater than 65536, nonnegative weights no greater than 4294967295 with at least one positive weight, and a finite nonnegative threshold no greater than 4294967295.

Positive weights below $2^{-16}$, nonzero coordinate magnitudes below $2^{-24}$, total positive weight divided by the minimum positive weight above $2^{24}$, and positive thresholds below `FLT_MIN` are unsupported. A nonconstant coordinate range smaller than `64 * FLT_EPSILON` times its largest magnitude is rejected to avoid collapsing a narrow cloud at a large offset. The lower bounds keep squared FP32 differences and weighted residuals away from underflow. GPU assignment also rejects a nonzero FP32 difference whose squared distance or positive weighted residual underflows to zero.

Before publication, centers must be finite and lie within the coordinate envelope of samples and seeds, allowing the rounding margin and a relative envelope allowance of 0.0001. Limits apply to the prepared point set and internal center count, which can exceed the requested output palette size when final merging is enabled. Unsupported requests leave the initial centers unchanged; AUTO falls back and FORCE returns an error.

## Numerical and performance expectations

The GPU stores and accumulates FP32 coordinates and weights; the CPU uses double arithmetic. Arithmetic order and precision can change assignments near Voronoi boundaries, convergence and final palette entries. Fixed seeds require repeatability only on the same machine and build, not bit-identical CPU/GPU output. Assignment keeps the previous membership on an exact tie; only a strictly smaller distance switches owners, without an epsilon. The first assignment uses the lowest center index. Equal repair residuals pick the lowest sample index, and equal restart scores retain the earlier restart. More centers than unique colors may leave duplicate centers or empty clusters; finite output and the requested color-count bound are required, not a nonempty cluster for every center. The first use in a process also prepares GPU pipelines, so small images and short-lived CLI invocations can be slower than CPU execution. Compare cold and warm operation separately and measure the complete encoding as well as primary iteration. Point-set size, internal center count and iteration count determine the available work more directly than source image dimensions.

The exact numeric fixtures use representable integer or binary-fraction coordinates and weights whose sums remain exactly representable in FP32. Their assignment margins are large and their expected centroids follow independent weighted-mean calculations, so exact comparisons are justified. The weighted fixture crosses reduction-tile boundaries. Image output is tested separately against the standard `snake_192.png` input at 256 colors and MS-SSIM 0.98, following the [quality measurement policy](../../quality/measurement-policy.md) and [testing guide](../../testing/guide.md).

The numerical validation gate is `SSE_gpu <= 1.0025 * SSE_cpu`, with both scores computed in double on the same prepared point set, initialization seed and iteration settings. A zero CPU score requires a zero GPU score. The 0.25% gate exceeds the measured maximum absolute relative difference of 0.180185% by 0.069815 percentage points (about 38.7% headroom); the maximum positive degradation was only 0.017171%. This is a fixture regression budget, not a proven all-input approximation bound or a runtime CPU/GPU comparison. It does not relax any requested clustering setting.

The opt-in AUTO construction floor requires all of `M >= 12288`, `K >= 64`, `I >= 12` and `M * K * I >= 67108864`. `M` is the prepared point count, `K` the internal center count, and `I` the guaranteed minimum iteration count after primary polish and clamping to the resolved cap. The maximum iteration setting alone cannot establish this work: convergence may stop much earlier. AUTO never changes sampling, binning, restart count, pruning or convergence settings.

AUTO additionally requires the process's four GPU pipelines to have been fully initialized. A nonblocking readiness probe returns CPU while initialization is absent or in progress, including under GPU policy FORCE, with trace reason `pipeline-cold`. It neither starts compilation nor waits for another caller to finish it. An explicit `backend=gpu` request can initialize the process cache; an AUTO-only process stays on CPU. No speculative warm-up, persistent disk cache or future-frame prediction is involved.

Calibration on 2026-11-09 used Mac15,10 (`sysctl -n hw.model`), Apple M3 Max (`sysctl -n machdep.cpu.brand_string`), macOS 27.0.1, and `./build-gpu-kmeans/measure-gpu-kmeans.sh --baseline 422f2b19d --full` (now `tools/measure-gpu-kmeans.sh`). The baseline is `422f2b19d`; the measured optimized kernels are `e9f5cbbaf`. The script preserves raw `KMCAL` and per-iteration timestamps. The table uses only its untraced construction timings, including CPU preprocessing, seed selection, restart scoring and export. CPU uses Hamerly. Each fixture uses all loaded samples with binning disabled, one restart, and `(K, minimum = maximum iterations, seed)` of `(16, 4, 1)`, `(64, 12, 7)` and `(256, 24, 42)`.

| Fixture | M | K | I | CPU ms | Baseline GPU warm ms | GPU first at K ms | GPU warm ms | Relative SSE | Warm AUTO |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| snake | 4096 | 16 | 4 | 1.592 | 2.900 | 99.428 | 4.244 | +1.43899377e-08 | CPU |
| snake | 4096 | 64 | 12 | 3.021 | 5.818 | 7.861 | 4.671 | -4.75370809e-09 | CPU |
| snake | 4096 | 256 | 24 | 14.145 | 13.196 | 11.974 | 12.195 | +1.71708524e-04 | CPU |
| gradient | 12288 | 16 | 4 | 1.641 | 2.218 | 8.230 | 2.980 | +4.23182946e-05 | CPU |
| gradient | 12288 | 64 | 12 | 7.820 | 6.221 | 9.221 | 5.340 | +1.28508501e-05 | CPU |
| gradient | 12288 | 256 | 24 | 35.330 | 21.318 | 21.612 | 19.984 | +2.36142402e-05 | GPU |
| rare | 270000 | 16 | 4 | 29.409 | 84.581 | 32.231 | 22.798 | -9.11548498e-05 | CPU |
| rare | 270000 | 64 | 12 | 115.673 | 555.412 | 84.180 | 85.099 | +7.49274882e-05 | GPU |
| rare | 270000 | 256 | 24 | 477.391 | 4382.464 | 336.713 | 335.109 | -1.80184749e-03 | GPU |

Only the first K=16 call in each fixture process is process-cold; the K=64 and K=256 first-at-K calls already reuse pipelines. The 99.428 ms startup cannot be extrapolated into a measured cold win for large inputs. Cold AUTO therefore always uses CPU. GPU SSE was bit-identical before/after optimization in all nine cases in each of the three trace modes, and repeated output passed the fixed-seed check.

The admission floor excludes K=16 and the small M=4096 cases, including the modest snake K=256 win. The gradient K=64 warm win is also excluded because its first-at-K call (9.221 ms) loses to CPU (7.820 ms). Its work is 9437184, below the chosen conservative power-of-two floor of 67108864; gradient K=256 has 75497472 and passes. The three admitted samples win in both first-at-K and repeated measurements, with warm speedups of 1.768x, 1.359x and 1.425x. These floors interpolate a sparse calibration, not a fitted universal crossover: other chips, input distributions and unmeasured sizes remain unverified. AUTO stays opt-in and the default remains CPU.

For rare K=256, stage tracing of the second GPU call (attempt 6, 24 iterations, 192 empty repairs) localizes the improvement to empty repair. Times are GPU milliseconds summed over dispatches; stage tracing and normal batch tracing are separate runs and must not be added together.

| Phase | Baseline GPU ms | Optimized GPU ms |
| --- | ---: | ---: |
| Assignment | 7.002874 | 7.940874 |
| Tile partial sums | 6.187751 | 9.157289 |
| Center reduction | 3.647793 | 3.627708 |
| Update / repair / convergence | 3959.012041 | 28.029750 |
| Normal batch command, separate run | 4079.743208 | 46.503250 |

The first update alone accounts for 3956.877000 ms before and 26.016833 ms after optimization. The ordered tile reduction preserves addition order; these measurements do not establish a speedup for that phase. Normal batch CPU phases before/after are allocation 0.078/0.068 ms, upload 0.407/0.387 ms, encode 0.060/0.063 ms, submission 0.008/0.009 ms, completion wait 4081.860/47.181 ms, validation 0.378/1.040 ms and publication 0.067/0.083 ms. There is one submission per restart and no per-iteration CPU reduction, convergence check or readback. Optimized CPU seed selection and restart scoring take 146.440 and 69.928 ms in that batch-traced call; the complete untraced construction table is the performance decision boundary.

In stage mode, attempt 6's iteration-17 update has `encode_ms=26.019`, matching iteration 1's 26.017 ms GPU update; attempt 5 similarly has 24.880 versus 24.781 ms. This is consistent with command-buffer acquisition waiting behind the long first update when the queue fills, rather than 25 ms of CPU update arithmetic. The ordinary batch encodes in 0.063 ms. Acquisition is included in the encode timer, so this attribution is an inference from timing and submission order, not a separately timed API call.

A started GPU iteration batch is drained to completion. Result adoption and palette-store publication retain their cancellation/epoch checks; a cancelled or obsolete result cannot become the active palette. LATEST selects from completed snapshots through the existing store. Exact sequence equality across unconstrained CPU/GPU timings is not established: completion order can change which snapshot is available at selection time.

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
| GK-09 | AUTO executes an opted-in GPU iteration even below the palette-apply threshold. | [tests/palette/kmeans/0009_gpu_dispatch.t](../../../tests/palette/kmeans/0009_gpu_dispatch.t) |
| GK-10 | Residual feedback retains CPU execution. | [tests/palette/kmeans/0010_feedback_fallback.t](../../../tests/palette/kmeans/0010_feedback_fallback.t) |
| GK-11 | Active iteration snap retains CPU execution. | [tests/palette/kmeans/0011_snap_fallback.t](../../../tests/palette/kmeans/0011_snap_fallback.t) |
| GK-12 | An unavailable GPU device retains CPU execution. | [tests/palette/kmeans/0012_platform_fallback.t](../../../tests/palette/kmeans/0012_platform_fallback.t) |
| GK-14 | Convergence state is isolated between requests. | [tests/palette/kmeans/0014_request_isolation.t](../../../tests/palette/kmeans/0014_request_isolation.t) |
| GK-15 | Native float Oklab palette generation executes the selected GPU backend. | [tests/palette/kmeans/0015_native_float_dispatch.t](../../../tests/palette/kmeans/0015_native_float_dispatch.t) |
| GK-16 | Compact and environment backend settings reach the same stored option and image output. | [tests/cli/quantizer-options/0152_quantize_kmeans_backend_image_regression.t](../../../tests/cli/quantizer-options/0152_quantize_kmeans_backend_image_regression.t) |
| GK-17 | CLI OFF vetoes the GPU backend and a FORCE environment default. | [tests/palette/kmeans/0016_policy_off.t](../../../tests/palette/kmeans/0016_policy_off.t) |
| GK-18 | FORCE rejects feedback without a byte median-cut fallback. | [tests/palette/kmeans/0017_force_feedback.t](../../../tests/palette/kmeans/0017_force_feedback.t) |
| GK-19 | FORCE rejects an unavailable GPU without a byte median-cut fallback. | [tests/palette/kmeans/0018_force_platform.t](../../../tests/palette/kmeans/0018_force_platform.t) |
| GK-20 | FORCE executes a supported GPU construction and application. | [tests/palette/kmeans/0019_force_dispatch.t](../../../tests/palette/kmeans/0019_force_dispatch.t) |
| GK-21 | The default OFF policy vetoes an explicit GPU backend. | [tests/palette/kmeans/0020_policy_default_off.t](../../../tests/palette/kmeans/0020_policy_default_off.t) |
| GK-22 | An AUTO environment default enables the GPU backend. | [tests/palette/kmeans/0021_policy_env_auto.t](../../../tests/palette/kmeans/0021_policy_env_auto.t) |
| GK-23 | FORCE leaves an explicitly selected CPU k-means backend on CPU. | [tests/palette/kmeans/0022_force_cpu_backend.t](../../../tests/palette/kmeans/0022_force_cpu_backend.t) |
| GK-24 | FORCE rejects active quantizer-iteration snap. | [tests/palette/kmeans/0023_force_iteration_snap.t](../../../tests/palette/kmeans/0023_force_iteration_snap.t) |
| GK-0047 | Plain assignment keeps the previous owner on an exact tie. | [tests/palette/kmeans/0047_plain_ties.t](../../../tests/palette/kmeans/0047_plain_ties.t) |
| GK-0048 | Hamerly assignment keeps the previous owner on an exact tie. | [tests/palette/kmeans/0048_hamerly_ties.t](../../../tests/palette/kmeans/0048_hamerly_ties.t) |
| GK-0049 | Elkan assignment keeps the previous owner on an exact tie. | [tests/palette/kmeans/0049_elkan_ties.t](../../../tests/palette/kmeans/0049_elkan_ties.t) |
| GK-0050 | Yinyang assignment keeps the previous owner on an exact tie. | [tests/palette/kmeans/0050_yinyang_ties.t](../../../tests/palette/kmeans/0050_yinyang_ties.t) |
| GK-0051 | GPU iteration keeps the previous owner after a center update creates a tie. | [tests/palette/kmeans/0051_gpu_ties.t](../../../tests/palette/kmeans/0051_gpu_ties.t) |
| GK-0052 | Tiny coordinates are rejected without modifying caller outputs. | [tests/palette/kmeans/0052_tiny_coordinate.t](../../../tests/palette/kmeans/0052_tiny_coordinate.t) |
| GK-0053 | Tiny positive weights are rejected without modifying caller outputs. | [tests/palette/kmeans/0053_tiny_weight.t](../../../tests/palette/kmeans/0053_tiny_weight.t) |
| GK-0054 | A narrow cloud at a large offset is rejected atomically. | [tests/palette/kmeans/0054_narrow_range.t](../../../tests/palette/kmeans/0054_narrow_range.t) |
| GK-0055 | An all-zero-weight request is rejected atomically. | [tests/palette/kmeans/0055_zero_weights.t](../../../tests/palette/kmeans/0055_zero_weights.t) |
| GK-0056 | Excessive total-to-minimum weight ratio is rejected atomically. | [tests/palette/kmeans/0056_weight_condition.t](../../../tests/palette/kmeans/0056_weight_condition.t) |
| GK-0057 | Zero residuals and duplicate centers remain finite. | [tests/palette/kmeans/0057_zero_residual.t](../../../tests/palette/kmeans/0057_zero_residual.t) |
| GK-0058 | Zero-weight members do not corrupt GPU reduction or repair. | [tests/palette/kmeans/0058_zero_weight_members.t](../../../tests/palette/kmeans/0058_zero_weight_members.t) |
| GK-0059 | More centers than unique colors and many empty clusters remain bounded. | [tests/palette/kmeans/0059_many_empty.t](../../../tests/palette/kmeans/0059_many_empty.t) |
| GK-0060 | Injected GPU initialization failure falls back and a later request recovers. | [tests/palette/kmeans/0060_init_auto_recovery.t](../../../tests/palette/kmeans/0060_init_auto_recovery.t) |
| GK-0061 | FORCE rejects injected initialization failure and a later request recovers. | [tests/palette/kmeans/0061_init_force_recovery.t](../../../tests/palette/kmeans/0061_init_force_recovery.t) |
| GK-0062 | Partial buffer allocation failure falls back atomically and recovers. | [tests/palette/kmeans/0062_buffer_auto_recovery.t](../../../tests/palette/kmeans/0062_buffer_auto_recovery.t) |
| GK-0063 | FORCE rejects partial buffer allocation failure and recovers. | [tests/palette/kmeans/0063_buffer_force_recovery.t](../../../tests/palette/kmeans/0063_buffer_force_recovery.t) |
| GK-0064 | A completed command whose result is rejected falls back atomically. | [tests/palette/kmeans/0064_command_auto_recovery.t](../../../tests/palette/kmeans/0064_command_auto_recovery.t) |
| GK-0065 | FORCE rejects an injected command failure and recovers. | [tests/palette/kmeans/0065_command_force_recovery.t](../../../tests/palette/kmeans/0065_command_force_recovery.t) |
| GK-0066 | Mixed CPU/GPU restarts preserve the seeded CPU result. | [tests/palette/kmeans/0066_mixed_restarts.t](../../../tests/palette/kmeans/0066_mixed_restarts.t) |
| GK-0067 | AUTO admission respects each M/K/I floor and the product boundary. | [tests/palette/kmeans/0067_workload_boundary.t](../../../tests/palette/kmeans/0067_workload_boundary.t) |
| GK-0068 | Snake GPU palette SSE stays within the calibrated 0.25% CPU-relative fixture budget for each selected configuration. | [tests/palette/kmeans/0068_snake_sse_k16_i4_s1.t](../../../tests/palette/kmeans/0068_snake_sse_k16_i4_s1.t), [tests/palette/kmeans/0092_snake_sse_k64_i12_s7.t](../../../tests/palette/kmeans/0092_snake_sse_k64_i12_s7.t), [tests/palette/kmeans/0094_snake_sse_k256_i24_s42.t](../../../tests/palette/kmeans/0094_snake_sse_k256_i24_s42.t) |
| GKR-0068 | Snake GPU center arrays repeat exactly for each fixed seed and iteration limit. | [tests/palette/kmeans/0091_snake_repeat_k16_i4_s1.t](../../../tests/palette/kmeans/0091_snake_repeat_k16_i4_s1.t), [tests/palette/kmeans/0093_snake_repeat_k64_i12_s7.t](../../../tests/palette/kmeans/0093_snake_repeat_k64_i12_s7.t), [tests/palette/kmeans/0095_snake_repeat_k256_i24_s42.t](../../../tests/palette/kmeans/0095_snake_repeat_k256_i24_s42.t) |
| GK-0069 | Gradient GPU palette SSE stays within the calibrated 0.25% CPU-relative fixture budget for each selected configuration. | [tests/palette/kmeans/0069_gradient_sse_k16_i4_s1.t](../../../tests/palette/kmeans/0069_gradient_sse_k16_i4_s1.t), [tests/palette/kmeans/0097_gradient_sse_k64_i12_s7.t](../../../tests/palette/kmeans/0097_gradient_sse_k64_i12_s7.t), [tests/palette/kmeans/0099_gradient_sse_k256_i24_s42.t](../../../tests/palette/kmeans/0099_gradient_sse_k256_i24_s42.t) |
| GKR-0069 | Gradient GPU center arrays repeat exactly for each fixed seed and iteration limit. | [tests/palette/kmeans/0096_gradient_repeat_k16_i4_s1.t](../../../tests/palette/kmeans/0096_gradient_repeat_k16_i4_s1.t), [tests/palette/kmeans/0098_gradient_repeat_k64_i12_s7.t](../../../tests/palette/kmeans/0098_gradient_repeat_k64_i12_s7.t), [tests/palette/kmeans/0100_gradient_repeat_k256_i24_s42.t](../../../tests/palette/kmeans/0100_gradient_repeat_k256_i24_s42.t) |
| GK-0070 | Rare-color GPU palette SSE stays within the calibrated 0.25% CPU-relative fixture budget for each selected configuration. | [tests/palette/kmeans/0070_rare_sse_k16_i4_s1.t](../../../tests/palette/kmeans/0070_rare_sse_k16_i4_s1.t), [tests/palette/kmeans/0102_rare_sse_k64_i12_s7.t](../../../tests/palette/kmeans/0102_rare_sse_k64_i12_s7.t), [tests/palette/kmeans/0104_rare_sse_k256_i24_s42.t](../../../tests/palette/kmeans/0104_rare_sse_k256_i24_s42.t) |
| GKR-0070 | Rare-color GPU center arrays repeat exactly for each fixed seed and iteration limit. | [tests/palette/kmeans/0101_rare_repeat_k16_i4_s1.t](../../../tests/palette/kmeans/0101_rare_repeat_k16_i4_s1.t), [tests/palette/kmeans/0103_rare_repeat_k64_i12_s7.t](../../../tests/palette/kmeans/0103_rare_repeat_k64_i12_s7.t), [tests/palette/kmeans/0105_rare_repeat_k256_i24_s42.t](../../../tests/palette/kmeans/0105_rare_repeat_k256_i24_s42.t) |
| GK-0071 | A small AUTO workload stays on CPU and reports its selection reason. | [tests/palette/kmeans/0071_auto_small.t](../../../tests/palette/kmeans/0071_auto_small.t) |
| GK-0072 | The backend environment setting accepts AUTO. | [tests/palette/kmeans/0072_auto_environment.t](../../../tests/palette/kmeans/0072_auto_environment.t) |
| GK-0073 | GPU policy OFF vetoes AUTO backend selection. | [tests/palette/kmeans/0073_auto_policy_off.t](../../../tests/palette/kmeans/0073_auto_policy_off.t) |
| GK-0074 | Explicit Elkan uses CPU under AUTO GPU policy. | [tests/palette/kmeans/0074_elkan_auto.t](../../../tests/palette/kmeans/0074_elkan_auto.t) |
| GK-0075 | Explicit Yinyang uses CPU under AUTO GPU policy. | [tests/palette/kmeans/0075_yinyang_auto.t](../../../tests/palette/kmeans/0075_yinyang_auto.t) |
| GK-0076 | FORCE rejects explicit Elkan with GPU backend selection. | [tests/palette/kmeans/0076_elkan_force.t](../../../tests/palette/kmeans/0076_elkan_force.t) |
| GK-0077 | FORCE rejects explicit Yinyang with AUTO backend selection. | [tests/palette/kmeans/0077_yinyang_force.t](../../../tests/palette/kmeans/0077_yinyang_force.t) |
| GK-0078 | FORCE plus AUTO backend rejects feedback without changing settings. | [tests/palette/kmeans/0078_auto_force_feedback.t](../../../tests/palette/kmeans/0078_auto_force_feedback.t) |
| GK-0079 | Named output retains native CPU application with GPU construction enabled. | [tests/palette/kmeans/0079_named_application.t](../../../tests/palette/kmeans/0079_named_application.t) |
| GK-0080 | Native float output retains CPU application with GPU construction enabled. | [tests/palette/kmeans/0080_float_application.t](../../../tests/palette/kmeans/0080_float_application.t) |
| GK-0081 | An explicit AUTO suboption overrides a GPU backend environment setting. | [tests/palette/kmeans/0081_auto_cli_precedence.t](../../../tests/palette/kmeans/0081_auto_cli_precedence.t) |
| GK-0082 | Unsafe native coordinates fall back to the identical CPU result. | [tests/palette/kmeans/0082_numeric_auto_fallback.t](../../../tests/palette/kmeans/0082_numeric_auto_fallback.t) |
| GK-0083 | FORCE rejects unsafe native coordinates without publishing a result. | [tests/palette/kmeans/0083_numeric_force_rejection.t](../../../tests/palette/kmeans/0083_numeric_force_rejection.t) |
| GK-0084 | Completed GPU palettes retain the controlled LATEST selection frontier. | [tests/palette/kmeans/0084_gpu_latest_frontier.t](../../../tests/palette/kmeans/0084_gpu_latest_frontier.t) |
| GK-0085 | Cancelled or obsolete GPU results cannot be published by the palette store. | [tests/palette/kmeans/0085_gpu_cancel_publication.t](../../../tests/palette/kmeans/0085_gpu_cancel_publication.t) |
| GK-0086 | Empty repair retains sample-index ties across tile boundaries and repeatedly selected zero residuals. | [tests/palette/kmeans/0086_repair_tile_ties.t](../../../tests/palette/kmeans/0086_repair_tile_ties.t) |
| GK-0087 | Reduction handles 1024 centers, sparse tiles, signed samples and a partial final tile. | [tests/palette/kmeans/0087_wide_tile_reduction.t](../../../tests/palette/kmeans/0087_wide_tile_reduction.t) |
| GK-0088 | A cold AUTO request stays on CPU under FORCE and leaves pipelines uninitialized. | [tests/palette/kmeans/0088_auto_cold.t](../../../tests/palette/kmeans/0088_auto_cold.t) |
| GK-0089 | An eligible AUTO request executes the selected GPU backend after explicit pipeline initialization. | [tests/palette/kmeans/0089_auto_warm.t](../../../tests/palette/kmeans/0089_auto_warm.t) |
| GK-0090 | A high iteration cap with a low minimum stays on CPU, including with warm pipelines. | [tests/palette/kmeans/0090_auto_minimum.t](../../../tests/palette/kmeans/0090_auto_minimum.t) |

### Quality regression tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GK-13 | An observed GPU execution meets MS-SSIM 0.98 at 256 colors. | [tests/palette/kmeans/0013_gpu_quality.t](../../../tests/palette/kmeans/0013_gpu_quality.t) |

### Coverage boundary

GPU-specific tests skip only when a GPU device is unavailable, and fail when an available device cannot execute the implementation. Platform fallback is exercised separately by a build or machine without a GPU backend. The fixtures establish their stated contracts, not bitwise agreement with CPU arithmetic on arbitrary images or universal GPU speedups. Initialization, buffer-allocation and command failures are injected separately, with AUTO seed-preserving fallback, FORCE failure, recovery and mixed CPU/GPU restarts. Device-dependent cases skip when the selected GPU is unavailable. The measured SSE and performance scope is the Mac15,10 calibration above. The warm AUTO dispatch test needs an actual GPU device; cold selection, minimum-work selection and pure workload boundaries also run without one. Playback sequence equality under changed completion timing remains unverified.
