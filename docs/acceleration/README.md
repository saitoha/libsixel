# Acceleration backends

This page is for contributors who add or port a SIMD or GPU implementation. It explains how libsixel keeps track of which backends implement each accelerated operation, how source code is tagged so that the record can be checked, and what the static check rejects. The record itself is the [acceleration ledger](ledger.md). How the CPU and the GPU share memory is explained in [GPU host memory and frame sharing](../gpu/host-memory.md), and how SIMD levels are selected at run time in [Runtime execution policy](../cli/runtime-policy.md).

**Key ideas in one minute:**

- An accelerated operation usually needs more than one backend: NEON, SSE2, AVX2, WASM SIMD, Metal and Vulkan.
- The ledger has one row per operation and one column per backend, so a missing port stays visible.
- Every SIMD function and GPU kernel carries an `Accel:` tag that names its ledger row.
- `make staticcheck` rejects untagged acceleration and any disagreement between tags and the ledger.

## Why a ledger

An optimization is usually written and measured on one machine. It then exists for one backend only: a NEON loop with no SSE2 counterpart, or a Metal kernel with no Vulkan program. Nothing in the build or the tests reports the gap, because the scalar path keeps every other backend correct. The work is easy to forget after the change is merged.

The ledger turns these gaps into data. It lists every operation with its state on each backend. The static check keeps that list honest: a backend can be marked as implemented only when source code tagged with that backend exists, and tagged source code must appear in the list.

## Backends and states

The ledger has one column for the scalar reference and one for each acceleration backend:

| Column | Source it covers |
| --- | --- |
| Scalar | The portable C path that defines the result |
| NEON | Arm NEON intrinsics and vector types |
| SSE2 | x86 128-bit intrinsics, including later 128-bit extensions |
| AVX2 | x86 256-bit intrinsics; a 128-bit helper inside an AVX2 function counts as AVX2 |
| WASM | WebAssembly SIMD128 |
| Metal | Metal kernels |
| Vulkan | GLSL programs compiled for Vulkan |

Each cell holds one of four states:

| State | Meaning | Tag in source |
| --- | --- | --- |
| `done` | The backend implements the operation | Required for every column except Scalar |
| `planned` | A port is expected but not written | Not allowed |
| `n/a` | The backend does not apply; the Notes column gives the reason when it is not obvious | Not allowed |
| `rejected` | A port was written and measured and did not pay off; the Notes column records the result | Not allowed |

The Scalar column is `done` for every operation whose accelerated paths reproduce a scalar result. Scalar code has no detectable pattern, so the check does not require a tag for it; a tag with the `scalar` backend is accepted for a branch-free or otherwise optimized scalar path that should be listed.

## Accel tags

A tag is a comment of the form:

```c
/* Accel: SAMPLE-VARY-POINTS neon sse2 */
```

The first word is the ledger ID: capital letters, digits and hyphens, starting with a letter. The remaining words are backends from `scalar`, `neon`, `sse2`, `avx2`, `wasm`, `metal` and `vulkan`. Write the tag directly above the function, kernel or macro it describes, after the explanatory comment and before the signature. A function that holds both a NEON and an SSE2 branch names both backends. A GLSL program carries its tag after the license line, before `#version`. A Metal source embedded in an Objective-C string carries its tag above the string's declaration.

## What counts as an acceleration unit

The check scans the C, Objective-C, Metal and GLSL sources directly under `src/`. It divides C, Objective-C and Metal sources into top-level constructs: a function, a declaration, or a `#define` with its continuation lines. The comments and preprocessor lines before a construct belong to it, which is why a tag directly above a function counts as part of that function. A unit must carry a tag when it matches one of the following:

| Backend | What marks a unit |
| --- | --- |
| NEON | A NEON vector type such as `uint8x16_t`, or a NEON intrinsic call such as `vld1q_u8()` |
| SSE2 | `__m128` types or `_mm_` intrinsics |
| AVX2 | `__m256` types or `_mm256_` intrinsics |
| WASM | `v128_t` or `wasm_` intrinsics |
| Metal | A `kernel` function in a `.metal` file, or `kernel void` inside a string in an Objective-C or C file |
| Vulkan | Every `.glsl` file, as one unit |

The bundled stb image decoder and writer keep their upstream SIMD paths and are not scanned. Host code that drives a GPU, such as pipeline setup in `*-metal.m` or `*-vulkan.c`, is not a unit by itself; the kernel or program it runs is.

## Untagged legacy code

Code written before the ledger existed is listed in [`tests/_static/data/accel-untagged-legacy.tsv`](../../tests/_static/data/accel-untagged-legacy.tsv). Each row names a source file, the number of untagged units it may still contain, and the unit's name. The list can only shrink. Tagging a listed unit without removing its row fails the check, so the list always shows exactly what remains. A new untagged unit is never added to it; it is tagged and given a ledger row instead.

## Adding or porting an optimization

1. Find the operation in the [ledger](ledger.md), or add a row with a new ID when the operation is new. Mark every backend whose port is expected as `planned`.
2. Tag the new function or kernel and change its cell to `done` in the same change.
3. When a port is measured and dropped, change the cell to `rejected` and record the result in Notes.
4. Run `make staticcheck`.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| AC-01 | Every SIMD and GPU unit under `src/` carries an `Accel:` tag, except the units frozen in the legacy list; the legacy list matches the untagged units exactly; every tag names a ledger row whose cell for that backend is `done`; and every `done` cell outside Scalar has a tag. | [tests/_static/sh/staticcheck-accel-ledger.sh](../../tests/_static/sh/staticcheck-accel-ledger.sh) |
| AC-02 | The check rejects an untagged unit, a tag whose backend the unit does not use, a tag on a cell that is not `done`, an unknown ID or state, a malformed tag, a `done` cell without a tag, and a legacy row that no longer matches; it accepts AVX2 functions with 128-bit helpers, prose that names a vector type, and the vendored stb decoder. | [tests/_static/sh/staticcheck-accel-ledger-controls.sh](../../tests/_static/sh/staticcheck-accel-ledger-controls.sh) |
| AC-03 | No function compiled with an AVX2 target attribute under `src/` takes a 256-bit or 512-bit vector parameter by value, since 32-bit x86 GCC and Cygwin/MSYS x86-64 GCC load such an argument from a stack slot without 32-byte alignment; the check rejects a by-value control and accepts the pointer form. | [tests/_static/sh/staticcheck-avx-vector-params.sh](../../tests/_static/sh/staticcheck-avx-vector-params.sh) |

### Coverage boundary

The check recognizes acceleration by vector types, intrinsic names and kernel declarations. It does not detect a scalar optimization, a SIMD path reached only through a macro defined in another file, or GPU work written in a language it does not scan. It does not decide whether a `planned`, `n/a` or `rejected` state is correct, and it does not verify that a tagged implementation produces the scalar result; the behavioral tests of each operation own that.
