# Diagnostics, planner output, and timeline logs

The converters provide several observation channels. `-x` selects diagnostic policy, `img2sixel -v` prints additional encoder/planner information, and `-J PATH` records the shared JSON timeline. These channels answer different questions and have different stability and runtime costs.

| Channel | Use |
| --- | --- |
| Normal errors on stderr | Find why conversion or option parsing failed. |
| `-x code` | Select code-oriented status reporting for automation. This is not JSON and does not turn every trace into a stable record. |
| `-v` | Inspect the encoder's effective plan and additional debug information. Human output may evolve with implementation. |
| `-x human:trace_topic=LIST` | Inspect selected subsystem decisions, including runtime, loader, and handoff contracts. |
| `-J PATH` | Capture timestamps and worker/frame events for the timeline renderer. The output is JSON Lines, one object per line. |

Image data remains on its normal output stream. Redirect stderr separately when collecting diagnostics, and use `-o` to keep SIXEL/PNG bytes out of a terminal or text log.

## Diagnostic policy

The base mode is `human` by default or `code`. `SIXEL_DIAG_MODE` is the environment fallback. Explicit policy fields override the corresponding environment settings. The typed syntax, compact forms, repeated-setting behavior, and converter consumer scopes are defined by [suboptions](suboptions.md).

| Suboption | Purpose | Environment fallback |
| --- | --- | --- |
| `quiet=0\|1` (`Q`) | Reduce contextual option-error help in the encoder when code mode is selected. It is not a global suppression of every stderr writer. | `SIXEL_DIAG_MODE_QUIET` |
| `prefix_suggestions`, `fuzzy_suggestions`, `path_suggestions` (`P`, `F`, `S`) | Configure [correction suggestions](correction-suggestions.md). | `SIXEL_OPTION_PREFIX_SUGGESTIONS`, `SIXEL_OPTION_FUZZY_SUGGESTIONS`, `SIXEL_OPTION_PATH_SUGGESTIONS` |
| `force_colors=0\|1` (`C`) | Override normal status-color eligibility. Avoid it in logs that must not contain terminal control bytes. | `SIXEL_STATUS_FORCE_COLORS` |
| `trace_topic=LIST` (`T`) | Comma-separated topic selection. Each subsystem emits only the observations it implements. | `SIXEL_TRACE_TOPIC` |
| `handoff_trace=0\|1` (`H`) | Enable the encoder's minimal handoff trace. | `SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL` |
| `psd_trace`, `psd_header_only` (`D`, `E`) | Select PSD/PSB diagnostic decoding and trace detail; see [PSD](../loader/builtin/psd.md). Diagnostic-only decode can suppress ordinary SIXEL output. | `SIXEL_PSD_TRACE_ONLY`, `SIXEL_PSD_TRACE_HEADER_ONLY` |
| `abort_trace=0\|1` (`A`) | Configure the converter's [abort stack trace](abort-trace.md). | `SIXEL_ABORT_TRACE` |
| `log_lines=COUNT` (`N`) | Encoder timeline line-event sampling interval; CLI accepts 1..2147483647. It is not a maximum log-file length. | `SIXEL_LOG_LINES` |

Some entries are encoder-only; the shared registry rejects a suboption used with a converter that does not consume it. The environment can retain legacy parsing behavior: the current registry clamps an environment line interval below one to one, whereas the CLI `log_lines` field rejects zero. Leave `SIXEL_LOG_LINES` unset to avoid enabling line events through that environment control. Do not assume every accepted environment spelling has the same CLI grammar.

```sh
# Capture selected execution decisions without displaying the image.
img2sixel -v -x human:trace_topic=runtime_contract,loader,encode_handoff \
    -S -o /dev/null image.png 2>diagnostics.txt

# Request code-oriented reporting for a reproducible invalid argument.
img2sixel -x code -p invalid image.png
```

For option parsing, `img2sixel` can emit an `LSXCLI1|phase=option_parse|rc=...|code=...` record. The `rc` field is a library status, not necessarily the process exit code. On the ordinary non-OpenVMS CLI path, `img2sixel` maps success to 0, generic runtime failure to 1, invalid arguments to 2, and clipboard failure to 3. OpenVMS uses its platform condition mapping. Other executables' exit mappings must be checked independently. Scripts should check process status and parse only the record families they explicitly support.

## Recording and reading a timeline

```sh
img2sixel -S --threads=4 -J run.jsonl -o /dev/null image.png
python3 tools/timeline.py run.jsonl --output timeline.png
```

The renderer is a repository tool and requires its Python plotting dependencies. Use [threading measurements](../threading/measurement.md) for the maintained reproduction setup. `-J` overrides `SIXEL_LOG_PATH`. The writer opens a new log with truncation when logging initializes; use distinct paths for independent processes. The process shares the writer, and changing the log path after logging starts is rejected. Logging is not a per-image append-file selector.

Records contain `ts`, `session_id`, `thread`, `worker`, `role`, `event`, `job`, `frame_no`, `loop_no`, and `multiframe`. A line is an event, not necessarily a complete span. The renderer pairs and groups events; missing frame metadata, nested jobs, or a worker label do not independently prove concurrency or a worker-count contract. See [encoder timeline interpretation](../threading/encoder.md#timeline-interpretation) and [animation threading](../threading/animation.md).

Use `-S` or bounded playback when recording animations. An infinite loop produces an open-ended log. Smaller line-event intervals increase detail and logging overhead. Collect a trace to explain behavior, then measure timing separately with logging disabled; the trace changes I/O and scheduling costs. A completed conversion alone does not prove that a requested log file was successfully recorded, so inspect the artifact.

## Implementation

[`options-registry.c`](../../src/options-registry.c) owns policy fields and scopes. Converter-specific reporting is in [`img2sixel.c`](../../converters/img2sixel.c) and [`sixel2png.c`](../../converters/sixel2png.c). [`timeline-logger.c`](../../src/timeline-logger.c) attaches execution events, and [`timeline-writer.c`](../../src/timeline-writer.c) owns the shared file. [`timeline.py`](../../tools/timeline.py) interprets the event stream.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| DG-01 | CLI log-path selection overrides the environment; both forms create nonempty logs and leave image output equivalent. | [tests/cli/options/migration/0019_log_path_environment_cli_equivalence.t](../../tests/cli/options/migration/0019_log_path_environment_cli_equivalence.t) |
| DG-02 | Invalid encoder thread values produce the expected diagnostic. | [tests/cli/options/migration/0013_img2sixel_invalid_threads_diagnostic.t](../../tests/cli/options/migration/0013_img2sixel_invalid_threads_diagnostic.t) |
| DG-03 | Unset/empty line-event policy is disabled; legacy zero/negative/invalid intervals enable every-line sampling. | [tests/processing/timeline/0005_timeline_line_policy_environment.t](../../tests/processing/timeline/0005_timeline_line_policy_environment.t) |

### Coverage boundary

The [diagnostic suites](../../tests/diagnostics/) test selected record families, and the suggestion/abort documents own their respective controls. A parseable log does not establish scheduling performance or exhaustive event coverage. Human messages, planner layouts, and third-party decoder errors are not all stable machine interfaces merely because `-x code` was selected.
