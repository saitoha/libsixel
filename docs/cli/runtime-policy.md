# Runtime execution policy

`-j` / `--runtime-policy` configures the SIMD ceiling and selected scheduling/resize controls. It is accepted by both converters, with different suboption consumers. It does not select a quantizer or set the worker count. Use `-=` / `--threads` for the worker budget and `-G` for GPU policy.

## SIMD ceiling

```sh
img2sixel -j scalar -o scalar.six image.png
img2sixel -j auto -o native.six image.png
sixel2png -j scalar -i native.six -o decoded.png
```

Accepted base values are `auto`, `none`, `scalar`, `sse2`, `avx`, and `neon`; `none` and `scalar` request the scalar ceiling. `auto` permits the compiled implementation to use detected native capabilities. A requested ceiling is combined with the build, runtime CPU capabilities, pixel representation, and the implementations available in each stage. It cannot add an instruction set to a build or guarantee vectorization of every operation. The enum ordering is internal; do not interpret an architecture name as a portable request to emulate another CPU.

`SIXEL_SIMD_LEVEL` supplies the environment fallback. Explicit `-j` policy takes precedence. Runtime policy is process-level state, and several consumers cache their resolved settings on first use. Configure it before processing starts; separate processes are the reliable way to compare policies. This is not an API for independently retuning concurrent encoder instances.

## Suboptions and units

| Suboption | Consumer | Meaning | Environment fallback |
| --- | --- | --- | --- |
| `colorspace_min=PIXELS` (`C`) | `img2sixel` | Unsigned pixel-count floor for eligible colorspace parallel work. It does not create workers when the budget or build has none. | `SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS` |
| `parallel_factor=ROWS` (`F`) | `img2sixel` | Positive row span per resize thread-pool job, clamped to the available rows. This is not a multiplier on the thread count. | `SIXEL_PARALLEL_FACTOR` |
| `resize_precision=preserve\|linear\|float` (`R`) | `img2sixel` | Selects the resize representation policy described in [crop and resize](../functionality/crop-resize.md). | `SIXEL_PLANNER_RESIZE_PRECISION_MODE` |
| `scale_min_bytes=BYTES` (`B`) | `img2sixel` | Unsigned byte threshold for eligible parallel resize work. Zero preserves the eager threshold behavior; other eligibility checks still apply. | `SIXEL_SCALE_PARALLEL_MIN_BYTES` |
| `parallel_skew=VALUE` (`K`) | `sixel2png` | Decoder parallel partition adjustment, in the range -20..20. It changes scheduling balance rather than decoded image dimensions. | `SIXEL_PARALLEL_SKEW` |

The registry rejects suboptions outside the converter's consumer scope. In particular, the encoder's resize controls are not portable additions to `sixel2png -j`, and the decoder's `parallel_skew` is not an `img2sixel` setting. Explicit suboption fields override corresponding environment values. Repeated options use the shared typed-policy machinery; omitted fields are not a request to clear all existing process overrides.

Without a `parallel_factor` override, resize derives the job span from rows and threads. The minimum-work controls only determine whether a stage can benefit from parallel dispatch; they do not override memory limits or worker availability. See [threading](../threading/README.md) for the distinct encoder, decoder, and stage-local budgets.

```sh
# Scalar arithmetic with an explicit resize representation.
img2sixel -j scalar:resize_precision=linear -w 50% image.png

# Native SIMD, but avoid scheduling tiny resize tasks.
img2sixel -j auto:scale_min_bytes=1048576:parallel_factor=32 \
    --threads=4 -w 50% image.png
```

## Precision, quality, and performance

The SIMD ceiling and resize precision are independent fields in the same option. Scalar execution does not imply byte precision; a scalar linear-light resize still uses floating-point work. Conversely, selecting `auto` does not require float32 palette application. The effective path also depends on `-.`, `-W`, source representation, and loader CMS.

Vector accumulation can round differently from scalar accumulation. Changes in task boundaries can also expose stateful algorithm differences elsewhere in the pipeline. Compare decoded pixels or suitable quality metrics for the particular path; do not assume every policy must produce identical SIXEL bytes. The [resampling guide](../functionality/resampling.md) and [crop/resize measurements](../functionality/crop-resize.md) separate arithmetic, representation, and scheduling effects.

Use `-v` together with `-x human:trace_topic=runtime_contract` to inspect requested, native, and effective choices. For timing, first confirm the path with diagnostics, then measure without verbose tracing or timeline logging. Small inputs can be slower with more parallel work because setup and queueing costs dominate. No one threshold is best for all image sizes and machines.

## Implementation

The typed controls and consumer scopes live in [`options-registry.c`](../../src/options-registry.c). [`scale.c`](../../src/scale.c) and [`colorspace.c`](../../src/colorspace.c) own their respective work thresholds. Decoder scheduling is described in [decoder threading](../threading/decoder.md).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| RT-01 | The runtime SIMD environment reaches its consumer. | [tests/processing/runtime/0001_simd_level_environment_consumer.t](../../tests/processing/runtime/0001_simd_level_environment_consumer.t) |
| RT-02 | The colorspace minimum-work environment reaches its consumer. | [tests/processing/runtime/0002_colorspace_parallel_min_environment_consumer.t](../../tests/processing/runtime/0002_colorspace_parallel_min_environment_consumer.t) |
| RT-03 | The resize-precision environment reaches its consumer. | [tests/processing/runtime/0005_resize_precision_environment_consumer.t](../../tests/processing/runtime/0005_resize_precision_environment_consumer.t) |

### Coverage boundary

The [runtime consumer suite](../../tests/processing/runtime/) and [CLI regression suite](../../tests/cli/options/regression/) test selected resolutions and output effects. They do not establish that every SIMD implementation is available on the test host or that all settings improve performance. Resize numerical and quality coverage remains in the [resampling inventory](../testing/resampling-coverage.md).
