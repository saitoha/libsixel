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
       derive each span's painted rows
                 |
     overlap? ----+---- yes -> serial fallback
                 |
                 no
                 v
       create all N paint workers
                 |
       all-ready release barrier
                 |
                 v
          parallel paint + join
                 |
                 v
       continue serial parser/output
```

Each scan reconstructs the local SIXEL state needed at its useful boundary.
It also records the first and last row that the corresponding paint pass would
touch. The controller releases all paint workers together only when those row
ranges are ordered and disjoint. The shared pixel and paint-mask buffers can
then be updated without locks because no two workers own the same row.

An arbitrary byte split is advanced to the next SIXEL `-` boundary. This
normally aligns useful work to complete six-row bands; it does not create an
automatic five-row overlap. Several short byte spans can, however, converge on
the same later boundary and describe overlapping rows. The current
implementation detects that case after scan and falls back before paint rather
than attempting an unsafe parallel write.

All paint workers wait at a release barrier. If any worker cannot be created,
the controller aborts and joins the workers that already exist while they are
still blocked, leaving the serial fallback image untouched.

The checked-in eight-worker timeline uses a 1920 by 1080 raster. Eight scan
intervals begin together. All eight paint workers then become ready at a
barrier before their intervals begin as a parallel block. PNG output follows
and dominates the remaining process wall time; it is not part of the decoder
worker budget. Scan is shown in light blue and paint in dark blue.

![Eight-worker Full HD decoder timeline](measurements/decoder-thread8-timeline.png)

## Raster-size comparison

The same source and encoder policy were used to create two dedicated decoder
fixtures. These are single diagnostic observations, not repeated performance
benchmarks:

| Raster | SIXEL bytes | Scan wall | Parallel paint wall | Decoder wall | PNG wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| 900 x 675 | 334,858 | 1.249 ms | 1.188 ms | 2.715 ms | 18.507 ms |
| 1920 x 1080 | 1,114,189 | 3.379 ms | 3.189 ms | 6.915 ms | 68.783 ms |

Increasing the raster from 607,500 to 2,073,600 pixels multiplied the pixel
count by 3.41, scan wall time by approximately 2.71, parallel-paint wall time
by approximately 2.68, and the complete decoder interval by approximately
2.55 in this run. Scan and paint are now similar parts of direct decoding:
at Full HD they occupy 48.9 and 46.1 percent of the decoder interval,
respectively. PNG serialization is an order of magnitude longer than either
decoder phase in both observations.

The percentages use the complete decoder interval as their denominator. Scan,
paint, and controller gaps do not form a perfectly additive partition, and PNG
serialization is deliberately excluded. The retained
[`decoder-size-comparison.csv`](measurements/decoder-size-comparison.csv) and
both raw timelines allow this observation to be recalculated.

Calling this only “band decoding” hides two facts: the initial work units are
encoded byte spans, and destination paint becomes row-parallel only after the
validation pass has proved independence.

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
- disjoint row ownership, or clean fallback before shared-buffer writes;
- raster clipping, paint-mask, and OR-mode behavior;
- identical malformed-input and allocation-failure handling; and
- the public decoder's output ownership contract.

The timeline instrumentation records paired starts and finishes for both scan
and paint. `tests/cli/sixel2png/0011_parallel_timeline_spans_paired.t` keeps
that diagnostic contract from silently degrading back to zero-length markers.
`0012_parallel_paint_barrier.t` verifies that every paint worker reaches the
barrier before paint begins. Decoder tests 0025 and 0026 cover partial worker
creation and overlapping row ranges, including the clean-fallback guarantee.

## Measurement limits

The checked-in chart and two-size comparison are architectural observations
from one generated image class. They are not a decoder scaling curve. A
performance study should use repeated samples and vary payload size, SIXEL
command mix, destination dimensions, palette changes, OR mode, output format,
and thread count. Measure parser/paint time separately from PNG serialization
when the question is decoder parallelism.
