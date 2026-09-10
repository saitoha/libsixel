# Emscripten Compatibility

## Scope

This document owns the `__EMSCRIPTEN__` boundary and the WebAssembly/JavaScript runtime assumptions that differ from a native POSIX host. The overall inventory is in the [platform compatibility ledger](README.md).

## Build contract

Autotools detects Emscripten intent from the host triple or `emcc` before generic compiler discovery, normalizes the inferred host to wasm32 Emscripten, and chooses `emar`, `emranlib`, and related companion tools. Meson uses its Emscripten host identity. Moving detection later risks probing the build host or selecting native binutils.

Emscripten `-s` settings are final-link settings. `-sRETAIN_COMPILER_SETTINGS=1`, `-sNODERAWFS=1`, `-sFETCH=1`, codec ports, stack size, memory size, and environment settings must reach the executable link without being mistaken for ordinary C compile flags. The compiler settings are retained because runtime path handling queries `NODERAWFS`.

Split output contains both JavaScript and WebAssembly artifacts. Installation uses the sidecar installer and launcher templates so the executable entry point can find its `.js` and `.wasm` companions; installing only a nominal program file is incomplete.

## Runtime contract

When `NODERAWFS` is enabled, Windows-style drive and MSYS paths can refer to the host filesystem. The path adapters query `emscripten_get_compiler_setting("NODERAWFS")` and conservatively assume enabled when the query API or value is unavailable. When raw filesystem access is disabled, these paths must remain untouched.

Emscripten descriptors are treated as noninteractive because Node.js streams do not reliably implement the native terminal ioctls used by cursor-position queries. Signal, `select`, fork, and exclusive temporary-file assumptions are excluded where the runtime cannot provide the required observation. A false missing-path result from `access` is rechecked with `stat`, and temporary creation does not rely on `O_EXCL` under `NODERAWFS`.

BSD libfetch is replaced by the synchronous `emscripten_fetch` adapter. Its `-sFETCH=1` dependency belongs to the final link and error handling must retain HTTP status and size validation.

## Maintenance checklist

- Keep toolchain inference before generic compiler and companion-tool discovery.
- Put Emscripten settings on the final link and preserve runtime compiler-setting retention when path behavior depends on it.
- Treat `.js` and `.wasm` as one installed program unit.
- Do not re-enable terminal, process, signal, or filesystem assumptions without testing the intended Node.js/browser runtime.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| EM-01 | Emscripten toolchain intent remains ahead of generic compiler discovery, and the build retains link-only `NODERAWFS`, compiler-settings, and Fetch flags. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| EM-02 | Runtime path handling queries `NODERAWFS`, network fetching uses `emscripten_fetch`, and terminal/temp-file exclusions remain explicit. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh), [tests/platform/path/0001_path_to_libc_runtime.t](../../../tests/platform/path/0001_path_to_libc_runtime.t) |

### Coverage boundary

The static check protects build and source structure but does not execute JavaScript, load a `.wasm` sidecar, or observe Node.js filesystem and descriptor behavior. Emscripten CI must cover those runtime properties.
