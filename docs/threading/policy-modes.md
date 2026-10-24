# Threading policy modes

This explanation is for users and contributors choosing how libsixel spends a worker budget. It compares `lazy`, `balanced` and `greedy`, including their output and topology implications. The [threading-policy reference](../cli/threading-policy.md) owns syntax, keys, defaults and precedence; [encoder threading](encoder.md) owns allocator details and measurements.

## Independent work and data fan-out

A budget permits two different forms of concurrency: independent stages can overlap, and one stage can partition its data among workers. `lazy` favors independent work; `balanced` also partitions useful-sized work; `greedy` removes selected minimum-size gates. The mode does not itself raise the library budget.

The table follows a frame from preparation to output. Every reservation still fits the resolved budget and applicable stage limit.

| Stage | `lazy` | `balanced` | `greedy` |
| --- | --- | --- | --- |
| Palette branch and main preprocessing | Eligible asynchronous palette and main branches each use one slot | Eligible branches split preparation slots between palette and main work | Same split |
| Colorspace, scaling and sample extraction | Serial within each branch | Eligible work fans out above its size threshold | Eligible work fans out with minimum-size thresholds set to zero |
| Palette application and SIXEL encoding | One default dither producer can overlap independent encode jobs | Automatic small-budget staging or larger-budget row pipeline | Same allocation rules |
| FHEDT lookup-table construction | Serial | Uses eligible parallel build work within the budget | Same budgeted build work |
| SIXEL parsing | Serial even with a `decode` limit | Uses the resolved parsing budget | Uses the resolved parsing budget |
| Loader and next-frame preparation | Can overlap current emission | Can overlap current emission | Can overlap current emission |

Palette overlap requires independent work, such as an eligible adaptive sample whose palette can be built while the full image is transformed. It is not unconditional frame-level parallelism. In balanced and greedy modes the initial palette reservation is `floor(N/2)` and the main reservation is the remainder. A main colorspace conversion can reclaim completed palette slots after joining the palette worker. [Encoder threading](encoder.md#float-colorspace-transactions) explains that lifetime boundary.

For eligible spatial or positional dither at budgets two through four, balanced and greedy automatically finish parallel application before giving encoding its full reservation. Larger budgets normally overlap application and encoding. An explicit `dither` or `encode` limit selects a fitted allocation instead of automatic staging. [Dither/encode allocation](encoder.md#ditherencode-budget-policy) owns the formulas, geometry and exceptional paths.

## Output depends on dither history

Of the threading policy's scheduling choices, the dither band split can change output. Error diffusion carries residuals from earlier rows; independent bands start with separate histories. Warm-up rows reduce seams but do not reconstruct the complete serial history. The [work-band explanation](overview.md#work-band-seams-and-image-quality) describes this boundary.

With default dither geometry, lazy output equals a one-thread run using the same image, palette and other options. Its single dither producer preserves the history while independent six-row encoding jobs publish in order. An explicit `dither` limit or diffusion geometry override can split that history even in lazy mode.

Frame controls (`load`, `load_depth`, `prepare`, and implemented `palette=frame`) and `prepare_workers` never change output. They alter when independent work runs. Encode worker count changes neither indexed pixels nor their publication order. SIMD and arithmetic representation are separate runtime choices and can have their own rounding behavior.

## Performance CPUs and size thresholds

The topology probe returns performance logical CPUs when a hybrid layout can be identified. The following detection rules explain why balanced may use fewer slots than greedy for the same explicit budget.

| Platform | Detection | Fallback |
| --- | --- | --- |
| macOS | Read `hw.nperflevels`; when it exceeds one, read `hw.perflevel0.logicalcpu` | Logical CPU count if homogeneous, missing or invalid |
| Linux | Count CPU IDs and ranges in `/sys/devices/cpu_core/cpus` when it identifies a smaller core set | Logical CPU count if absent or invalid |
| Other platforms | No hybrid topology probe | Logical CPU count |

Balanced caps an explicit numeric or `all` budget to this result. Homogeneous systems and unknown layouts, including most VMs, have no additional hybrid cap. All modes use the same conservative library default from the [reference](../cli/threading-policy.md#options). Detection is a count-based limit, not a promise that workers are physically placed on a particular core type; `pin` is best effort and platform dependent.

Greedy resolves `colorspace_min`, `sample_min` and `scale_min_bytes` to zero even if `-j` explicitly supplies larger thresholds. It keeps format and algorithm eligibility, memory checks and available worker limits. Requesting more concurrency can cost more than it saves on small images; the mode makes no throughput guarantee.

## Frame overlap and ordered publication

Read-ahead and preparation are independent scheduling decisions. This diagram shows the permitted overlap; only the emission lane publishes SIXEL and temporal history.

```text
loader:       load n       load n+1       load n+2
preparation:  prepare n    prepare n+1    prepare n+2
emission:                 emit n         emit n+1       emit n+2
                          |< overlap >|
```

Preparation of the next frame can run during current emission, while output still advances one frame at a time. Delta history is committed in order. [Animation threading](animation.md) explains the corrective re-preparation needed when the next frame's predicted alpha-key reservation disagrees with committed history.

## In libsixel

The mode is process configuration shared by library consumers. The converters apply their application defaults before resolving it. Thread pools remain local to operations; a configured budget is not a global operating-system thread quota. [Threading overview](overview.md) distinguishes planned slots from control threads and concurrent clients.

## Implementation map

- [src/threading.c](../../src/threading.c) resolves topology, mode, fan-out and runtime threshold overrides.
- [src/planner.c](../../src/planner.c) allocates palette/main and dither/encode work.
- [src/encoder.c](../../src/encoder.c) owns frame read-ahead, prepare/emit overlap and ordered temporal-state commit.
- [src/decoder-parallel.c](../../src/decoder-parallel.c) resolves parsing limits and safe parallel spans.
- [Byte FHEDT](../../src/lookup-fhedt-8bit.c) and [float FHEDT](../../src/lookup-fhedt-float32.c) consume the shared fan-out resolver, retaining serial execution when thread-local storage is unavailable.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| PM-01 | Mode-only defaults, topology caps, lazy fan-out/parsing, greedy threshold overrides and lazy allocator reservations are checked directly. | [tests/processing/threadpool/0003_thread_policy.t](../../tests/processing/threadpool/0003_thread_policy.t) |
| PM-02 | Lazy still-image output matches one-thread output while encoding retains multiple slots. | [tests/planner/pipeline/0023_pipeline_lazy_still_bits.t](../../tests/planner/pipeline/0023_pipeline_lazy_still_bits.t) |
| PM-03 | Lazy animated output matches one-thread output. | [tests/planner/pipeline/0024_pipeline_lazy_animation_bits.t](../../tests/planner/pipeline/0024_pipeline_lazy_animation_bits.t) |
| PM-04 | Lazy delta output preserves one-thread temporal results. | [tests/planner/pipeline/0025_pipeline_lazy_delta_bits.t](../../tests/planner/pipeline/0025_pipeline_lazy_delta_bits.t) |
| PM-05 | Prepare-ahead and serial preparation produce identical full and delta output. | [tests/planner/pipeline/0019_pipeline_prepare_ahead_bits.t](../../tests/planner/pipeline/0019_pipeline_prepare_ahead_bits.t) |
| PM-06 | Preparation worker limits preserve output through CLI and environment routes. | [tests/cli/options/regression/0183_threading_prepare_workers_image_regression.t](../../tests/cli/options/regression/0183_threading_prepare_workers_image_regression.t) |
| PM-07 | Load concurrency routes preserve output. | [tests/cli/options/regression/0179_threading_load_image_regression.t](../../tests/cli/options/regression/0179_threading_load_image_regression.t) |
| PM-08 | Playback admits its configured capacity independently of loader read-ahead depth. | [tests/planner/pipeline/0021_playback_admission.t](../../tests/planner/pipeline/0021_playback_admission.t) |

### Coverage boundary

The [encoder coverage](encoder.md#test-coverage) owns detailed allocation, palette/preprocessing transactions and sampling evidence. PM-01 tests the shared fan-out resolver consumed by both FHEDT backends; it does not instrument every FHEDT worker dispatch. The [lookup suite](../../tests/processing/lookup/) checks FHEDT and affinity consumers. Host topology and thread availability limit observed concurrency; the tests do not promise worker participation on every run, affinity on unsupported systems or a universal speedup. Output equivalence is tested on specific still, full-frame and delta fixtures, not all possible images.
