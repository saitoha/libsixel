# Acceleration backends

This page is for contributors who add or port a SIMD or GPU implementation. It explains how libsixel keeps track of which backends implement each accelerated operation, how source code is tagged so that the record can be checked, and what the static check rejects. The record itself is the [acceleration ledger](ledger.md). How the CPU and the GPU share memory is explained in [GPU host memory and frame sharing](../gpu/host-memory.md), and how SIMD levels are selected at run time in [Runtime execution policy](../cli/runtime-policy.md).

**Key ideas in one minute:**

- An accelerated operation usually needs more than one backend: NEON, SSE2, AVX2, WASM SIMD, Metal, Vulkan and WebGPU.
- The ledger has a SIMD table and a GPU table, each with one row per operation and one column per backend, so a missing port stays visible.
- Every SIMD unit, GPU kernel and shader fragment carries an `Accel:` tag that names its ledger row.
- `make staticcheck` rejects untagged acceleration and any disagreement between tags and the ledger.

## Why a ledger

An optimization is usually written and measured on one machine. It then exists for one backend only: a NEON loop with no SSE2 counterpart, or a Metal kernel with no Vulkan program. Nothing in the build or the tests reports the gap, because the scalar path keeps every other backend correct. The work is easy to forget after the change is merged.

The ledger turns these gaps into data. It lists every operation with its state on each backend. The static check keeps that list honest: a backend can be marked as implemented only when source code tagged with that backend exists, and tagged source code must appear in the list.

## Backends and states

The ledger is split into two tables so that neither grows too wide to read. Both start with a column for the scalar reference; the SIMD table then has one column for each CPU vector backend, and the GPU table one for each GPU backend:

| Table | Column | Source it covers |
| --- | --- | --- |
| Both | Scalar | The portable C path that defines the result |
| SIMD | NEON | Arm NEON intrinsics and vector types |
| SIMD | SSE2 | x86 128-bit intrinsics, including later 128-bit extensions |
| SIMD | AVX2 | x86 256-bit AVX and AVX2 intrinsics. A 128-bit helper inside a 256-bit function counts toward that function’s backend. |
| SIMD | WASM | WebAssembly SIMD128 on the CPU |
| GPU | Metal | Metal kernels |
| GPU | Vulkan | GLSL programs compiled for Vulkan |
| GPU | WebGPU | WGSL programs for WebGPU, whatever host code dispatches them |

An operation has a row in each table that has a backend for it, under the same ID. An operation with both SIMD and GPU ports, such as `SAMPLE-VARY-POINTS`, therefore has two rows, and their Scalar cells must agree. A row whose backend cells are all `n/a` is rejected when the operation also has a row in the other table; an operation with no accelerated backend at all keeps a single SIMD row. Backends of the table an operation is missing from count as `n/a`.

The WebGPU column tracks ports for command-line use. An operation with Metal and Vulkan implementations records a missing WebGPU port as `planned`, including resident palette and body stages. The ledger records the WGSL programs. The WebGPU host code runs in thread-disabled Emscripten builds.

Each cell uses a state from the following table. The `8bit` state applies to GPU backend columns.

| State | Meaning | Tag in source |
| --- | --- | --- |
| `done` | Scalar or SIMD implements the operation. A GPU backend implements both the 8bit and FP32 data paths. | SIMD requires a backend tag. GPU requires both the backend tag and its `-fp32` form. |
| `8bit` | The GPU backend implements the 8bit data path. FP32 support is pending. | The backend tag without a suffix is required. |
| `planned` | A port is expected but not written | Not allowed |
| `n/a` | The backend does not apply; the Notes column gives the reason when it is not obvious | Not allowed |
| `rejected` | A port was written and measured and did not pay off; the Notes column records the result | Not allowed |

The Scalar column is `done` for every operation whose accelerated paths reproduce a scalar result. Scalar code has no detectable pattern, so the check does not require a tag for it; a tag with the `scalar` backend is accepted for a branch-free or otherwise optimized scalar path that should be listed.

An FP32 data path accepts the operation's FP32 data representation. FP32 arithmetic inside an 8bit data path still counts as `8bit`.

## Accel tags

A tag is a comment of the form:

```c
/* Accel: SAMPLE-VARY-POINTS neon sse2 */
```

The first word is the ledger ID: capital letters, digits and hyphens, starting with a letter. The remaining words are backends from `scalar`, `neon`, `sse2`, `avx2`, `wasm`, `metal`, `vulkan` and `webgpu`. Write the tag directly above the function, kernel or macro it describes, after the explanatory comment and before the signature. A function that holds both a NEON and an SSE2 branch names both backends. GLSL and WGSL files carry their tags near the start of the file. Standalone Metal fragments carry their tags before the first declaration or directive. Keep a GLSL prefix’s `#version` in its required leading position and use `//` comments for WGSL tags. An embedded shader carries its tag above the declaration of the host string.

Use `metal-fp32`, `vulkan-fp32` or `webgpu-fp32` on the GPU source that implements the FP32 data path. A GPU unit shared by both paths can carry both forms.

## What counts as an acceleration unit

The check scans maintained implementation sources under `src/`, `include/`, `converters/` and `assessment/`, including subdirectories. Top-level C, Objective-C, Objective-C++ and Metal constructs include functions, declarations and macros with their continuation lines. The comments and preprocessor lines before a construct belong to it. Place the tag before the declaration or signature so it covers the whole unit. A unit must carry a tag when it matches one of the following:

| Backend | What marks a unit |
| --- | --- |
| NEON | A NEON vector type such as `uint8x16_t`, or a NEON intrinsic name such as `vld1q_u8` |
| SSE2 | `__m128` types or `_mm_` intrinsics |
| AVX2 | `__m256` types or `_mm256_` intrinsics |
| WASM | `v128_t` or `wasm_` intrinsics |
| Metal | A `kernel` function, a `.metal` fragment without kernels, or an embedded Metal kernel declaration |
| Vulkan | A `.glsl` or `.comp` file as one unit, or embedded GLSL compute source |
| WebGPU | A `.wgsl` file as one unit, or embedded WGSL compute source |

The bundled stb image decoder and writer keep their upstream SIMD paths. The checker audits maintained shader sources before they are embedded into generated headers. A GPU pipeline is registered by tagging the kernel or program it runs.

## Untagged legacy code

The migration of legacy code is complete. The same tag and ledger checks apply to all maintained acceleration units.

## Adding or porting an optimization

1. Find the operation in the [ledger](ledger.md), or add a row with a new ID when the operation is new. Use the SIMD table for a CPU vector backend and the GPU table for a GPU backend, and add a row to the other table under the same ID when the operation gains a backend of that kind. Mark every backend whose port is expected as `planned`.
2. Tag the new function or kernel and update its cell in the same change. Use `8bit` for a GPU backend with only the 8bit path. Use `done` once both GPU paths are implemented and tagged.
3. When a port is measured and dropped, change the cell to `rejected` and record the result in Notes.
4. Run `make staticcheck`.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| AC-01 | Every detected SIMD or GPU unit carries a tag consistent with its ledger cell. SIMD `done` and GPU `8bit` require a matching backend tag. GPU `done` requires tags for both paths. | [tests/_static/sh/staticcheck-accel-ledger.sh](../../tests/_static/sh/staticcheck-accel-ledger.sh) |
| AC-02 | The checker rejects missing, misplaced, malformed and mismatched tags, unsupported vector constructs, and inconsistent ledger rows and cells. Controls cover GPU precision states, nested sources, shader fragments, embedded shaders, and scalar and AVX helper tags. | [tests/_static/sh/staticcheck-accel-ledger-controls.sh](../../tests/_static/sh/staticcheck-accel-ledger-controls.sh) |
| AC-03 | No function compiled with an AVX2 target attribute under `src/` takes a 256-bit or 512-bit vector parameter by value, since 32-bit x86 GCC and Cygwin/MSYS x86-64 GCC load such an argument from a stack slot without 32-byte alignment; the check rejects a by-value control and accepts the pointer form. | [tests/_static/sh/staticcheck-avx-vector-params.sh](../../tests/_static/sh/staticcheck-avx-vector-params.sh) |

### Coverage boundary

The check recognizes explicit vector types, intrinsic names, shader files and common entry-point syntax in embedded shaders. Compiler auto-vectorization, inline assembly, macro expansion across files and shaders assembled dynamically by host code require review. Review determines whether a missing port is planned, inapplicable or rejected. GPU precision tags declare which paths the source implements. Review and behavioral tests establish actual FP32 coverage and agreement with the scalar result.
