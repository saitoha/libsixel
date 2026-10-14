# Evaluate a SIXEL video capture with lsqa

This tutorial starts with a small repository fixture, then explains how to record a larger experiment with source-frame and display-availability timelines. Build `converters/img2sixel` and `assessment/lsqa` first and run the commands from the repository root. The complete option and report contracts belong to [video evaluation](video-evaluation.md); the [measurement policy](measurement-policy.md) governs comparisons.

## Encode one animation cycle and measure its reconstruction

The fixture has three 16×16 frames with durations 0.04, 0.08 and 0.12 seconds. The third frame repeats the second. This configuration emits one complete SIXEL DCS per input frame, including the repeated frame, and introduces no progressive repair packets.

```sh
converters/img2sixel tests/data/assessment/video-snake-16.png \
    -L'builtin!' --cms-engine=none --loop-control=disable \
    -g -Zdelta:P0 -dnone -Goff -~none > video.six

assessment/lsqa -Ma -Aframe -L'builtin!' --cms-engine=none \
    -dnone -Rauto -Tsrgb -Wlinear -Pfloat32 \
    -m S-CIELAB -v60 -u2 -t0.1 -o frames.jsonl \
    tests/data/assessment/video-snake-16.png video.six
```

The selected metric's duration-weighted mean appears on stdout. `frames.jsonl` contains metadata, three frame records, and a final summary with `complete:true`. Require both a successful exit and that footer before accepting a result. The report includes S-CIELAB distribution, reference MS-SSIM and residual persistence even though the selected scalar metric is S-CIELAB.

`-g` makes encoding unpaced in this example. The frame report measures reconstruction, not playback latency. Do not invent display timestamps from the frame numbers or treat this capture's generation times as normal real-time playback. A real-time experiment needs the separate recording step below.

For a sufficiently large video, select `-m MS-SSIM` to collect the reference structural metric together with the report's S-CIELAB diagnostics. The [minimum dimensions](metrics.md#reference-ms-ssim) apply; the small tutorial fixture reports this metric as unavailable. Record the metric name with each score.

## Represent frames that transmit no packet

Use an explicit source timeline whenever the producer can omit a frame or send multiple packets for one frame. The supplied held-frame fixture contains two complete packets for the same three-frame reference:

```sh
assessment/lsqa -Ma -Aframe -L'builtin!' --cms-engine=none \
    -dnone -Rauto -Tsrgb -Wlinear -Pfloat32 \
    -m S-CIELAB -v60 -u2 -t0.1 \
    -J tests/data/assessment/video-snake-16-held.jsonl \
    -o held.jsonl tests/data/assessment/video-snake-16.png \
    tests/data/assessment/video-snake-16-held.six
```

The third timeline row repeats the second row's `byte_end`, so lsqa retains the previous canvas and evaluates it for the third reference interval. A packet-count mismatch without `-J` would be an error. The byte offsets in that fixture belong only to that exact capture; do not reuse them for a freshly encoded file.

## Record your own source and display timelines

Prepare one reference animation cycle with a fixed canvas and a documented color interpretation. APNG and animated GIF are supported by the builtin loader; convert other video formats before evaluation. Preserve frame correspondence, record any crop or rate conversion, and keep the same prepared input for all encoders. A 24fps source converted to 25fps does not become a native 25fps recording.

The recorder has two independent responsibilities:

1. For every source frame, record its index, source PTS, duration, and the cumulative capture offset through the last complete DCS generated for it. Include no-output frames. A source frame skipped before encoding repeats the preceding offset. Do not infer this association merely by counting DCS packets.
2. At the output sink, record when each complete DCS range becomes available. Complete a short write before marking its bytes available. If one write completes several packets, they may share a timestamp. Do not treat queue admission as write completion, and do not subtract the first packet's delay.

Use the same monotonic time origin for intended source playback and output availability. A file-write completion or a pipe-reader observation is a measurement boundary that must be named in the experiment; neither establishes physical terminal presentation. Source PTS remain independent of encode start times. Record load, encode, queue and write timings separately when those costs matter.

The recorder writes `frame-timeline.jsonl` using the [source timeline schema](video-evaluation.md#external-timeline) and `display-timeline.jsonl` using the [display timeline schema](video-evaluation.md#display-timeline-and-clock-alignment). Byte offsets include the capture's framing and must end at complete DCS boundaries. The first source frame needs a baseline packet. Include packets that arrive after the reference ends in the display timeline so their byte cost remains visible.

When imposing a queue or bandwidth limit, use the same policy across candidates. Drop source work before encoding when necessary. Removing an already encoded delta packet invalidates the retained history assumed by later packets. Keep a failed or partial-write capture explicitly incomplete; do not pass it off as a successfully reconstructed video.

## Compare frame and clock results

Once capture has finished, run assessment separately from the timed encoder. The following example assumes a prepared 25fps reference and a recorder that produced both sidecars:

```sh
assessment/lsqa -Ma -Aframe -L'builtin!' --cms-engine=none \
    -dnone -Rauto -Tsrgb -Wlinear -Pfloat32 \
    -m MS-SSIM -v60 -u2 -t0.1 \
    -J frame-timeline.jsonl -o aligned.jsonl reference.apng capture.six

assessment/lsqa -Ma -Aclock -L'builtin!' --cms-engine=none \
    -dnone -Rauto -Tsrgb -Wlinear -Pfloat32 \
    -m MS-SSIM -v60 -u2 -t0.1 \
    -C display-timeline.jsonl -f25 -o clock.jsonl reference.apng capture.six
```

`-J` provides reference timing, so the frame command does not also take `-f`. Clock mode uses the independent display timeline and obtains source timing from the loader or an explicit `-f`. For exact 30fps, select `-f30`; loader centisecond metadata alone cannot preserve every rational frame period.

These examples deliberately fix an unprofiled sRGB input, builtin loading without CMS, sRGB target interpretation and a float32 linear-luminance comparison signal. For profiled or wide-gamut sources, establish the appropriate [color-aware evaluation](color-evaluation.md) configuration first. Hold it constant across candidates; do not remove source profiles just to make an example command apply.

## Read quality, persistence and cost together

Frame alignment answers how accurately a completed logical frame reconstructs its matching source. Clock alignment answers what image was available during each reference interval. A candidate may improve the first result while worsening the second through late output. Clock records split at both reference and display changes, so their count need not match the source-frame count.

Compare the following observations without collapsing their different units into an arbitrary total score:

| Observation | How to use it |
| --- | --- |
| S-CIELAB mean and spatial p95 | Measure spatially filtered color error and its tail; lower is better. S-CIELAB is not exclusively a banding detector. |
| `above_fraction` | Inspect the screen area exceeding the chosen diagnostic threshold. |
| `persistence.weighted_excess` | Inspect errors that exceed the threshold and remain at the same screen coordinates; a longer run receives more weight. |
| `persistent_fraction`, `max_age` | Check the extent and duration of continuous residual runs. These are not motion-compensated object tracks. |
| MS-SSIM | Guard against structural damage with a reference-product baseline. |
| Summary `bytes`, `bytes_per_second` | Compare the complete encoded stream cost, including late packets. This is not measured network throughput. |
| Recorder latency, dropped frames and queue size | Establish whether a quality gain survives the playback deadline and output budget. |

Use the interval durations when aggregating frame or segment statistics. The report's S-CIELAB spatial p95 is a percentile across pixels, not a temporal percentile. Likewise, summary `quality["S-CIELAB_max"]` is a time average of spatial maxima; `temporal_max` gives the peak over the sequence. Inspect brief structural drops as well as the mean.

Keep PPD, threshold and persistence ramp fixed for the primary comparison. `-v60 -u2 -t0.1` specifies one diagnostic setup, not a validated universal perceptual grade. Use predetermined sensitivity checks rather than selecting the threshold that makes a candidate win. Check known short and long errors, an identical-image control and deliberately delayed output before trusting a campaign.

For localized diagnosis, add `-E errors.f32` to a short run with `-o`. Its raw float32 planes and per-record offsets are described in [report and map semantics](video-evaluation.md#report-and-map-semantics). At 1440×800, each plane is about 4.6 MB; 100 frame records use about 461 MB, and clock alignment can produce more records. The JSONL report is sufficient for routine full-run summaries.

Archive the input and runtime hashes, actual loaded library, encoder and lsqa arguments, environment overrides, backend evidence, both sidecars, capture and reports. For a palette optimization, include a matched control that performs the same refinement without temporal feedback, then compare against ordinary encoding at nearby color counts. A smaller stream at the same requested color count is useful evidence, but the final tradeoff is the bytes and time needed to meet the same quality conditions.
