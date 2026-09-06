# Decoder Threading

## Why decoding is split by bytes

A SIXEL stream states vertical movement through control sequences in the input.
Before parsing those sequences, an arbitrary byte offset cannot be mapped
directly to a destination-row strip. The decoder therefore begins serially,
establishes raster bounds and initial palette state, and passes the remaining
payload to the parallel decoder as byte spans.

The default span lengths are nearly equal. `SIXEL_PARALLEL_SKEW` can bias the
distribution by up to 20 percent so later workers receive systematically more
or less input. Thread count is capped by payload length; a payload cannot have
more non-empty initial spans than bytes.

## Normal direct path

The normal path is a two-pass design rather than a single parallel paint:

```text
serial parser reaches parallel anchor
                 |
                 v
       split payload into N byte spans
                 |
                 v
       parallel validation/state scan
                 |
             join all N
                 |
                 v
   paint span 0, span 1, ... span N-1
        one joined worker at a time
                 |
                 v
       continue serial parser/output
```

Each scan reconstructs the local SIXEL state needed at its useful boundary.
Byte spans can nevertheless target overlapping destination pixels. Painting
all of them concurrently into the shared image would introduce data races and
could violate parser order, so the direct paint pass starts and joins one span
at a time.

The checked-in eight-worker timeline makes the distinction visible. Eight
scan intervals begin together. The eight paint intervals then form a
staircase, with no overlap. PNG output follows and dominates this small test's
remaining wall time; it is not part of the decoder worker budget.

![Eight-worker decoder timeline](measurements/decoder-thread8-timeline.png)

Calling this only “band decoding” hides two facts: the initial work units are
encoded byte spans, and the ordinary destination paint is currently ordered
rather than parallel.

## Local-buffer and undither path

When decoder-side undithering is prepared, workers can decode into local row
chunks and publish finished rows through the ordered coordination path. Local
buffers reduce direct write conflicts and permit more work to remain inside
the parallel worker lifetime. This path has different allocation and copying
costs from direct paint and must be measured separately.

Do not infer behavior for decoder undithering from the direct-path chart. A
timeline should identify scan, decode, paint, local-copy, and output stages
explicitly whenever those paths are compared.

## Fallback and correctness

Parallel decoding begins only after the serial parser has established an
anchor. A worker can still encounter syntax that needs global parser context,
including raster resets or palette redefinitions. Such a stream may fall back
to serial handling. This is an intended correctness boundary, not proof that
the configured thread option was ignored.

Parallel work must preserve:

- control-sequence and palette semantics at each span boundary;
- parser-order writes when spans address the same destination region;
- raster clipping, paint-mask, and OR-mode behavior;
- identical malformed-input and allocation-failure handling; and
- the public decoder's output ownership contract.

The timeline instrumentation records paired starts and finishes for both scan
and paint. `tests/cli/sixel2png/0011_parallel_timeline_spans_paired.t` keeps
that diagnostic contract from silently degrading back to zero-length markers.

## Measurement limits

The checked-in chart is an architectural observation from one generated
SIXEL input. It is not a decoder scaling curve. A performance study should
vary payload size, SIXEL command mix, destination dimensions, palette changes,
OR mode, output format, and thread count. Measure parser/paint time separately
from PNG serialization when the question is decoder parallelism.
