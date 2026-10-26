# Threading policy

This reference is for CLI users and library integrators configuring `-=` / `--threading-policy`. It owns syntax, defaults, precedence and consumer scopes. [Policy modes](../threading/policy-modes.md) explains stage scheduling and output effects; [runtime execution policy](runtime-policy.md) owns SIMD and minimum-work thresholds.

## Options

```sh
img2sixel -=balanced:global_budget=all:prepare_workers=2 image.png
img2sixel -=lazy:Ball:Q4 image.gif
sixel2png --threading-policy=greedy:B3 -i image.six -o image.png
```

The syntax is `-= MODE[:KEY=VALUE...]` or `--threading-policy=MODE[:KEY=VALUE...]`, with a required base of `lazy`, `balanced` or `greedy`. A compact key is one uppercase letter immediately followed by its value, such as `B4` or `Ball`. `SIXEL_ENCODER_THREADING_POLICY` and `SIXEL_DECODER_THREADING_POLICY` accept a base only, without suboptions; its default is `balanced`.

The table gives every key, its compact form and its per-key environment source. Counts range from 1 through `INT_MAX` unless a narrower range is shown. “Automatic” means allocation from the resolved budget and mode, rather than a fixed worker count.

| Key | Short | Accepted values | Default | Environment | Consumer |
| --- | --- | --- | --- | --- | --- |
| `global_budget` | `B` | Positive integer or case-insensitive `all` | Library: `min(4, performance logical CPUs)`; converters: `all` | `SIXEL_ENCODER_THREADING_GLOBAL_BUDGET` / `SIXEL_DECODER_THREADING_GLOBAL_BUDGET` | Both |
| `load` | `L` | `serial`, `ahead` | `ahead` | `SIXEL_ENCODER_THREADING_LOAD` | Encoder |
| `load_depth` | `Q` | 1..16 | 4 | `SIXEL_ENCODER_THREADING_LOAD_DEPTH` | Encoder |
| `prepare` | `P` | `serial`, `ahead` | `ahead` | `SIXEL_ENCODER_THREADING_PREPARE` | Encoder |
| `palette` | `C` | `frame`, `latest` | `frame` | `SIXEL_ENCODER_THREADING_PALETTE` | Encoder |
| `dither` | `D` | Positive integer | Automatic | `SIXEL_ENCODER_THREADING_DITHER` | Encoder |
| `encode` | `E` | Positive integer | Automatic | `SIXEL_ENCODER_THREADING_ENCODE` | Encoder |
| `prepare_workers` | `W` | Positive integer | Resolved budget | `SIXEL_ENCODER_THREADING_PREPARE_WORKERS` | Encoder |
| `decode` | `S` | Positive integer | Encoder budget | `SIXEL_ENCODER_THREADING_DECODE` | Encoder SIXEL input |
| `pin` | `I` | `0`, `1` | Dither workers: 1; FHEDT workers: 0 | `SIXEL_ENCODER_THREADING_PIN` | Encoder |

`all` selects the logical CPU count. `balanced` then caps it, like an explicit numeric budget, to detected performance logical CPUs. The library's conservative default applies in every mode: selecting a mode alone does not enlarge it. In a build without threads, the effective count is one.

Each converter sets its own empty or unset `SIXEL_ENCODER_THREADING_*` or `SIXEL_DECODER_THREADING_*` budget to `all` and mode to `balanced`, after processing `--env`. Nonempty user values survive these defaults. Direct library callers receive neither converter default.

## Contract

The budget counts planned work slots, including participating callers, rather than every operating-system thread in the process. [Threading overview](../threading/overview.md#worker-budget-is-not-a-process-thread-limit) explains control threads and independent pools.

Stage limits shrink to the available budget; requesting more slots is never an error. `dither` and `encode` limit palette application and SIXEL byte generation. `prepare_workers` limits scaling, colorspace conversion, sampling and palette construction. Ordinary next-frame preparation keeps its independent cap in both palette modes. Latest emission receives the entire encoder budget N with the same dither/encode allocator as frame mode. Its background palette builders and latest-only full-frame copies use only idle execution slots, capped by `prepare_workers`; they wait when none are available. A returning emission job takes priority without waiting for a borrower, which returns its loan at the next job boundary. These CPU loans do not reduce the emission allocation. The encoder’s `decode` key caps SIXEL parsing inside its builtin loader; `lazy` always parses serially. A decoder object uses its whole budget for SIXEL parsing and has no `decode` key. Without an explicit mode or budget, library decoder parsing uses one thread.

`load` controls loader read-ahead and `load_depth` bounds its queued frames. `prepare=ahead` permits preparation of frame n+1 during emission of frame n. Publication remains ordered, one frame at a time. With `palette=frame`, these keys and `prepare_workers` preserve output. [Animation threading](../threading/animation.md) describes ownership and the separate public playback queue capacity.

Numeric `global_budget=0`, negative values and words other than `all` fail CLI parsing with a diagnostic naming the positive-integer-or-`all` grammar. Invalid per-key environment values are ignored, leaving the next applicable default. `palette=latest` selects `k = max { s | s <= n, palette(s) published before selection }` for frame n, within the current session epoch. This includes n itself if its palette is already complete and excludes every future frame. Selection occurs once after the display delay, before LUT/gradient setup, and remains fixed throughout application. Ordinary CPU animation and playback sessions build every frame’s own palette and can apply an earlier completed palette concurrently. The first frame waits for its own palette. Results depend on completion timing, so repeated runs need not produce identical palettes or output bytes; there is no scene-change detector.

Loader loop boundaries retain the session sequence; new inputs and playback detach discard its palettes. A fixed/indexed/high-color bypass starts a new epoch. A one-worker budget, a build without threads, unavailable asynchronous execution, static input, external or builtin palettes, preserved indexed palettes, high-color output, GPU-enabled paths, DRCS, presentation/attached repair and generated macros use frame behavior. Serial fallbacks produce the same bytes as `palette=frame` with the same options. Indexed input that requires requantization is eligible for generated-palette processing. Delta and source transparency reuse only palettes with matching reservation and color profiles; a mismatch waits for the current frame’s own palette, and a stale prepare-ahead prediction is joined and rebuilt from its pristine source. Native color output and palette export use the selected palette. History retains the colors actually emitted, and each frame binds its own mask, origin and threshold limit. Verbose output reports the requested and effective palette modes.

Playback results report the selected source in `palette_sequence`, bounded by the submitted `sequence`. A frame without a selected palette reports zero; a successful frame-mode or bypass frame reports its own sequence. Latest retains a selected sequence if a later operation fails. An own-build failure fails that frame and drain even after output; already written bytes cannot be rolled back. Completion callbacks remain in submission order and drain joins outstanding builds.

`img2sixel` accepts encoder keys; `sixel2png` accepts only `global_budget`. A key outside the converter's scope is rejected. An invalid policy argument leaves the stored policy unchanged.

## Precedence and interactions

Each encoder and decoder owns its policy. `setopt('=')` changes only that object. Budget precedence is the object's explicit `global_budget`, then the matching low-level API default, then its type-specific environment, then library defaults. The encoder and decoder sides never read one another's environment or API default.

`sixel_set_encoder_threads(n)` and `sixel_set_decoder_threads(n)` set independent, internally locked process defaults for budgets only. Call them before processing starts. In the C API, values `n <= 0` select `all`. The Python, Perl, Ruby and PHP binding setters accept positive integers or case-insensitive `all`, with surrounding whitespace stripped from strings. Ruby also accepts `:all`. The bindings reject `auto`, zero and negative budgets with a positive-integer-or-`all` error; only `all` is translated to the C zero sentinel. They supply objects without an explicit budget and object-less APIs such as `sixel_encode()`, `sixel_dither_*`, `sixel_decode_raw()` and `sixel_decode_direct()`. An explicit object budget wins regardless of setter call order.

The base resolves independently: an explicit object base wins over the matching `SIXEL_ENCODER_THREADING_POLICY` or `SIXEL_DECODER_THREADING_POLICY`, then `balanced`. Stage keys resolve from explicit object fields, encoder environment, then stage defaults. Playback freezes this resolution when attached: an explicit config overrides the palette key, including its default `SIXEL_PLAYBACK_PALETTE_PER_FRAME=0`; a NULL config inherits the encoder policy. The override does not modify the encoder and does not survive detach. RGB submissions are session frames even when their input `multiframe` flag is zero. Repeated policy arguments replace the base and non-budget keys; an omitted budget preserves the object's earlier explicit budget.

Each work entry freezes the resolved mode, budget, stage limits, pin and frame keys in a thread-local scope. Helper threads inherit that snapshot. Nested budget scopes restrict the current operation; a nested public call on another object resolves that object's policy independently. The builtin SIXEL loader inherits its encoder's scope and applies the encoder's `decode` cap. Object-less APIs inside an active scope inherit it; outside one they resolve their own side's defaults. In-flight work is unaffected by later default changes.

The `greedy` mode makes `-j` minimum-size thresholds ineffective for eligible colorspace, sampling and scaling work. It does not change the SIMD ceiling or bypass format, memory, thread-creation or algorithm eligibility checks. See [mode behavior](../threading/policy-modes.md).

## Implementation map

- [Registry](../../src/options-registry.c) owns keys, scope, ranges and the named unsigned `all` sentinel. [Typed parsing](../../src/options.c) applies values atomically; `LSXSUB1` records `value=0` for `all`, independent of host topology.
- [Thread resolution](../../src/threading.c) owns precedence and topology; [decoder resolution](../../src/decoder-parallel.c) preserves the serial library parsing default.
- [Planner](../../src/planner.c) fits stage reservations to the resolved budget. `img2sixel -v` reports `threading: mode=... budget=...` and the frame settings.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| TP-01 | Library default and direct decoder default are resolved separately. | [tests/processing/threadpool/0002_thread_default_counts.t](../../tests/processing/threadpool/0002_thread_default_counts.t) |
| TP-02 | Modes preserve the library default; CLI/API/environment precedence, `all`, numeric rejection, atomic failure, palette acceptance and frozen-scope precedence and decoder limits are enforced. | [tests/processing/threadpool/0003_thread_policy.t](../../tests/processing/threadpool/0003_thread_policy.t) |
| TP-03 | Short, long and environment budget routes use the same `greedy` mode when comparing encoder output; `all` routes store the stable sentinel. | [tests/cli/options/regression/0185_threading_global_budget_image_regression.t](../../tests/cli/options/regression/0185_threading_global_budget_image_regression.t) |
| TP-04 | Converter defaults resolve to the available performance CPU budget, with user environment and CLI overrides. | [tests/processing/threadpool/0004_converter_default_budget.t](../../tests/processing/threadpool/0004_converter_default_budget.t) |
| TP-05 | Load depth is routed through the registry with range fallback and output equivalence. | [tests/cli/options/regression/0180_threading_load_depth_image_regression.t](../../tests/cli/options/regression/0180_threading_load_depth_image_regression.t) |
| TP-06 | Decoder budget routes preserve output with type-specific environment. | [tests/cli/options/regression/0186_decoder_threading_budget_image_regression.t](../../tests/cli/options/regression/0186_decoder_threading_budget_image_regression.t) |
| TP-07 | Objects, low-level defaults and nested scopes stay isolated. | [tests/processing/threadpool/0005_object_policy_isolation.t](../../tests/processing/threadpool/0005_object_policy_isolation.t) |
| TP-08 | Concurrent direct encoding and playback retain separate planner budgets and modes. | [tests/processing/threadpool/0006_concurrent_object_policies.t](../../tests/processing/threadpool/0006_concurrent_object_policies.t) |
| TP-09 | Both Python budget setters accept positive counts and trimmed, case-insensitive `all` and reject non-positive budgets. | [tests/bindings/python/0035_python_api_thread_budget_setters.py](../../tests/bindings/python/0035_python_api_thread_budget_setters.py) |
| TP-10 | Both Perl budget setters validate positive counts and trimmed, case-insensitive `all` and reject non-positive budgets before FFI conversion. | [tests/bindings/perl/0035_perl_api_thread_budget_setters.pl](../../tests/bindings/perl/0035_perl_api_thread_budget_setters.pl), [tests/bindings/perl/0055_perl_api_set_encoder_threads_invalid_numbers.pl](../../tests/bindings/perl/0055_perl_api_set_encoder_threads_invalid_numbers.pl) |
| TP-11 | Both Ruby budget setters accept positive counts, trimmed, case-insensitive `all` and `:all` and reject non-positive budgets. | [tests/bindings/ruby/0035_ruby_api_thread_budget_setters.rb](../../tests/bindings/ruby/0035_ruby_api_thread_budget_setters.rb) |
| TP-12 | Both PHP budget setters accept positive counts and trimmed, case-insensitive `all` and reject non-positive budgets. | [tests/bindings/php/0035_php_api_thread_budget_setters.php](../../tests/bindings/php/0035_php_api_thread_budget_setters.php) |
| TP-13 | Compact, long and environment palette routes retain latest, explicit frame overrides it, and effective frame output remains byte-identical. | [tests/cli/options/regression/0182_threading_palette_image_regression.t](../../tests/cli/options/regression/0182_threading_palette_image_regression.t) |
| TP-14 | Playback config overrides and NULL inheritance reach execution, remain frozen and leave the encoder unchanged; latest reports bounded palette provenance and serial fallback matches frame bytes. | [tests/planner/pipeline/0026_playback_palette_policy.t](../../tests/planner/pipeline/0026_playback_palette_policy.t) |
| TP-15 | An unfinished own build does not block application using a published palette; independent application produces the same bytes. | [tests/planner/pipeline/0047_latest_apply_before_own.t](../../tests/planner/pipeline/0047_latest_apply_before_own.t) |
| TP-16 | An own palette completed before the transaction is selected. | [tests/planner/pipeline/0048_latest_select_completed_own.t](../../tests/planner/pipeline/0048_latest_select_completed_own.t) |
| TP-17 | Future publication cannot be selected by an earlier frame. | [tests/planner/pipeline/0049_latest_excludes_future.t](../../tests/planner/pipeline/0049_latest_excludes_future.t) |
| TP-18 | Indexed bypass resets palette provenance and detach starts a fresh session. | [tests/planner/pipeline/0050_latest_bypass_epoch.t](../../tests/planner/pipeline/0050_latest_bypass_epoch.t) |
| TP-19 | Own failure after emission fails the frame and drain while retaining the selected sequence. | [tests/planner/pipeline/0051_latest_own_failure_after_output.t](../../tests/planner/pipeline/0051_latest_own_failure_after_output.t) |
| TP-20 | Thread creation failure runs the same sample synchronously and selects its own palette. | [tests/planner/pipeline/0052_latest_thread_failure_same_sample.t](../../tests/planner/pipeline/0052_latest_thread_failure_same_sample.t) |
| TP-21 | An opaque full-capacity palette cannot replace the reserved own palette needed by the next delta frame. | [tests/planner/pipeline/0061_latest_delta_reservation.t](../../tests/planner/pipeline/0061_latest_delta_reservation.t) |
| TP-22 | RGB byte/float output and retained history agree with the selected palette, submission threshold limit and diffuse/skip policy. | [tests/planner/pipeline/0062_latest_delta_wire_history.t](../../tests/planner/pipeline/0062_latest_delta_wire_history.t) |
| TP-23 | HLS wire colors and retained history agree with the selected palette and current application settings. | [tests/planner/pipeline/0063_latest_delta_hls_history.t](../../tests/planner/pipeline/0063_latest_delta_hls_history.t) |
| TP-24 | Native export retains the selected palette after the own builder completes. | [tests/planner/pipeline/0064_latest_delta_native_export.t](../../tests/planner/pipeline/0064_latest_delta_native_export.t) |
| TP-25 | Current source masks and partial-plane origins bind independently of the selected palette source. | [tests/planner/pipeline/0065_latest_alpha_origin_history.t](../../tests/planner/pipeline/0065_latest_alpha_origin_history.t) |
| TP-26 | A stale reservation prediction joins and rebuilds from the pristine frame and selects the repaired own palette. | [tests/planner/pipeline/0066_latest_delta_prediction_redo.t](../../tests/planner/pipeline/0066_latest_delta_prediction_redo.t) |
| TP-27 | A representation transition retires already published future attempts and rebuilds them in the new epoch. | [tests/planner/pipeline/0067_latest_profile_epoch.t](../../tests/planner/pipeline/0067_latest_profile_epoch.t) |
| TP-28 | An older palette histogram cannot disable diffusion for the current frame. | [tests/planner/pipeline/0068_latest_stale_histogram.t](../../tests/planner/pipeline/0068_latest_stale_histogram.t) |
| TP-29 | Byte export retains the applied palette after the own builder completes. | [tests/planner/pipeline/0069_latest_delta_byte_export.t](../../tests/planner/pipeline/0069_latest_delta_byte_export.t) |
| TP-30 | GPU-enabled, builtin and high-color playback requests execute frame behavior with identical output bytes. | [tests/planner/pipeline/0070_latest_feature_fallback.t](../../tests/planner/pipeline/0070_latest_feature_fallback.t) |
| TP-31 | A write failure while an own build is outstanding retains the selected sequence, joins the build and interrupts queued frames in order; subsequent admissions and drain preserve the first failure. | [tests/planner/pipeline/0071_latest_write_failure.t](../../tests/planner/pipeline/0071_latest_write_failure.t) |
| TP-32 | Private byte/float snapshot clone allocation failure releases the partial copy and leaves the retained snapshot usable. | [tests/planner/pipeline/0072_palette_snapshot_allocation.t](../../tests/planner/pipeline/0072_palette_snapshot_allocation.t) |
| TP-33 | One-worker latest matches frame bytes for lazy float32 delta animation with serial loading and ahead preparation. | [tests/planner/pipeline/0073_latest_serial_frame_bytes.t](../../tests/planner/pipeline/0073_latest_serial_frame_bytes.t) |
| TP-34 | Emission receives N with outstanding build and preparation leases; a borrower waits without an idle slot and returns a revoked loan at its next job boundary, while emission never waits for it. | [tests/planner/pipeline/0075_latest_emit_priority_loans.t](../../tests/planner/pipeline/0075_latest_emit_priority_loans.t) |
| TP-35 | A real latest full-frame session borrows an idle slot for its owned input copy while retaining the full emission budget. | [tests/planner/pipeline/0076_latest_full_frame_copy_loan.t](../../tests/planner/pipeline/0076_latest_full_frame_copy_loan.t) |

### Coverage boundary

The [registry route suite](../../tests/cli/options/regression/) checks the remaining per-key assignments and scope declarations. The [mode coverage](../threading/policy-modes.md#test-coverage) owns scheduling and output comparisons. The [pipeline suite](../../tests/planner/pipeline/) also exercises owned requests, store lifetime, epoch invalidation, scheduler grants and construction fault recovery. Barrier tests control completion order without relying on sleeps. Byte equivalence applies to the stated fallback fixtures; timing-dependent latest output has no universal golden byte stream. DRCS, presentation/repair, macro receivers and real GPU execution retain their existing platform or terminal verification boundaries. Detected topology depends on the host; these tests do not emulate every operating system or prove physical CPU affinity.
