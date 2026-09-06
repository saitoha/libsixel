# Threading

libsixel uses several short-lived worker groups rather than one global thread
pool. Start with the terminology and execution model before interpreting a
timeline or changing a thread-count policy.

- [Threading overview](overview.md) defines worker budgets, SIXEL bands,
  palette-application work bands, decoder byte spans, ordering, and the image
  quality implications of band boundaries.
- [Encoder threading](encoder.md) explains palette work, the dither/encode
  pipeline, its budget split, the ordered writer, Full HD `img2sixel`
  timelines, repeated worker-scaling results, and the requested-palette-size
  sweep at two, four, six, and eight worker budgets.
- [Decoder threading](decoder.md) explains the serial parser anchor, parallel
  validation scan, row-independence proof, parallel paint barrier, and
  local-buffer decode path, with repeated Full HD worker-scaling results.
- [Animation threading](animation.md) explains loader/encoder handoff, frame
  ordering, temporal state, and the current lack of a process-wide budget.
- [Measurement and reproduction](measurement.md) defines the checked-in
  diagnostic protocol, one-command regeneration, validation, and a separate
  protocol for measuring band-seam quality.

The checked-in charts and raw records are under
[`measurements/`](measurements/). Most are architectural observations from one
controlled run. The encoder and decoder scaling charts use repeated samples,
but remain host-specific characterizations rather than general throughput
benchmarks.
