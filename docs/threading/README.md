# Threading

libsixel uses several short-lived worker groups rather than one global thread
pool. Start with the terminology and execution model before interpreting a
timeline or changing a thread-count policy.

- [Threading overview](overview.md) defines worker budgets, SIXEL bands,
  palette-application work bands, decoder byte spans, ordering, and the image
  quality implications of band boundaries.
- [Encoder threading](encoder.md) explains palette work, the dither/encode
  pipeline, its budget split, the ordered writer, and measured timelines.
- [Decoder threading](decoder.md) explains the serial parser anchor, parallel
  validation scan, ordered paint pass, and local-buffer decode path.
- [Animation threading](animation.md) explains loader/encoder handoff, frame
  ordering, temporal state, and the current lack of a process-wide budget.
- [Measurement and reproduction](measurement.md) defines the checked-in
  diagnostic protocol, one-command regeneration, validation, and a separate
  protocol for measuring band-seam quality.

The checked-in charts and raw records are under
[`measurements/`](measurements/). They are architectural observations from one
controlled run, not general throughput benchmarks.
