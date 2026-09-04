# Palette Construction Pipeline Architecture

## Purpose

Palette construction is a sequence of distinct decisions. Sampling chooses
which source pixels represent the image, binning converts those samples into a
weighted color population, and quantization selects palette entries from that
population. These decisions must remain independently selectable even when the
runtime fuses their implementations.

This document defines the target architecture and the migration invariants for
introducing independent sampling and binning policies. It complements
[Palette Quantization](quantization.md), which explains the mathematical
objectives of the palette solvers.

## Logical pipeline

The fixed-palette branch has the following logical data flow:

```text
loaded frame
    |
    v
sampling
    |
    v
sample point stream
    |
    v
palette-space transform (-X)
    |
    v
binning
    |
    v
weighted point set
    |
    v
quantization (-Q)
    |
    v
palette finalization -> lookup preparation -> palette application
```

The stages have separate responsibilities:

- sampling chooses which spatial observations enter palette construction;
- the palette-space transform defines the coordinates in which colors are
  clustered;
- binning aggregates sample mass in that color space;
- quantization selects at most `K` representatives from the resulting point
  set.

The normal image-processing branch and palette branch join only after the
palette and its lookup preparation are ready. A supplied-palette path may skip
sampling, binning, and quantization completely.

## Sampling policy

Sampling controls extraction from the source image. Its policy includes the
selection method, target population, deterministic seed where applicable, and
small-region preservation. It does not choose histogram precision or a
palette solver.

The policy is expected to grow around a structure such as:

```text
method = auto | full | grid | stratified | random
target = COUNT
solid = on | off
seed = INTEGER
```

The current adaptive grid and solid-region augmentation behavior must be
representable without changing output during migration. Solid-region
augmentation currently repeats selected colors to increase their influence.
The target point-stream contract should express that influence as an explicit
base weight and retain its origin:

```text
sample point:
    color
    base weight
    provenance = grid | solid | reserved
```

An explicit weight prevents a later binning implementation from accidentally
discarding the protection provided to a small solid-colored feature.

## Binning policy

Binning consumes points after their palette-space coordinates are known. It
controls how sample mass is aggregated, independently of the quantizer that
will consume the result.

The policy is expected to distinguish:

```text
mode = auto | none | exact | hard | soft
bits = INTEGER
kernel = trilinear
grid-map = POLICY
```

The modes have these intended meanings:

- `none` preserves the individual sample sequence;
- `exact` combines only identical color coordinates;
- `hard` assigns all of a sample's mass to one finite grid cell;
- `soft` distributes the mass among neighboring cells according to a kernel.

The distinction between `none` and `exact` matters. Combining duplicate points
preserves many weighted objectives, but it may change initialization order,
candidate identity, or medoid selection. It must not be described as a
universally invisible optimization.

Dense and compact-sparse storage are implementation backends, not binning
policies. The planner may choose between them from the finite bin domain,
estimated occupied cells, and memory budget. They should appear in diagnostics
but should not become stable user-facing semantics.

## Progressive automatic resolution

Sampling and binning are independent explicit policies, but their `auto`
values are resolved only when the next stage needs a concrete value. The
quantizer declares capabilities and preferences; it does not implement
sampling or binning itself.

```text
frame metadata -> resolve sampling -> sampling
                                      |
                                      v
                 sample metadata -> resolve binning -> binning
                                                        |
                                                        v
                         weighted-point metadata -> quantization
```

An unresolved value remains part of the per-frame plan until its execution
boundary is reached. Resolution follows these rules:

| Sampling | Binning | Resolver action |
| --- | --- | --- |
| `auto` | `auto` | Resolve sampling first, then resolve binning from the resulting sample metadata. |
| explicit | `auto` | Choose binning compatible with the sampling result and quantizer. |
| `auto` | explicit | Resolve sampling without replacing the explicit binning policy. |
| explicit | explicit | Validate the combination without replacing it. |

When `-Qauto` is also requested, the planner retains a quantizer candidate set.
If binning needs a concrete consumer capability, the quantizer family is
resolved at that boundary rather than at the beginning of the frame. Remaining
quantizer subpolicies may stay unresolved until the weighted point set exists.
Candidate selection may use palette size, frame dimensions, colorspace,
quality profile, sample count, and memory budget. It must be deterministic for
the same inputs and settings.

Each quantizer must declare whether it accepts raw samples, weighted points,
fractional weights, observed-color representatives, and additional moments.
An unsupported explicit combination is an error. Only an `auto` input may be
replaced during planning. Each effective choice is fixed before its owning
stage executes and is never reopened within that frame. Allocation failure
during execution must not silently select a different algorithm.

Soft binning can contribute one sample to as many as eight cells for a
trilinear three-dimensional kernel. Sparse capacity estimates must be bounded
by both the contribution count and the finite grid domain:

```text
maximum occupied entries =
    min(sample count * maximum contributions per sample,
        addressable bin count)
```

The effective plan must be visible in verbose diagnostics and measurement
metadata, including the requested values, resolution phase, resolved policies,
storage backend, estimated allocation, and resolution reason. Request origin
and lifecycle are separate: an `auto` request remains identified as `auto`
after an effective value has been selected.

## Filter and artifact boundaries

Logical stages are filters. Orchestration code creates concrete filters through
the filter factory and executes them through `sixel_filter_run()`. It must not
call a concrete filter's initializer, apply function, or whole-frame execution
helper directly. Concrete execution entry points remain private to the owning
filter implementation.

The current filter I/O contract carries frame slots. The target palette branch
requires typed artifacts so that a point stream or weighted point set is not
disguised as an image frame:

```text
FRAME -> SAMPLE_STREAM -> WEIGHTED_POINT_SET -> PALETTE -> LOOKUP
```

Algorithmic services may be shared below a filter, such as solid-color
detection or lookup construction. Such services must not duplicate the
filter's complete execution contract or provide an alternate orchestration
path.

The repository static suite enforces the current dispatch boundary. Concrete
filter headers may not expose `*_apply()` or `*_frame()` execution helpers,
concrete constructors may only be called by their owning implementation and
the filter factory, and execution calls may only occur inside the owning
filter. This source check is especially important for the amalgamated build,
where translation-unit visibility can allow a private-looking call to compile.

This component-ownership check applies to the library implementation under
`src/`. Programs under `converters/` are public-API consumers rather than
filter-component implementations. A separate private-include check prevents
those programs from depending on `src/*.h`; they are not treated as additional
filter owners.

## Logical separation and physical fusion

Separate filters do not require an intermediate allocation after every stage.
The planner may select one of the following physical forms while preserving the
same logical DAG and diagnostics:

```text
materialized:
    sampling -> sample buffer -> transform -> point buffer -> binning

streamed:
    sampling -> batches -> transform -> binning

fused:
    one execution operator implements sampling + transform + binning
```

Batch streaming is preferable to a virtual call per point. Fusion is an
internal optimization, not a CLI policy. It must be benchmarked because a
materialized contiguous buffer can be better for SIMD or GPU conversion, while
direct histogram updates can avoid allocation and memory traffic.

## Migration waves

The migration is intentionally divided so structural changes, CLI changes, and
default changes can be reviewed independently.

1. Enforce filter dispatch boundaries. Route existing sampling and final-merge
   execution through the factory and vtable, make whole-filter helpers private,
   and add a static boundary check.
2. Add requested, effective, reason, and lifecycle state to the existing
   per-frame encode DAG context. Remove unused policy copies from the palette
   worker job without changing output.
3. Represent current target-derived grid sampling as an explicit internal
   policy and record minimal sample metadata while retaining the current frame
   payload.
4. Define orthogonal binning semantics and the weighted-point-set artifact
   contract before moving an implementation.
5. Extract current K-means hard and soft histogram construction into an
   independent binning filter. Pass migrated binning settings explicitly while
   preserving current point order and output.
6. Declare quantizer capabilities and introduce stage-specific pure resolvers.
   Resolve sampling before sampling and binning from actual sample metadata,
   without silent execution-time fallback.
7. Add top-level sampling and binning policy options. Keep existing `-Q`
   suboptions as deprecated aliases and reject conflicting explicit values.
8. Move other quantizer-specific histogram construction to the shared binning
   stage only where semantics match, and add further explicit policies such as
   exact aggregation.
9. Measure the sampling, binning, and quantization axes independently. Use the
   results to select automatic profiles and only then change defaults.
10. Add chunk streaming, operator fusion, or storage optimizations where
    measurement shows a benefit, while keeping logical filters, resolution
    records, and trace spans observable.

Each wave must leave the normal and amalgamated builds consistent. Tests must
cover direct policy parsing, effective-plan resolution, unsupported
combinations, compatibility aliases, output quality, allocation bounds, and
the filter boundary itself.
