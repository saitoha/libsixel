# Emscripten Compatibility

## Scope

This document owns the `__EMSCRIPTEN__` boundary and the WebAssembly/JavaScript runtime assumptions that differ from a native POSIX host. The overall inventory is in the [platform compatibility ledger](README.md).

## Build contract

Autotools detects Emscripten intent from the host triple or `emcc` before generic compiler discovery, normalizes the inferred host to wasm32 Emscripten, and chooses `emar`, `emranlib`, and related companion tools. Meson uses its Emscripten host identity. Moving detection later risks probing the build host or selecting native binutils.

Emscripten `-s` settings are final-link settings. `-sRETAIN_COMPILER_SETTINGS=1`, `-sNODERAWFS=1`, `-sFETCH=1`, codec ports, stack size, memory size, and environment settings must reach the executable link without being mistaken for ordinary C compile flags. The compiler settings are retained because runtime path handling queries `NODERAWFS`.

Thread-enabled command-line programs and the test runner link with `-sPROXY_TO_PTHREAD=1 -sEXIT_RUNTIME=1` in both build systems. Running C `main()` on a worker leaves the JavaScript runtime available to process worker cleanup messages while C waits for queued frames or joins helper threads. Otherwise repeated frame work can accumulate completed workers and their stacks while the runtime message loop is blocked. Runtime exit also terminates the worker pool when `main()` returns. These settings apply to executable targets only: library consumers retain control over their own JavaScript lifecycle, and thread-disabled executables do not use the proxy. See Emscripten's [pthreads guidance](https://emscripten.org/docs/porting/pthreads.html).

SDK codec ports contribute their `-sUSE_LIB...=1` link settings to `Libs.private`. They do not add host `libjpeg`, `libpng`, `libtiff` or `libwebp` pkg-config requirements: those SDK ports have no corresponding host `.pc` package. The generated metadata must resolve independently of host codec development packages.

Split output contains both JavaScript and WebAssembly artifacts. Installation uses the sidecar installer and launcher templates so the executable entry point can find its `.js` and `.wasm` companions; installing only a nominal program file is incomplete.

Coverage CI uses gcov instrumentation (`--enable-gcov` or Meson `-Db_coverage=true`) and reads the resulting `.gcda`/`.gcno` files with the SDK LLVM coverage reader. It does not also enable `-fprofile-instr-generate` or `-fcoverage-mapping`: the SDK compiler and its bundled source-profile runtime can disagree on the profile-record layout, causing an out-of-bounds access while writing profiles at exit. Emscripten can then report a secondary reentrant-exit assertion. A successful test must both terminate normally and produce counters that the reader accepts; disabling runtime exit or ignoring exit failure would invalidate that evidence. The [static coverage check](../../testing/staticcheck.md#test-coverage) protects the matrix configuration.

## Runtime contract

The Node raw-filesystem build includes [emscripten-node-stdio.js](../../../build-aux/emscripten-node-stdio.js) before runtime startup. On the Node main thread it puts available native stdin/stdout/stderr handles into blocking mode before pthread workers share those descriptors. This matches C stdio’s synchronous filesystem calls: a full pipe waits for its reader instead of failing an image write with `EAGAIN`. Browser runtimes and worker message streams are excluded. [Node documents asynchronous POSIX pipe streams](https://nodejs.org/api/process.html#a-note-on-process-io); the handle operation is Node-specific and requires runtime pipeline testing when upgrading Node.

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
| EM-03 | Both build systems retain paired worker-proxy and runtime-exit link settings for thread-enabled executable targets. | [tests/_static/sh/staticcheck-emscripten-thread-runtime.sh](../../../tests/_static/sh/staticcheck-emscripten-thread-runtime.sh) |
| EM-04 | Both build systems retain the Node main-thread standard-stream initializer for raw filesystem I/O. | [tests/_static/sh/staticcheck-emscripten-node-stdio.sh](../../../tests/_static/sh/staticcheck-emscripten-node-stdio.sh) |

### Coverage boundary

The static check protects build and source structure but does not execute JavaScript, load a `.wasm` sidecar, or observe Node.js filesystem and descriptor behavior. Emscripten CI must cover those runtime properties. Long palette-auto playback cases exercise repeated thread creation and joining; the static check does not prove worker reclamation or a runtime duration.
