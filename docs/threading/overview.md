# Threading Overview

## Why the vocabulary matters

The word *band* describes three different units in discussions of SIXEL
threading. They are related, but they are not interchangeable.

| Unit | Boundary | Purpose |
| --- | --- | --- |
| SIXEL band | Six image rows | Natural unit encoded by one SIXEL body segment |
| Palette-application work band | A horizontal strip, rounded to a multiple of six rows | Give independent workers useful dither and lookup work |
| Decoder byte span | A contiguous interval in the encoded input | Divide parser work before its actual image-row extent is known |

This documentation uses *SIXEL band*, *work band*, and *byte span* explicitly.
The informal phrase *band decoding* refers to the last row above in the current
decoder: it does not mean that the decoder initially divides the destination
image into horizontal strips.

## Worker budget is not a process thread limit

`--threads=N` supplies a worker budget to the current operation. It is not a
strict upper bound on all operating-system threads in the process. Depending
on the path, the process can also contain:

- the caller or controller thread;
- an encoder writer thread that commits finished SIXEL bands in order;
- a loader/encoder handoff thread for animation;
- a temporarily overlapping palette-build worker; and
- worker pools created independently by other components.

The thread-pool component deliberately owns one short-lived queue and forbids
global queue or process-budget state. This keeps components independent, but
it also means that current code does not coordinate all concurrently live
pools through one process-wide scheduler.

When documenting or measuring a configuration, distinguish these quantities:

1. configured worker budget;
2. planned workers per stage during an overlap window;
3. pool capacity after a stage finishes;
4. distinct workers that actually execute at least one job; and
5. total operating-system threads, including control and writer threads.

They need not have the same value.

## Band encoding

The fixed-palette encoder first maps source pixels to palette indexes. It then
encodes each six-row SIXEL band independently and lets an ordered writer append
the finished byte fragments.

```text
source pixels
     |
     v
+---------------- palette application ----------------+
| horizontal work band 0 | work band 1 | work band 2  |
+-------------------------+-------------+--------------+
     | completed index rows
     v
[SIXEL rows 0..5] [SIXEL rows 6..11] [SIXEL rows 12..17] ...
       encode jobs may finish out of order
     v
             ordered writer -> SIXEL byte stream
```

Work-band height is at least six and is rounded upward to a multiple of six.
That alignment lets the palette-application producer release complete SIXEL
bands without requiring the encoder to merge partial six-row states. Queue
depth normally starts at three times the configured worker budget and is
capped by the number of SIXEL bands.

The encode jobs do not alter pixels. Given the same indexed image and palette,
dividing the SIXEL byte generation into six-row jobs does not by itself reduce
visual quality. Any visual difference attributed broadly to “band encoding”
must first be localized: in the normal pipeline it is usually caused by how
the preceding palette-application work bands handle dither history, not by the
ordered SIXEL writer.

## Work-band seams and image quality

Error diffusion is causal. A sequential raster scan carries an accumulated
error state from every earlier row into the next row. Independently processed
work bands cannot cheaply import that complete state.

libsixel can prepend overlap rows to a work band:

```text
previous output band       next output band
----------------------|----------------------------
                      ^ y0
             <-------->
              overlap rows recomputed as warm-up
             [discarded] [committed rows from y0...]
```

The worker starts at `y0 - overlap`, recomputes those source rows to warm its
local error state, suppresses their output, and commits only rows beginning at
`y0`. This reduces a hard reset at the seam, but it is an approximation: the
warm-up begins without all history that reached `y0 - overlap` in a full-frame
scan. With zero overlap, every work band starts with a fresh local diffusion
state.

The current automatic overlap is six rows when the requested palette has at
most 32 colors and zero rows above 32 colors. Explicit diffusion suboptions
can override work-band height and overlap. The implementation currently names
the overlap suboption `band_overwrap`; retain that spelling when reproducing a
configuration.

Consequently, a parallel error-diffusion encode can differ slightly from a
one-worker encode even when palette construction, lookup, and all other
options are fixed. The difference may be measurable without being visually
harmful, and a global average can hide a faint horizontal seam. Review both
spatial metrics and seam-local error maps. Positional methods and `-d none` do
not carry the same mutable cross-row residual, so they are less sensitive to
this particular boundary mechanism.

Inter-frame diffusion has an additional temporal dependency and reports that
parallel work bands are unsupported. A policy must explicitly advertise work-
band support; the pipeline cannot assume that every dither method is safe to
split.

See [Measurement and reproduction](measurement.md#band-seam-quality-protocol)
for the controlled comparison required after changing band geometry, overlap,
scan order, or error-state handling.

## Band decoding

The decoder cannot initially split work by destination rows because row
positions are encoded inside the stream. After the serial parser establishes
raster bounds and a palette anchor, the parallel decoder divides the remaining
input into nearly equal byte spans. Each worker reconstructs enough parser
state to align useful work with SIXEL line boundaries.

```text
serial parser -> raster/palette anchor
                         |
encoded payload: [ byte span 0 ][ byte span 1 ][ byte span 2 ] ...
                         |
                         v
               decode or validation workers
                         |
                         v
              ordered publication to the image
```

Spans may overlap in the destination even when their input bytes do not. The
normal direct path therefore performs a parallel validation scan and then
paints spans in parser order. Some streams also require a serial fallback when
a worker encounters a token needing global parser context, such as a raster
reset or palette redefinition.

## Ordering is part of correctness

Parallel stages may compute independent fragments out of order, but externally
visible state is committed in a defined order:

- encoder SIXEL fragments are written by increasing band index;
- direct decoder paint spans preserve parser order;
- animation frames are encoded in frame order; and
- temporal dither state crosses frame boundaries only in that order.

Performance work must not weaken these commit rules. If a new optimization
changes the unit of work, document both its independence proof and its ordered
publication boundary.

## Owning implementation

- `src/planner.c` predicts the encoder budget split and work-band geometry.
- `src/dither.c` applies palettes to independent work bands and discards
  warm-up output.
- `src/encoder-core-encode.c` pipelines palette application, six-row encode
  jobs, and the ordered writer.
- `src/decoder-parallel.c` divides the byte stream and coordinates scan,
  decode, paint, and fallback behavior.
- `src/encoder.c` owns loader-to-encoder frame handoff and frame ordering.
- `include/6cells.idl` defines the independent thread-pool component boundary.

The stage-specific documents describe these paths in more detail.
