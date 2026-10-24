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

`--threading-policy=balanced:global_budget=N` supplies the worker budget for the current operation. The base selects how that budget is spent on still images and animations: `lazy` overlaps independent branches and pipelines, with one default dither band and serial colorspace, sampling, scaling, FHEDT construction and SIXEL parsing; `balanced` retains size heuristics and caps the effective budget to performance logical CPUs on hybrid hardware; `greedy` uses the full budget and makes `colorspace_min`, `sample_min` and `scale_min_bytes` zero even when `-j` explicitly sets them. The default base is `balanced`. Explicit stage keys override mode defaults and shrink to fit the budget; lazy SIXEL parsing remains serial.

`global_budget` (`B`) and `sixel_set_threads()` take precedence over `SIXEL_THREADING_GLOBAL_BUDGET`. An explicitly selected CLI base or `SIXEL_THREADING_POLICY=lazy|balanced|greedy` defaults to hardware concurrency. Without an explicit budget or mode, library processing retains two slots and direct SIXEL parsing one. The converters supply `balanced` when the base environment is empty or unset, so their default uses hardware concurrency subject to the performance-core cap. On macOS the cap uses `hw.perflevel0.logicalcpu` only when `hw.nperflevels > 1`; on Linux it counts `/sys/devices/cpu_core/cpus` ranges. Missing or invalid topology falls back to the logical count.

The budget bounds planned work slots rather than all operating-system threads in the process. Depending on the path, the process can also contain:

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

Work-band height is at least six and is rounded upward to a multiple of six. That alignment lets the palette-application producer release complete SIXEL bands without requiring the encoder to merge partial six-row states. Queue depth starts at three times the initial encode reservation, capped by the number of SIXEL bands. The [dither/encode budget policy](encoder.md#ditherencode-budget-policy) defines how the shared allocator assigns successive full-budget phases to spatial methods at budgets two through four and retains overlapping phases at higher budgets and distinguishes an early estimate from the resolved runtime allocation.

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

The [allocator geometry policy](encoder.md#ditherencode-budget-policy) chooses overlap from the effective method and completed palette. Spatial diffusion uses a low-color warm-up; stateless methods need none. Explicit diffusion suboptions can override work-band height and overlap. The overlap suboption is spelled `band_overwrap`; retain that spelling when reproducing a configuration.

Consequently, a parallel error-diffusion encode can differ slightly from a
one-worker encode even when palette construction, lookup, and all other
options are fixed. The difference may be measurable without being visually
harmful, and a global average can hide a faint horizontal seam. Review both
spatial metrics and seam-local error maps. Positional methods and `-d none` do
not carry the same mutable cross-row residual, so they are less sensitive to
this particular boundary mechanism.

A policy must explicitly advertise work-band support; the pipeline cannot
assume that every dither method is safe to split.

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
           derive destination row ownership
                         |
                all-ready paint barrier
                         |
                         v
                parallel image paint
```

Spans may overlap in the destination even when their input bytes do not. The
normal direct path therefore performs a parallel validation scan, records each
span's actual destination-row range, and releases parallel paint only when the
ranges are disjoint. Overlapping ranges fall back before any paint worker can
modify the image. Some streams also require a serial fallback when a worker
encounters a token needing global parser context, such as a raster reset or
palette redefinition.

## Ordering is part of correctness

Parallel stages may compute independent fragments out of order, but externally
visible state is committed in a defined order:

- encoder SIXEL fragments are written by increasing band index;
- direct decoder paint spans own disjoint rows, otherwise they fall back before
  publication;
- animation frames are encoded in frame order; and
- retained delta history crosses frame boundaries only in that order.

Performance work must not weaken these commit rules. If a new optimization
changes the unit of work, document both its independence proof and its ordered
publication boundary.

## Owning implementation

- `src/planner.c` owns the shared allocation calculation and the early estimate of the encoder budget split and work-band geometry.
- `src/dither.c` applies palettes to independent work bands and discards
  warm-up output.
- `src/encoder-core-encode.c` pipelines palette application, six-row encode
  jobs, and the ordered writer.
- `src/decoder-parallel.c` divides the byte stream and coordinates scan,
  decode, paint, and fallback behavior.
- `src/encoder.c` owns loader-to-encoder frame handoff and frame ordering.
- `include/6cells.idl` defines the independent thread-pool component boundary.

The stage-specific documents describe these paths in more detail.
