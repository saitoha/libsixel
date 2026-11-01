# Threading policy modes

This explanation is for users and contributors choosing how libsixel spends a worker budget. It compares `lazy`, `balanced` and `greedy`, including their output and topology implications. The [threading-policy reference](../cli/threading-policy.md) owns syntax, keys, defaults and precedence; [encoder threading](encoder.md) owns allocator details and measurements.

## Independent work and data fan-out

A budget permits two different forms of concurrency: independent stages can overlap, and one stage can partition its data among workers. `lazy` favors independent work; `balanced` also partitions useful-sized work; `greedy` removes selected minimum-size gates. The mode does not itself raise the library budget.

The table follows a frame from preparation to output with `palette=frame`. Every reservation still fits the resolved budget and applicable stage limit.

| Stage | `lazy` | `balanced` | `greedy` |
| --- | --- | --- | --- |
| Palette branch and main preprocessing | Eligible asynchronous palette and main branches each use one slot | Eligible branches split preparation slots between palette and main work | Same split |
| Colorspace, scaling and sample extraction | Serial within each branch | Eligible work fans out above its size threshold | Eligible work fans out with minimum-size thresholds set to zero |
| Palette application and SIXEL encoding | One default dither producer can overlap independent encode jobs | Automatic small-budget staging or larger-budget row pipeline | Same allocation rules |
| FHEDT lookup-table construction | Serial | Uses eligible parallel build work within the budget | Same budgeted build work |
| SIXEL parsing | Serial | Uses the resolved parsing budget | Uses the resolved parsing budget |
| Loader and next-frame preparation | Can overlap current emission | Can overlap current emission | Can overlap current emission |

Palette overlap requires independent work, such as an eligible adaptive sample whose palette can be built while the full image is transformed. It is not unconditional frame-level parallelism. In balanced and greedy modes the initial palette reservation is `floor(N/2)` and the main reservation is the remainder. A main colorspace conversion can reclaim completed palette slots after joining the palette worker. [Encoder threading](encoder.md#float-colorspace-transactions) explains that lifetime boundary.

For eligible spatial or positional dither at budgets two through four, balanced and greedy automatically finish parallel application before giving encoding its full reservation. Larger budgets normally overlap application and encoding. An explicit `dither` or `encode` limit selects a fitted allocation instead of automatic staging. [Dither/encode allocation](encoder.md#ditherencode-budget-policy) owns the formulas, geometry and exceptional paths.

For `palette=latest`, eligible CPU animation and playback can overlap an owned palette build with application of a completed palette, including full-frame sampling after preprocessing. Application and encoding retain the entire budget N and the same stage limits and allocator as frame mode. Builders borrow idle work slots; lazy allows this independent work without increasing its data-parallel fan-out. `load=serial`, `prepare=serial` and `prepare_workers=1` do not by themselves disable latest selection. A serial fallback uses frame behavior; the [policy reference](../cli/threading-policy.md#contract) owns eligibility and selection.

`palette=auto` uses these frame/latest scheduling behaviors with adjacent completion-interval pairs in five ABBA blocks and increasing reevaluation intervals only when a choice is decisively confirmed. The [policy reference](../cli/threading-policy.md#contract) owns the 20-frame balanced comparison, palette-wait trigger, median paired gain, 3% plus twice-MAD margin and back-off/reset rule. Interior probe transitions pass a frozen mode to prepare-ahead; only the final decision holds preparation. The default stays frame. Auto can temporarily probe the slower behavior and retains completed palettes across frame periods, with compatibility checks on reuse; neither probing nor adaptation promises a speedup.

## Output depends on dither history

Of the threading policy's scheduling choices, the dither band split can change output. Error diffusion carries residuals from earlier rows; independent bands start with separate histories. Warm-up rows reduce seams but do not reconstruct the complete serial history. The [work-band explanation](overview.md#work-band-seams-and-image-quality) describes this boundary.

With `palette=frame` and default dither geometry, lazy output equals a one-thread run using the same image, palette and other options. Its single dither producer preserves the history while independent six-row encoding jobs publish in order. An explicit `dither` limit or diffusion geometry override can split that history even in lazy mode.

With `palette=frame`, frame controls (`load`, `load_depth` and `prepare`) and `prepare_workers` alter when independent work runs without changing output. With `palette=latest` or auto executing latest, scheduling can change which completed palette is selected; [the policy reference](../cli/threading-policy.md#contract) owns selection and fallback. Once a palette and dither geometry are fixed, encode worker count changes neither indexed pixels nor their publication order. SIMD and arithmetic representation are separate runtime choices and can have their own rounding behavior.

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

Preparation of the next frame can run during current emission, while output still advances one frame at a time. Both effective palette behaviors give emission N and preserve ordinary preparation's independent cap. Latest builders and their full-frame input copies borrow execution slots while emission jobs drain or its caller waits for child jobs or output. Renewed emission demand stops further background admissions; an admitted background job returns its loan at its next boundary. Emission never waits for that return, so a job already in progress may briefly overlap renewed foreground demand. This policy does not promise a speedup. Delta history is committed in order. [Animation threading](animation.md) explains the corrective re-preparation needed when the next frame's predicted alpha-key reservation disagrees with committed history.

## In libsixel

Each encoder and decoder owns its mode. A work entry resolves a policy snapshot and helper threads inherit it; concurrent objects retain independent policies. The converters apply their type-specific application defaults before resolution. Thread pools remain local to operations; a configured budget is not a global operating-system thread quota. [Threading overview](overview.md) distinguishes planned slots from control threads and concurrent clients.

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
| PM-01 | Mode-only defaults, topology caps, lazy fan-out/parsing, greedy threshold overrides and lazy allocator reservations are checked directly. | [tests/platform/threadpool/0003_thread_policy.t](../../tests/platform/threadpool/0003_thread_policy.t) |
| PM-02 | Lazy still-image output matches one-thread output while encoding retains multiple slots. | [tests/encoder/planning/0023_pipeline_lazy_still_bits.t](../../tests/encoder/planning/0023_pipeline_lazy_still_bits.t) |
| PM-03 | Lazy animated output matches one-thread output. | [tests/encoder/planning/0024_pipeline_lazy_animation_bits.t](../../tests/encoder/planning/0024_pipeline_lazy_animation_bits.t) |
| PM-04 | Lazy delta output preserves one-thread temporal results. | [tests/encoder/planning/0025_pipeline_lazy_delta_bits.t](../../tests/encoder/planning/0025_pipeline_lazy_delta_bits.t) |
| PM-05 | Prepare-ahead and serial preparation produce identical full and delta output. | [tests/encoder/planning/0019_pipeline_prepare_ahead_bits.t](../../tests/encoder/planning/0019_pipeline_prepare_ahead_bits.t) |
| PM-06 | Preparation worker limits preserve output through CLI and environment routes. | [tests/cli/option-syntax/0088_threading_prepare_workers_image_regression.t](../../tests/cli/option-syntax/0088_threading_prepare_workers_image_regression.t) |
| PM-07 | Load concurrency routes preserve output. | [tests/cli/option-syntax/0085_threading_load_image_regression.t](../../tests/cli/option-syntax/0085_threading_load_image_regression.t) |
| PM-08 | Playback admits its configured capacity independently of loader read-ahead depth. | [tests/encoder/planning/0021_playback_admission.t](../../tests/encoder/planning/0021_playback_admission.t) |
| PM-09 | Object settings and encoder/decoder defaults stay independent. | [tests/platform/threadpool/0005_object_policy_isolation.t](../../tests/platform/threadpool/0005_object_policy_isolation.t) |
| PM-10 | Concurrent encode and playback calls preserve their own budgets and dither plans. | [tests/platform/threadpool/0006_concurrent_object_policies.t](../../tests/platform/threadpool/0006_concurrent_object_policies.t) |

| PM-11 | Auto executes both scheduling behaviors at frame boundaries and resets to frame on playback reattachment. | [tests/encoder/planning/0086_palette_auto_playback.t](../../tests/encoder/planning/0086_palette_auto_playback.t) |

### Coverage boundary

The [encoder coverage](encoder.md#test-coverage) owns detailed allocation, palette/preprocessing transactions and sampling evidence. PM-01 tests the shared fan-out resolver consumed by both FHEDT backends; it does not instrument every FHEDT worker dispatch. The [lookup suite](../../tests/lookup) checks FHEDT and affinity consumers. Host topology and thread availability limit observed concurrency; the tests do not promise worker participation on every run, affinity on unsupported systems or a universal speedup. Output equivalence is tested on specific still, full-frame and delta fixtures, not all possible images.
