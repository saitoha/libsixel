# Threading policy

This reference is for CLI users and library integrators configuring `-=` / `--threading-policy`. It owns syntax, defaults, precedence and consumer scopes. [Policy modes](../threading/policy-modes.md) explains stage scheduling and output effects; [runtime execution policy](runtime-policy.md) owns SIMD and minimum-work thresholds.

## Options

```sh
img2sixel -=balanced:global_budget=all:prepare_workers=2 image.png
img2sixel -=lazy:Ball:Q4 image.gif
sixel2png --threading-policy=greedy:B3:S2 -i image.six -o image.png
```

The syntax is `-= MODE[:KEY=VALUE...]` or `--threading-policy=MODE[:KEY=VALUE...]`, with a required base of `lazy`, `balanced` or `greedy`. A compact key is one uppercase letter immediately followed by its value, such as `B4` or `Ball`. `SIXEL_THREADING_POLICY` accepts a base only, without suboptions; its default is `balanced`.

The table gives every key, its compact form and its per-key environment source. Counts range from 1 through `INT_MAX` unless a narrower range is shown. “Automatic” means allocation from the resolved budget and mode, rather than a fixed worker count.

| Key | Short | Accepted values | Default | Environment | Consumer |
| --- | --- | --- | --- | --- | --- |
| `global_budget` | `B` | Positive integer or case-insensitive `all` | Library: `min(4, performance logical CPUs)`; converters: `all` | `SIXEL_THREADING_GLOBAL_BUDGET` | Both |
| `load` | `L` | `serial`, `ahead` | `ahead` | `SIXEL_THREADING_LOAD` | Encoder |
| `load_depth` | `Q` | 1..16 | 4 | `SIXEL_THREADING_LOAD_DEPTH` | Encoder |
| `prepare` | `P` | `serial`, `ahead` | `ahead` | `SIXEL_THREADING_PREPARE` | Encoder |
| `palette` | `C` | `frame`; `latest` is reserved and rejected | `frame` | `SIXEL_THREADING_PALETTE` | Encoder |
| `dither` | `D` | Positive integer | Automatic | `SIXEL_THREADING_DITHER` | Encoder |
| `encode` | `E` | Positive integer | Automatic | `SIXEL_THREADING_ENCODE` | Encoder |
| `prepare_workers` | `W` | Positive integer | Resolved budget | `SIXEL_THREADING_PREPARE_WORKERS` | Encoder |
| `decode` | `S` | Positive integer | Resolved budget when a mode or budget is explicit; otherwise 1 | `SIXEL_THREADING_DECODE` | Decoder |
| `pin` | `I` | `0`, `1` | Dither workers: 1; FHEDT workers: 0 | `SIXEL_THREADING_PIN` | Encoder |

`all` selects the logical CPU count. `balanced` then caps it, like an explicit numeric budget, to detected performance logical CPUs. The library's conservative default applies in every mode: selecting a mode alone does not enlarge it. In a build without threads, the effective count is one.

Both converters set empty or unset `SIXEL_THREADING_GLOBAL_BUDGET` to `all` and empty or unset `SIXEL_THREADING_POLICY` to `balanced`, after processing `--env`. Nonempty user values survive these defaults. Direct library callers receive neither converter default.

## Contract

The budget counts planned work slots, including participating callers, rather than every operating-system thread in the process. [Threading overview](../threading/overview.md#worker-budget-is-not-a-process-thread-limit) explains control threads and independent pools.

Stage limits shrink to the available budget; requesting more slots is never an error. `dither` and `encode` limit palette application and SIXEL byte generation. `prepare_workers` limits scaling, colorspace conversion, sampling and palette construction, independently of the emission budget. `decode` only reduces SIXEL parsing concurrency; `lazy` always parses serially.

`load` controls loader read-ahead and `load_depth` bounds its queued frames. `prepare=ahead` permits preparation of frame n+1 during emission of frame n. Publication remains ordered, one frame at a time. These keys and `prepare_workers` preserve output. [Animation threading](../threading/animation.md) describes ownership and the separate public playback queue capacity.

Numeric `global_budget=0`, negative values and words other than `all` fail CLI parsing with a diagnostic naming the positive-integer-or-`all` grammar. Invalid per-key environment values are ignored, leaving the next applicable default. `palette=latest` is recognized but returns `SIXEL_NOT_IMPLEMENTED`; it does not select a different palette or silently fall back.

`img2sixel` accepts encoder keys; `sixel2png` accepts `global_budget` and `decode`. A key outside the converter's scope is rejected. An invalid policy argument leaves the stored policy unchanged.

## Precedence and interactions

For the global budget, resolution is: explicit CLI/policy `global_budget`, then `sixel_set_threads(n)`, then `SIXEL_THREADING_GLOBAL_BUDGET`, then the library default. CLI and API requests occupy separate fields, so a later API call cannot replace an explicit policy key. The API maps `n <= 0` to `all`; it does not reset the budget to the library default.

The base resolves independently: an explicit policy base wins over `SIXEL_THREADING_POLICY`, which wins over `balanced`. Other keys resolve from explicit fields in the policy argument, then their per-key environment variables, then stage defaults. Policy configuration is process-wide; establish it before starting concurrent operations.

Repeated policy arguments replace the base and refresh non-budget keys from the current argument and environment. An omitted global budget preserves an earlier explicit CLI budget and the independent API request. The mode cap applies after budget precedence. For example, an API budget of eight still resolves below eight if `balanced` detects fewer performance logical CPUs.

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
| TP-02 | Modes preserve the library default; CLI/API/environment precedence, `all`, numeric rejection, atomic failure, reserved palette rejection and decoder limits are enforced. | [tests/processing/threadpool/0003_thread_policy.t](../../tests/processing/threadpool/0003_thread_policy.t) |
| TP-03 | Short, long and environment `all` routes store the stable sentinel and preserve output through both converters. | [tests/cli/options/regression/0185_threading_global_budget_image_regression.t](../../tests/cli/options/regression/0185_threading_global_budget_image_regression.t) |
| TP-04 | Converter defaults resolve to the available performance CPU budget, with user environment and CLI overrides. | [tests/processing/threadpool/0004_converter_default_budget.t](../../tests/processing/threadpool/0004_converter_default_budget.t) |
| TP-05 | Load depth is routed through the registry with range fallback and output equivalence. | [tests/cli/options/regression/0180_threading_load_depth_image_regression.t](../../tests/cli/options/regression/0180_threading_load_depth_image_regression.t) |

### Coverage boundary

The [registry route suite](../../tests/cli/options/regression/) checks the remaining per-key assignments and scope declarations. The [mode coverage](../threading/policy-modes.md#test-coverage) owns scheduling and output comparisons. Detected topology depends on the host; these tests do not emulate every operating system or prove physical CPU affinity.
