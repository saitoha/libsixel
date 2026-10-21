# Lookup policy: `auto`

This page describes the `auto` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`auto` asks the lookup-policy selector to choose a concrete backend. It does not define a distance, index, or complexity class of its own. The selected backend owns all of those properties.

The built-in encoder default is `auto`, so omitting `--lookup-policy` and passing `--lookup-policy=auto` select the same backend. The only difference is the explicit-policy flag: an explicit value also enables lookup for supplied palettes, as described above.

## Method

### Dispatch flow

The current selector behaves as follows:

```text
completed palette and pixel format
              |
              v
 explicit concrete policy? -- yes --> named policy
              |
              no
              v
 canonical two-color black/white palette? -- yes --> internal mono policy
              |
              no
              v
 lookup disabled, depth != 3, or explicit none? -- yes --> none
              |
              no
              v
 auto? -- yes --> vptree (same exact distance and tie rule)
              |                  |
              |                  +-- none --> ordered bulk rows
              |                  +-- diffusion --> hinted point queries
              |                  +-- other --> existing point queries
              |
              no
              v
 recognized concrete policy? -- yes --> that 8-bit or float32 backend
              |
              no
              v
             6bit
```

`auto` selects the exact [`vptree`](vptree.md) backend and chooses the invocation strategy from the resolved dither method. `none` uses ordered bulk rows when [its fences permit](../dithering/README.md#none-nearest-color-without-error-shaping); error diffusion retains causal hinted point queries, and other methods retain their existing point queries. Byte bulk mapping reuses consecutive identical colors and the existing atomic 24-bit exact-key memo. When NEON or SSE2 and its compiler intrinsics header are available, eight cold queries are mapped together by exhaustive SIMD distance evaluation. CPU capability alone is insufficient for either byte or float SIMD: both the header include and the kernel require the corresponding configure or Meson header probe. A short tail and builds without usable intrinsics use the exact scalar search. Palette-order strict comparisons retain the lowest-index tie rule. No approximate backend is eligible for `auto`; only an unrecognized policy value reaches the historical `6bit` fallback.

Both byte and float bulk mapping preserve scan order. Float rows reuse complete native coordinates in caller-owned memo tables and evaluate cold batches with double SIMD distances; neither keys nor palettes are narrowed to bytes. Explicit `vptree` uses the same bulk operation under `none`, while every other explicit policy retains its own mapping implementation. Image size and worker count do not change the selected metric or tie rule. The timeline records the resolved class as an event under role `lookup`, with worker `batch` or `point`; `none-batch` separately records actual fast-loop entry. A selected bulk strategy can still fall back to point mapping when transparency, retained pixels or output clipping require it.

The selected extended float path bypasses the byte black/white shortcut: its byte projection cannot establish equality of native float coordinates. It enables the requested lookup even for supplied palettes. Thus `auto`, explicit or default, always resolves to `vptree.float32` on that path.

### Bulk mapping contract

`ILookupPolicy.map_pixels(pixels, count, pixel_stride, result, result_stride, workspace)` returns the same ordered indices as repeated `map_pixel` calls on an identically prepared policy. Input stride is in bytes; output stride is in indices. Signed strides allow a reversed row without changing approximate cache insertion order. Every addressed sample and output must be valid, input and output buffers must not overlap, and zero count touches neither buffer. Implementations without a bulk kernel use the common scalar fallback. A bulk call keeps no persistent scan history in the policy and follows the prepared policy's existing concurrency rules. The optional `workspace_size` operation reports the size of caller-owned scratch: allocate zeroed, malloc-aligned bytes, keep them private to one caller and one unchanged prepared policy, then free them directly. Reset scratch after preparation changes. A NULL operation or zero size requires no scratch; NULL scratch remains valid after allocation failure. The byte vptree shared memo publishes complete exact-key entries atomically, while float vptree scratch is private to each dither band and persists across its rows. This contract does not strengthen approximate policies into order-independent nearest-neighbor searches.

## Guarantee

`auto` inherits the selected backend's result guarantee.

## Cost

`auto` inherits the selected backend's preparation cost, query cost, and storage. Its own dispatch is constant work.

Benchmarks must report the resolved class name rather than only saying "auto." Otherwise a selector change can make two results incomparable without changing the benchmark command.

## Name and sources

`auto` is a libsixel descriptive name, not the name of a published search algorithm. It means deferred policy selection.

The selection rules are implemented in [`lookup-policy.c`](../../../../src/lookup-policy.c). The public values are registered in [`options-registry.c`](../../../../src/options-registry.c).
