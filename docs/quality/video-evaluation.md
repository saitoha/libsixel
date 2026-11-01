# Video reconstruction, timing and residual persistence

For a runnable introduction and a capture-recording procedure, see the [video evaluation workflow](video-workflow.md).

`lsqa -M animation` compares one decoded reference animation cycle against a retained SIXEL display canvas. `-Ma` is the unique-prefix shorthand. Default `-A frame` measures reconstruction quality at matching source frames. `-A clock` evaluates what is available at each reference time using an explicit display timeline; delayed output therefore leaves an older canvas or the initial background in the comparison. Reports also describe screen-coordinate residual persistence. These diagnostics do not model gaze or motion-dependent temporal perception. The general [measurement policy](measurement-policy.md) governs controlled comparisons; [S-CIELAB](metrics.md#spatial-color-difference-s-cielab) defines the spatial metric.

```sh
assessment/lsqa -Ma -m S-CIELAB -o frames.jsonl reference.apng output.six
assessment/lsqa -Ma -J timeline.jsonl -o frames.jsonl -E errors.f32 \
    reference.apng output.six
assessment/lsqa -Ma -Ac -C display.jsonl -f30 -t0.1 -u2 \
    -o clock.jsonl -E clock.f32 reference.apng output.six
```

The reference uses the selected animation-capable loader, including its composition, disposal, colorspace and alpha rules. APNG and animated GIF are supported by the builtin loader. The target is a fixed-canvas SIXEL capture. Both images must have identical dimensions; no crop, resize, frame-count truncation or content-based re-pairing conceals a mismatch. `-M single` retains its first-image behavior, while `-M6` still composes all passes into one still image. Animation is an explicit lsqa operation, not a decoder mode accepted by `sixel2png`.

## Arguments

| Short and long form | Contract | Environment |
| --- | --- | --- |
| `-M animation`, `--input-mode=animation` | Opt into video assessment. | Explicit CLI only, like other input modes. |
| `-o FILE`, `--frame-report=FILE` | Write JSONL metadata, frame or clock-segment records, and a final summary. | `LSQA_FRAME_REPORT` |
| `-A MODE`, `--alignment=MODE` | `frame` (default) or `clock`, accepting unique prefixes. Clock requires `-C` and excludes `-J`. | `LSQA_ALIGNMENT` |
| `-C FILE`, `--display-timeline=FILE` | Supply complete-packet availability times for clock alignment. | `LSQA_DISPLAY_TIMELINE` |
| `-t SECONDS`, `--persistence-time=SECONDS` | Time for the residual weight to rise from zero to one, finite and nonnegative. Default 0.1 seconds; zero applies full weight immediately. Explicit use requires `-o`. | `LSQA_PERSISTENCE_TIME` |
| `-J FILE`, `--target-timeline=FILE` | Supply logical frame boundaries and durations independently of DCS packet count. | `LSQA_TARGET_TIMELINE` |
| `-E FILE`, `--error-map=FILE` | Write row-major spatial CIE76 error maps as little-endian IEEE-754 float32 samples. Requires `-o`. | `LSQA_ERROR_MAP` |
| `-u VALUE`, `--scielab-threshold=VALUE` | Count pixels whose spatial CIE76 error is strictly greater than this finite nonnegative value. Default 2 is diagnostic, not a validated perceptual pass threshold. | `LSQA_SCIELAB_THRESHOLD` |
| `-f RATE`, `--reference-fps=RATE` | Replace reference frame durations with `1/RATE` seconds. Accept a positive number or ratio such as `30000/1001`, at most 1000 fps. Conflicts with `-J`. | `LSQA_REFERENCE_FPS` |

The shared CLI precedence is explicit option, environment, then default. Empty environment values are unset. File options require named files, not `-`; target stdin remains available. Video-specific settings require `-Ma`. Reconstruction requires `-d none`, and reference interpretation requires `-R auto`, so the loader owns reference metadata. `-T` retains the original SIXEL color interpretation, including Display-P3 and ICC. No additional dither or palette quantization is applied during evaluation.

With frame alignment and without `-J`, the caller declares that each complete DCS is exactly one source frame. Both counts must agree. This interpretation is unsuitable for skipped output frames, repair packets or high-color multipass output; use an explicit timeline for those streams. A matching count is validation of the count, not proof of the caller's frame correspondence.

By default, frame durations come from the loader's centisecond metadata. This cannot preserve every APNG rational duration, including exact 30 fps. Use `-f30`, `-f30000/1001`, or an external timeline when that precision matters. A missing or zero duration is an error rather than an invented playback rate. One animation cycle is evaluated without replay sleeps.

## External timeline

Each JSONL record has exactly these four numeric members, in any order:

```json
{"frame":0,"byte_end":3762,"pts":0,"duration":0.04}
{"frame":1,"byte_end":7530,"pts":0.04,"duration":0.08}
{"frame":2,"byte_end":7530,"pts":0.12,"duration":0.12}
```

`frame` is the zero-based reference frame number, contiguous from zero. `byte_end` is the cumulative exclusive offset immediately after the last complete DCS belonging to that frame, measured in the original capture, including preceding whitespace and permitted cursor framing. Offsets are nondecreasing and may group several DCS packets. Repeating an offset declares a no-output frame: keep the previous canvas and still compare it against the new reference. The first frame must provide a baseline packet. A boundary inside a packet is invalid.

`pts` and positive `duration` are seconds on a normalized source timeline beginning at zero. Intervals must be contiguous; boundary consistency allows `1e-9 * (1 + previous_end)` seconds of numeric roundoff. These fields explicitly supply the reference timing; they are not writer timestamps. Every reference frame needs a record, including frames with no transmitted bytes. Extra records, missing records, duplicate or unknown members, nonfinite numbers and malformed packets fail the operation. Records are limited to 1023 bytes, and the capture retains the existing decoder's `INT_MAX` byte-size limit.

The timeline is an evaluation sidecar, not an in-stream attribute or a decision about a future IDCS format. Producers must record the true source correspondence; lsqa does not infer generation boundaries from palette contents or image similarity.

## Display timeline and clock alignment

`-A clock -C display.jsonl` uses a separate sidecar with exactly three numeric members per record:

```json
{"update":0,"byte_end":3762,"pts":0.02}
{"update":1,"byte_end":7530,"pts":0.10}
{"update":2,"byte_end":11298,"pts":0.24}
```

`update` is contiguous from zero. `byte_end` strictly increases and ends at a complete DCS boundary. An update may group several complete packets, and every packet in the capture must belong to an update. `pts` is a finite nonnegative availability time in seconds, nondecreasing across updates. Equal-time updates apply in capture order before measuring the interval on their right. Zero-duration samples are not emitted. There is no entry for a no-output frame: retain the latest canvas until another update arrives. The strict JSON grammar, record size and capture limits also apply here.

The caller establishes time zero at the intended start of reference playback and supplies times on that same clock. Do not subtract the first output's delay or align by image content. Reference durations come from its loader or `-f`; `-J` is incompatible because its frame-grouping semantics do not describe availability events. The display sidecar requires `-A clock`, while `-A frame` retains source correspondence independently of availability.

The timestamp denotes when the declared complete packet range becomes available. A writer may use measured completion times as a proxy, but successful writes do not establish physical terminal presentation. Record the capture method externally; the report labels the basis `caller-supplied packet availability`. The evaluator treats the grouped packets as an atomic update and does not simulate partial DCS painting, terminal processing, vsync or scanout. Measuring those requires a different capture source, not an invented delay correction.

Evaluation covers exactly one reference cycle, `[0, reference_end)`. Before the first update the display is the assessment background (`-B`, otherwise black); this interval contributes to quality and `initial_blank_duration`. At each reference boundary or display update, lsqa evaluates the resulting constant reference/display pair and weights it by the interval duration. Updates inside a reference frame create additional `segment` records, so brief errors are not lost through frame-boundary sampling. Each segment includes its zero-based reference `frame`, global `segment` index, `pts` and `duration`. Clock reports use schema `lsqa-clock-report-v1`; frame reports retain `lsqa-frame-report-v1`.

Updates at or beyond the reference end are validated but do not extend the evaluation window with a frozen reference. The summary records their `late_updates` and `late_bytes`; all transmitted bytes still count in `bytes` and `bytes_per_second`. `frame_bytes` is the prefix made available within the evaluation window, and `trailing_bytes` counts non-packet tail framing as in frame mode. These three byte categories sum to the capture size. `segments` counts evaluated intervals; `frames` still counts reference frames. A missing mapped packet or malformed late event fails the whole evaluation even if it lies outside the score window.

## Residual persistence diagnostic

Reports track the duration of continuous strict `S-CIELAB error > -u` at each screen pixel. Error at or below the threshold resets that pixel's age to zero; otherwise age carries across reference and display boundaries. At each instant, the excess error `max(error - threshold, 0)` is multiplied by `min(age / ramp_seconds, 1)`. With `-t0`, this weight is one immediately. The linear ramp is integrated over each constant-error interval, including a ramp completion inside that interval. Thus inserting packet or frame boundaries without changing the error does not change the integral. The default 100 ms is an adjustable diagnostic choice, not a psychophysically validated threshold.

Each frame or segment includes a `persistence` object:

- `weighted_excess`: pixel-averaged excess spatial CIE76 error after integrating the ramp weight over the interval. Lower is better. The units remain spatial CIE76, not CIEDE2000 or seconds.
- `persistent_fraction`: the fraction of pixel-time above the threshold for at least the ramp duration. With a zero ramp, this equals the ordinary above-threshold coverage.
- `max_age`: the largest continuous above-threshold age at the interval end, in seconds.

The summary duration-weights `weighted_excess` and `persistent_fraction` and takes the maximum of interval `max_age`. Metadata records the versioned model `screen-exceedance-linear-ramp-v1`, the threshold, ramp duration and `motion_compensated:false`. These values are diagnostics in the report, not additional `-m` metrics or calibrated `-b` acceptance scores. Keep the ordinary S-CIELAB distributions and MS-SSIM alongside them so reducing persistence cannot hide an increase in spatial error.

This model follows screen locations, not objects, chromatic direction or artifact identity. A changing above-threshold error at the same location continues the age; a residual moving into a different location starts a new age there. Scene cuts do not automatically reset ages. Consequently it detects persistent screen-local banding or stale regions but cannot by itself establish perceptual persistence of a moving trail, separate banding from other low-frequency error, or implement fixation-aware weighting. Motion correspondence and observer validation remain outside this diagnostic's contract.

## Canvas and byte accounting

The target decoder keeps original paint-time RGBA and color-register state across packets. Unpainted pixels in `P2=1` packets retain their old colors even when a register is redefined. `P2=0` or an omitted P2 clears the canvas before painting while retaining color registers. Unpainted background is flattened against `-B` or the default black only for assessment. This capture profile does not import a terminal's preexisting display, background or palette.

Each packet must declare the same positive raster with unit pixel aspect, within the existing 6000-by-6000 decoder bound, and color registers start from the existing single-image decoder default palette. Each DCS initially selects register 15, like that decoder; later definitions persist across packets. The report records this initial palette and register, so default-palette output is not mistaken for a capture of a terminal-specific palette. Accepted framing is raw DCS packets, a single `ESC 7` anchor followed by `ESC 8` before subsequent packets, or `ESC [ H` before every packet as emitted by non-terminal animation output. Whitespace can separate packets. Arbitrary cursor movement, scrolling, other terminal controls, OR composition and damage rectangles with different rasters are not supported. This is a fixed-origin capture decoder, not a terminal emulator.

In frame mode, per-frame `byte_start`, `byte_end`, `bytes` and `cumulative_bytes` account for the source ranges consumed by that logical frame. `packets` and `cumulative_packets` count its complete DCS packets. Trailing permitted whitespace belongs to summary `trailing_bytes`; summary `bytes = frame_bytes + trailing_bytes` covers the entire capture. `bytes_per_second` divides total bytes by reference duration and is encoded-stream cost, not measured network throughput. A no-output frame costs zero bytes but retains its display error.

## Report and map semantics

The metadata record identifies the schema, alignment, paths, mapping and timing sources, comparison settings, background, S-CIELAB model, PPD, difference formula, boundary convention and persistence settings. Every frame or clock segment records dimensions, PTS and duration, byte/packet accounting, computed quality metrics, and the S-CIELAB distribution:

- `mean` and `max` summarize spatial CIE76 error over all pixels.
- `p95` is the exact nearest-rank percentile at rank `ceil(0.95 * width * height)`, with one-based ranks. It is a percentile over pixels of this frame or segment, not over time.
- `threshold`, `above_count` and `above_fraction` describe strict `error > threshold` coverage over all pixels.

With `-o`, video assessment computes S-CIELAB mean/max and reference MS-SSIM in addition to the selected metrics. MS-SSIM is unavailable below the [minimum dimensions](metrics.md#reference-ms-ssim). Without a report, a selected or gated invocation computes only the union of its requested metrics, and no error-map storage is allocated. A single unique `-m` prints the selected metric's duration-weighted temporal mean; multiple selections print named means as specified by the [metric selection and gate interface](metrics.md#selecting-metrics-and-gates). `-o` records the additional diagnostics without contaminating selected stdout. Without a selected metric or baseline, stdout contains the summary JSON. Every `-b` condition applies to the duration-weighted mean, not the worst frame; all conditions must pass.

The summary contains duration-weighted `quality` means and unweighted temporal minima/maxima. For example, temporal maximum of `S-CIELAB_max` is the largest error observed in any frame, whereas its `quality` entry is a duration-weighted average of frame or segment maxima. Nonfinite computed values serialize as JSON `null`; temporal extrema are omitted for any metric having a nonfinite frame. Such metrics are not silently averaged over just their finite frames.

`-E` appends one unnormalized float32 error plane per frame record or clock segment. Metadata records its path, little-endian format, row-major order and spatial CIE76 units; each frame or segment records its byte offset and byte count. Width and height are in that record, and every plane occupies `width * height * 4` bytes. Summary statistics are computed from double-precision errors before the export's float32 rounding. These maps preserve magnitude and position, not signed chromatic residuals or motion correspondence. Their interval durations permit independent reconstruction of the persistence diagnostic; the exported samples themselves remain unweighted spatial errors.

Both inputs and all frame boundaries are validated before named outputs are opened. Report/map paths must be distinct from each other and from both inputs and either timeline file. Temporary streams bound memory usage; optional maps consume temporary disk space proportional to the number of evaluated frames or segments. A complete report ends with a `complete:true` summary. Input failures leave existing report/map files untouched. Output I/O failures return an error and may leave incomplete destination files; successful completion requires the process exit status as well as the footer.

Implementation boundaries are [the video driver](../../assessment/video.c), [assessment map extraction](../../assessment/assessment.c), and [the retained SIXEL painter](../../src/fromsixel.c). There is no content-based re-pairing, range selection, native MP4 decoding, optical flow or gaze model in this operation. Color/byte tradeoff sweeps and encoder CPU/GPU timing belong to an external experiment runner using these reports.

## Test coverage

<!-- test-coverage: enforced -->

The APNG fixture uses `tests/data/inputs/snake_16.png`, its horizontal reflection, and that reflected image again, with durations 0.04, 0.08 and 0.12 seconds. Frozen SIXEL packets were generated independently per still with builtin loading and no dither; timeline variants group a palette-only packet or omit a repeated frame. The stale-output variant retains the first image across all three references. These fixtures test mapping, accounting and aggregation, not an independent numerical oracle for quantization. Numerical map extraction has a separate uniform-field oracle.

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| VQ-01 | Per-frame reports contain ordered frames, byte totals and distribution coverage. | [tests/cli/lsqa-video/0001_frame_report.t](../../tests/cli/lsqa-video/0001_frame_report.t) |
| VQ-02 | No-output frames preserve the canvas and cost zero bytes. | [tests/cli/lsqa-video/0002_held_frame.t](../../tests/cli/lsqa-video/0002_held_frame.t) |
| VQ-03 | Explicit logical frames can group multiple complete packets. | [tests/cli/lsqa-video/0003_grouped_packets.t](../../tests/cli/lsqa-video/0003_grouped_packets.t) |
| VQ-04 | Temporal means use duration weights, including stale display intervals. | [tests/cli/lsqa-video/0004_duration_weighted_summary.t](../../tests/cli/lsqa-video/0004_duration_weighted_summary.t) |
| VQ-05 | Map sample counts and frame offsets match the image geometry. | [tests/cli/lsqa-video/0005_map_stream.t](../../tests/cli/lsqa-video/0005_map_stream.t) |
| VQ-11 | Exported map values reproduce the distribution within the float32 rounding budget. | [tests/cli/lsqa-video/0014_map_distribution.t](../../tests/cli/lsqa-video/0014_map_distribution.t) |
| VQ-06 | Rational reference FPS overrides loader durations. | [tests/cli/lsqa-video/0009_reference_fps.t](../../tests/cli/lsqa-video/0009_reference_fps.t) |
| VQ-07 | Explicit zero threshold overrides the environment. | [tests/cli/lsqa-video/0010_threshold_cli_over_environment.t](../../tests/cli/lsqa-video/0010_threshold_cli_over_environment.t) |
| VQ-08 | Caller-owned assessment maps expose the analytic uniform-field error. | [tests/assessment/scielab/0009_scielab_map_output.t](../../tests/assessment/scielab/0009_scielab_map_output.t) |
| VQ-09 | Home-framed animation preserves previous paint across register redefinition. | [tests/decoder/parser/0097_animation_palette_carry.t](../../tests/decoder/parser/0097_animation_palette_carry.t) |
| VQ-12 | Animation uses the ordinary decoder initial palette and register. | [tests/decoder/parser/0100_animation_default_palette.t](../../tests/decoder/parser/0100_animation_default_palette.t) |
| VQ-10 | Clear packets reset the display but retain color definitions. | [tests/decoder/parser/0098_animation_clear_canvas.t](../../tests/decoder/parser/0098_animation_clear_canvas.t) |
| VQ-13 | Clock samples every reference/display intersection and integrates quality by duration. | [tests/cli/lsqa-video/0015_clock_intersections.t](../../tests/cli/lsqa-video/0015_clock_intersections.t) |
| VQ-14 | A delayed baseline includes the initial background in quality and duration accounting. | [tests/cli/lsqa-video/0016_clock_initial_background.t](../../tests/cli/lsqa-video/0016_clock_initial_background.t) |
| VQ-15 | Updates at the reference end cost bytes without extending or improving the score. | [tests/cli/lsqa-video/0017_clock_late_bytes.t](../../tests/cli/lsqa-video/0017_clock_late_bytes.t) |
| VQ-16 | Equal-time updates are applied in order before the next positive-duration segment. | [tests/cli/lsqa-video/0018_clock_simultaneous_updates.t](../../tests/cli/lsqa-video/0018_clock_simultaneous_updates.t) |
| VQ-17 | Clock map offsets and lengths index one plane per segment. | [tests/cli/lsqa-video/0019_clock_map_stream.t](../../tests/cli/lsqa-video/0019_clock_map_stream.t) |
| VQ-18 | Report persistence agrees with an independent integral from exported maps. | [tests/cli/lsqa-video/0024_persistence_report.t](../../tests/cli/lsqa-video/0024_persistence_report.t) |
| VQ-19 | Explicit zero ramp overrides the environment and applies full excess-error weight. | [tests/cli/lsqa-video/0025_persistence_cli_zero.t](../../tests/cli/lsqa-video/0025_persistence_cli_zero.t) |
| VQ-20 | A short residual receives a smaller mean weight than an equally large persistent residual. | [tests/assessment/persistence/0001_persistence_duration_weighting.t](../../tests/assessment/persistence/0001_persistence_duration_weighting.t) |
| VQ-21 | Equality resets only the corresponding pixel age, retaining other pixel histories. | [tests/assessment/persistence/0002_persistence_pixel_reset.t](../../tests/assessment/persistence/0002_persistence_pixel_reset.t) |
| VQ-22 | Splitting a constant residual interval leaves the time-integrated score unchanged. | [tests/assessment/persistence/0003_persistence_partition_invariance.t](../../tests/assessment/persistence/0003_persistence_partition_invariance.t) |

### Defensive and malformed-input tests

| ID | Contract | Owning test |
| --- | --- | --- |
| VQD-01 | A malformed late packet preserves an existing report. | [tests/cli/lsqa-video/0006_late_error_preserves_report.t](../../tests/cli/lsqa-video/0006_late_error_preserves_report.t) |
| VQD-02 | A shorter target cannot silently truncate reference evaluation. | [tests/cli/lsqa-video/0007_frame_count_mismatch.t](../../tests/cli/lsqa-video/0007_frame_count_mismatch.t) |
| VQD-03 | Duplicate timeline members are invalid. | [tests/cli/lsqa-video/0008_duplicate_timeline_key.t](../../tests/cli/lsqa-video/0008_duplicate_timeline_key.t) |
| VQD-04 | Nonfinite coverage thresholds are rejected. | [tests/cli/lsqa-video/0011_nonfinite_threshold.t](../../tests/cli/lsqa-video/0011_nonfinite_threshold.t) |
| VQD-05 | Map export requires its report index. | [tests/cli/lsqa-video/0012_map_requires_report.t](../../tests/cli/lsqa-video/0012_map_requires_report.t) |
| VQD-06 | Timeline and FPS overrides cannot conflict. | [tests/cli/lsqa-video/0013_timeline_fps_conflict.t](../../tests/cli/lsqa-video/0013_timeline_fps_conflict.t) |
| VQD-07 | An undersized map cannot be written or reported as valid. | [tests/assessment/scielab/0010_scielab_map_capacity.t](../../tests/assessment/scielab/0010_scielab_map_capacity.t) |
| VQD-08 | A malformed next packet invalidates the sequence. | [tests/decoder/parser/0099_animation_malformed_tail.t](../../tests/decoder/parser/0099_animation_malformed_tail.t) |
| VQD-09 | An unmapped capture tail cannot silently escape clock validation. | [tests/cli/lsqa-video/0020_clock_unmapped_tail.t](../../tests/cli/lsqa-video/0020_clock_unmapped_tail.t) |
| VQD-10 | Availability times cannot run backwards. | [tests/cli/lsqa-video/0021_clock_time_regression.t](../../tests/cli/lsqa-video/0021_clock_time_regression.t) |
| VQD-11 | Availability byte boundaries cannot split a DCS packet. | [tests/cli/lsqa-video/0022_clock_split_packet.t](../../tests/cli/lsqa-video/0022_clock_split_packet.t) |
| VQD-12 | Clock alignment requires an explicit display timeline. | [tests/cli/lsqa-video/0023_clock_requires_timeline.t](../../tests/cli/lsqa-video/0023_clock_requires_timeline.t) |

The quality floor in the normal fixture guards gross reconstruction damage. These tests do not establish a subjective persistence threshold, a perceptual benefit from feedback, or real-time playback performance.
