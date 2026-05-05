# Building libsixel

This document explains how to build libsixel using the traditional Autotools
build system as well as the Meson build system. Examples assume a POSIX-like
shell unless noted otherwise.

## Prerequisites

Before building, ensure the following tools are available:

- A C compiler (e.g. gcc, clang, or MSVC on Windows)
- `make` (GNU Make for Autotools; Ninja is recommended for Meson)
- `pkg-config` (optional)
- Optional dependencies: libpng, libjpeg, libcurl and others if you want the
  corresponding features enabled.

### Additional notes

- On macOS, command line tools (`xcode-select --install`) or Xcode are required.
- On Windows, install MSYS2, Cygwin, or Visual Studio depending on the target
  build environment.

## Building with Autotools

### Unix-like systems (Linux, macOS, \*BSD)

```sh
./configure
make
make check    # optional
make install  # may require sudo
make installcheck  # optional: run tests against installed commands
```

`installcheck` follows the usual "install-style" intent (validate installed
artifacts rather than build-tree outputs).  `make installcheck` expects
`make install` to be completed first and then runs the existing TAP suite with
`PATH` and loader variables adjusted to the installed tree.  When validating a
staged package image, pass `SIXEL_INSTALLCHECK_DESTDIR=/path/to/stage`.

Customize `./configure` options to enable PNG, JPEG, curl, or Quick Look
extension support.  Run `./configure --help` to see the full list of options.

Bash and Zsh completion scripts are not installed by default.  If you need
them, specify `--with-bashcompletiondir=auto` or `--with-zshcompletiondir=auto`
or provide the desired directory path explicitly.

#### Autotools build options

| Option | Default | Description |
| --- | --- | --- |
| `--disable-img2sixel` | disabled (builds by default) | Skip building the `img2sixel` CLI tool. |
| `--disable-sixel2png` | disabled (builds by default) | Skip building the `sixel2png` CLI tool. |
| `--enable-abort-trace` | `auto` | Enable abort stack trace support for CLI tools (unsupported on Emscripten and Windows targets). |
| `--enable-quicklook-extension` | `auto` | Build the macOS Quick Look extension bundle (requires macOS ≥ 10.15). |
| `--enable-quicklook-preview` | `auto` | Use macOS Quick Look to render previews for non-image inputs. |
| `--disable-appkit` | enabled | Build the macOS clipboard backend without AppKit (default uses AppKit when available). |
| `--enable-thumbnailer-command` | `auto` | Enable the FreeDesktop.org thumbnailer command bridge used by GNOME, Cinnamon, MATE, and Xfce (via Tumbler) as well as other desktops that honour `.thumbnailer` definitions. |
| `--disable-threads` | `auto` | Disable the internal threadpool support. |
| `--enable-amalgamated-lib` | `no` | Build `libsixel` from the amalgamated translation unit. |
| `--enable-amalgamated-tools` | `no` | Build CLI tools from the amalgamated translation unit. |
| `--with-coregraphics[=auto]` | `auto` | Use CoreGraphics for macOS-only rendering helpers. |
| `--with-libcurl[=auto]` | `auto` | Link against libcurl to enable network transfers. |
| `--with-libfetch[=auto]` | `auto` | Link against libfetch to enable network transfers. |
| `--with-jpeg[=auto]` | `auto` | Link against libjpeg to decode JPEG input. |
| `--with-png[=auto]` | `auto` | Link against libpng to decode PNG input. |
| `--with-tiff[=auto]` | `auto` | Link against libtiff to decode TIFF input. |
| `--with-webp[=auto]` | `auto` | Link against libwebp to decode WebP input. |
| `--with-lcms2[=auto]` | `auto` | Link against lcms2 for ICC color management. |
| `--with-librsvg[=auto]` | `auto` | Link against librsvg (+cairo) for SVG rendering. |
| `--with-gdk-pixbuf2` | `no` | Build the optional gdk-pixbuf2 loader module. |
| `--enable-gdk-pixbuf-loader` | `no` | Build the gdk-pixbuf SIXEL loader module. |
| `--with-gd` | `no` | Enable helpers that use the GD image library. |
| `--with-winpthread` | `no` | Link against `libwinpthread` (Windows targets only). |
| `--with-wic[=auto]` | `auto` | Use Windows Imaging Component (WIC) libraries when present. |
| `--with-winhttp[=auto]` | `auto` | Use WinHTTP on Windows for network operations. |
| `--with-pkgconfigdir=DIR` | `libdir/pkgconfig` | Install `libsixel.pc` under the specified pkg-config directory. |
| `--with-bashcompletiondir[=DIR]` | `no` (disabled) | Install the bash completion script (`auto` selects the system default path). |
| `--with-zshcompletiondir[=DIR]` | `no` (disabled) | Install the zsh completion script (`auto` selects the system default path). |
| `--enable-simd` / `--disable-simd` | `auto` | Control SSE2/NEON SIMD acceleration detection; `auto` enables when supported. |
| `--enable-wiccodec` / `--disable-wiccodec` | `auto` | Build the Windows WIC codec DLL. |
| `--enable-register-dll` | `no` | Call `regsvr32` during `make install` to register the WIC codec DLL (requires admin rights). |
| `--enable-python` | `no` | Build and install the Python bindings. |
| `--enable-ruby` | `no` | Build and install the Ruby bindings. |
| `--enable-perl` | `no` | Enable the Perl binding compatibility checks used by build scripts. |
| `--enable-php` | `no` | Build the bundled PHP FFI package artifact under `php/dist/`. |
| `--enable-debug` | `no` | Enable debug macros and apply extra diagnostic compiler flags. |
| `--enable-coverage` | `no` | Compile with compiler coverage instrumentation. |
| `--enable-gcov` | `no` | Deprecated alias for `--enable-coverage`. |
| `--enable-analyzer` | `no` | Enable static analyzer mode. Compiler-specific behavior: clang requires `scan-build`, gcc adds `-O2 -fanalyzer`, MSVC/clang-cl adds `/W4 /analyze /wd28253`. |
| `--enable-sanitizer=<profile>` | `no` | Enable sanitizer instrumentation (`address`, `undefined`, `address,undefined`, `memory`, `thread`, `function`, `implicit-integer-truncation`, `cfi-icall`, `object-size`, `integer`, or `float-divide-by-zero`). The `integer` profile keeps intentional unsigned wrap and unsigned shift-base arithmetic unreported. |
| `--enable-lto=<mode>` | `no` | Enable link-time optimization (`full` or `thin`). |
| `--enable-thinlto-cache` | `no` | Enable linker ThinLTO cache (requires `--enable-lto=thin`). |
| `--with-thinlto-cache-dir=PATH` | auto | ThinLTO cache directory (default: `${TMPDIR-/tmp}/libsixel-thinlto-cache` when cache is enabled). |
| `--enable-pgo=<mode>` | `no` | Enable profile-guided optimization (`generate` or `use`). |
| `--with-pgo-data=PATH` | empty | Profile data directory/path used by PGO generate/use mode. |
| `--with-pgo-profdata=FILE` | empty | LLVM merged profile used by `--enable-pgo=use` (GNU/LLVM toolchains). |
| `--enable-fuzz` | `no` | Build libFuzzer targets under `fuzz/`. |
| `--enable-tests` | `no` | Build the optional test suites. |
| `--enable-xsave-probe` / `--disable-xsave-probe` | `auto` | Control `_xgetbv` probing during AVX capability detection. |
| `--with-shebang-file=PATH` | disabled | Prepend the contents of `PATH` to generated executables (skips files that already start with a shebang) and mark them executable. |
| `--disable-emscripten-retain_compiler_settings` | enabled | Disable `-sRETAIN_COMPILER_SETTINGS=1` in Emscripten builds. |
| `--disable-emscripten-noderawfs` | enabled | Disable `-sNODERAWFS=1` in Emscripten builds. |
| `--with-emscripten-stack_size=SIZE` | `786432` | Set Emscripten `-sSTACK_SIZE`. |
| `--with-emscripten-initial_memory=SIZE` | `268435456` | Set Emscripten `-sINITIAL_MEMORY`. |
| `--with-emscripten-environment=LIST` | empty | Set Emscripten `-sENVIRONMENT`. |
| `--disable-emscripten-allow_memory_growth` | enabled | Disable `-sALLOW_MEMORY_GROWTH=1` in Emscripten builds. |
| `--disable-emscripten-single_file` | disabled | Enable `-sSINGLE_FILE=1` (configure semantics follow current implementation). |
| `--disable-emscripten-wasm_bigint` | disabled | Enable `-sWASM_BIGINT=1` (configure semantics follow current implementation). |
| `--disable-emscripten-assertions` | enabled | Add `-sASSERTIONS=0` in Emscripten builds. |

#### macOS Quick Look preview

When the Quick Look preview feature is enabled (the default on macOS builds),
`img2sixel` falls back to macOS Quick Look APIs to rasterize non-image inputs
such as PDF or SVG files before encoding them to SIXEL. Use the
`--disable-quicklook-preview` configure flag or `-Dquicklook_preview=disabled`
in Meson to turn this behavior off.

#### macOS Quick Look extension

To build and install the Quick Look extension, ensure you are running on macOS
10.15 or later and pass `--enable-quicklook-extension`. After `make install`,
the script `tools/quicklook-extension.bash` is called automatically to register
the Preview and Thumbnail extensions for the current user.  To unregister them,
run:

```sh
tools/quicklook-extension.bash uninstall \
    --products-dir quicklook-extension/products \
    --register-tool quicklook-extension/register_sixel_preview
```

#### Windows WIC codec (Autotools)

When building with Autotools under MSYS2 or Cygwin, you can enable the WIC
codec by configuring with `--enable-wiccodec`.  If `--register-dll` is specified,
`make install` calls `regsvr32` to register `libwicsixel.dll`.  To unregister
later:

```sh
make uninstall
```

The uninstall target invokes `regsvr32 /u` automatically when the DLL was
registered during install.

### Static analyzer builds (Autotools)

Enable analyzer mode at configure time:

```sh
./configure --enable-analyzer
```

Build command behavior depends on the compiler:

- clang: run build through `scan-build` explicitly.
- gcc / MSVC / clang-cl: run normal `make`.

Example (clang):

```sh
SCAN_BUILD="$(command -v scan-build || command -v scan-build-18 || command -v scan-build-17 || command -v scan-build-16)"
"$SCAN_BUILD" --status-bugs --keep-going make -j"$(nproc)" V=1
```

### Emscripten (Autotools)

`emscripten/build-emcc.sh` is included as a **sample helper** that wraps the
steps below.  For a transparent, reproducible setup without the script, follow
this manual procedure.

1. **Install and activate Emscripten SDK** (skip if you already have `emcc`):

   ```sh
   git clone --depth 1 https://github.com/emscripten-core/emsdk.git emscripten/emsdk
   emscripten/emsdk/emsdk install latest
   emscripten/emsdk/emsdk activate latest
   . emscripten/emsdk/emsdk_env.sh
   ```

2. **Prepare a build directory** for the generated JS:

   ```sh
   mkdir -p emscripten/build
   ```

3. **Configure** (from `emscripten/build`) using `--with-shebang-file` to embed
   a Node-friendly shebang into the generated wrapper:

   ```sh
   cd emscripten/build
   SHEBANG_FILE="$(pwd)"/emscripten-node-shebang
   printf '#!/usr/bin/env node\n' > "${SHEBANG_FILE}"
   emconfigure ../../configure \
     --host=wasm32-unknown-emscripten \
     --disable-shared \
     --with-shebang-file="${SHEBANG_FILE}" \
     CFLAGS="-O3" \
     LDFLAGS="-sWASM_BIGINT=1 \
              -sSINGLE_FILE=1 \
              -sENVIRONMENT=node \
              -sABORTING_MALLOC=0 \
              -sNODERAWFS=1 \
              -sFORCE_FILESYSTEM=1 \
              -sALLOW_MEMORY_GROWTH=1 \
              -sINITIAL_MEMORY=67108864 \
              -sSTACK_SIZE=2097152 \
              -flto"
   ```

   The `--with-shebang-file=PATH` flag inserts the contents of `PATH` (for
   example, `#!/usr/bin/env node`) at the top of the generated JavaScript
   wrapper so the resulting program is directly executable without an
   additional shim.

   When `-sSINGLE_FILE=1` is not enabled (the current Autotools default unless
   `--disable-emscripten-single_file` is passed), `make install` uses a split
   runtime layout:

   - `$(libexecdir)/libsixel`: JS wrappers and `.wasm` sidecars
   - `$(bindir)`: launcher scripts that call `node` explicitly

4. **Build and (optionally) test**:

   ```sh
   emmake make -j
   emmake make check  # optional, requires Node.js
   ```

Re-run the configure and build steps after updating Emscripten or changing
configuration flags.  `emscripten/build-emcc.sh` can still be used as a quick
reference or automated example of the same sequence.

## Building with Meson

Meson provides a faster and more portable build.  Install Meson and Ninja via
pip or your package manager:

```sh
pip install meson ninja  # or use your package manager
```

Create a build directory and configure:

```sh
meson setup builddir
```

#### Meson build options

| Option | Type / Default | Description |
| --- | --- | --- |
| `-Dpng=` | feature, `auto` | Link against libpng for PNG decoding. |
| `-Dtiff=` | feature, `auto` | Link against libtiff for TIFF decoding. |
| `-Djpeg=` | feature, `auto` | Link against libjpeg for JPEG decoding. |
| `-Dwebp=` | feature, `auto` | Link against libwebp for WebP decoding. |
| `-Dlcms2=` | feature, `auto` | Link against lcms2 for ICC color management. |
| `-Dlibrsvg=` | feature, `auto` | Link against librsvg (+cairo) for SVG rendering. |
| `-Dcurl=` | feature, `auto` | Link against libcurl for network transfers. |
| `-Dfetch=` | feature, `auto` | Link against libfetch for network transfers. |
| `-Dgdk_pixbuf2=` | feature, `disabled` | Build the gdk-pixbuf2 loader module. |
| `-Dgdk_pixbuf_loader=` | feature, `disabled` | Build the gdk-pixbuf SIXEL loader module. |
| `-Dgd=` | feature, `disabled` | Enable helpers based on the GD image library. |
| `-Dcoregraphics=` | feature, `auto` | Use the CoreGraphics framework on macOS. |
| `-Dappkit=` | feature, `enabled` | Control AppKit clipboard backend support on macOS. |
| `-Dquicklook_extension=` | feature, `auto` | Build the macOS Quick Look extension bundle. |
| `-Dquicklook_preview=` | feature, `auto` | Use macOS Quick Look to render previews for non-image inputs. |
| `-Dwic=` | feature, `auto` | Use Windows Imaging Component (WIC) libraries. |
| `-Dwiccodec=` | feature, `auto` | Build the Windows WIC codec DLL. |
| `-Dregister_dll=` | boolean, `false` | Register the WIC codec DLL during `meson install` (requires admin rights). |
| `-Dwinhttp=` | feature, `auto` | Use WinHTTP on Windows for network operations. |
| `-Dwinpthread=` | feature, `disabled` | Link against `libwinpthread`. |
| `-Dthreads=` | feature, `auto` | Control internal threadpool support. |
| `-Dbashcompletiondir=` | string, `disabled` | Install the bash completion script (`auto` selects the system default path). |
| `-Dzshcompletiondir=` | string, `disabled` | Install the zsh completion script (`auto` selects the system default path). |
| `-Dsimd=` | feature, `enabled` | Control SIMD acceleration; `auto` defers to detection, `disabled` forces off. |
| `-Dpython=` | feature, `disabled` | Build and install the Python bindings. |
| `-Druby=` | feature, `disabled` | Build and install the Ruby bindings. |
| `-Dphp=` | feature, `disabled` | Build the bundled PHP FFI package artifact under `build/php/dist/`. |
| `-Dperl=` | feature, `disabled` | Enable Perl binding tests. |
| `-Dfuzz=` | boolean, `false` | Build libFuzzer targets under `fuzz/`. |
| `-Dfuzz_sanitizers=` | combo, `auto` | Select sanitizer profile for fuzz targets (`auto`, `full`, or `fuzzer`). |
| `-Dtests=` | boolean, `true` | Build the test suites. |
| `-Damalgamated_lib=` | boolean, `false` | Build `libsixel` from the amalgamated translation unit. |
| `-Damalgamated_tools=` | boolean, `false` | Build CLI tools from the amalgamated translation unit. |
| `-Dshebang_file=` | string, empty | Prepend the file contents to generated executables (skips files that already start with a shebang) and mark them executable. |
| `-Dgcov=` | boolean, `false` | Enable gcov coverage instrumentation. |
| `-Danalyzer=` | boolean, `false` | Enable static analyzer mode. Compiler-specific behavior: clang requires `scan-build` at setup time, gcc adds `-O2 -fanalyzer`, MSVC/clang-cl adds `/W4 /analyze /wd28253`. |
| `-Dexperimental_sanitizer=` | combo, `none` | Enable a Clang-only experimental sanitizer profile (`function`, `implicit-integer-truncation`, `cfi-icall`, `object-size`, `integer`, or `float-divide-by-zero`). The `integer` profile keeps intentional unsigned wrap and unsigned shift-base arithmetic unreported. |
| `-Dmsvc_pgo=` | combo, `off` | MSVC/clang-cl specific PGO mode (`off`, `generate`, `use`). |
| `-Dmsvc_pgo_data=` | string, empty | Optional `/PGD:` path used when `-Dmsvc_pgo=generate/use`. |
| `-Dimg2sixel=` | feature, `enabled` | Build the `img2sixel` CLI tool. |
| `-Dsixel2png=` | feature, `enabled` | Build the `sixel2png` CLI tool. |
| `-Dabort_trace=` | feature, `auto` | Enable abort stack traces for CLI tools. |
| `-Dthumbnailer_command=` | feature, `auto` | Install the SIXEL thumbnailer command integration. |
| `-Dxsave_probe=` | feature, `auto` | Control `_xgetbv` probing during AVX capability detection. |
| `-Demscripten_retain_compiler_settings=` | boolean, `true` | Add `-sRETAIN_COMPILER_SETTINGS=1` for Emscripten builds. |
| `-Demscripten_noderawfs=` | boolean, `true` | Add `-sNODERAWFS=1` for Emscripten builds. |
| `-Demscripten_stack_size=` | integer, `786432` | Set Emscripten `-sSTACK_SIZE`. |
| `-Demscripten_initial_memory=` | integer, `268435456` | Set Emscripten `-sINITIAL_MEMORY`. |
| `-Demscripten_environment=` | string, empty | Set Emscripten `-sENVIRONMENT`. |
| `-Demscripten_allow_memory_growth=` | boolean, `true` | Add `-sALLOW_MEMORY_GROWTH=1` for Emscripten builds. |
| `-Demscripten_single_file=` | boolean, `false` | Add `-sSINGLE_FILE=1` for Emscripten builds. |
| `-Demscripten_wasm_bigint=` | boolean, `false` | Add `-sWASM_BIGINT=1` for Emscripten builds. |
| `-Demscripten_assertions=` | boolean, `true` | Keep Emscripten assertions enabled (`false` adds `-sASSERTIONS=0`). |

For Emscripten targets with `-Demscripten_single_file=false`, Meson installs
the JS wrappers and `.wasm` sidecars into `libexecdir/libsixel` and installs
`bindir` launchers that execute those wrappers via `node`.
| `-Db_lto=` | base option, `false` | Meson base option for link-time optimization. |
| `-Db_lto_mode=` | base option, `default` | LTO mode (`default`/`thin`; `thin` requires compatible toolchain). |
| `-Db_pgo=` | base option, `off` | Meson base option for profile-guided optimization (`generate`/`use`). |
| `-Db_sanitize=` | base option, `none` | Meson base sanitizer option (`address`, `undefined`, etc.). |

To install shell completion scripts, add `-Dbashcompletiondir=auto` or
`-Dzshcompletiondir=auto` to the command above, or supply the target directory
path explicitly.

Then build and run tests:

```sh
meson compile -C builddir
meson test -C builddir
meson install -C builddir  # may require sudo on Unix-like systems
meson compile -C builddir installcheck  # optional: tests installed commands
```

### Static analyzer builds (Meson)

Enable analyzer mode at setup time:

```sh
meson setup builddir -Danalyzer=true
```

Build command behavior depends on the compiler:

- clang: run compile through `scan-build` explicitly.
- gcc / MSVC / clang-cl: run normal `meson compile`.

Example (clang):

```sh
SCAN_BUILD="$(command -v scan-build || command -v scan-build-18 || command -v scan-build-17 || command -v scan-build-16)"
"$SCAN_BUILD" --status-bugs --keep-going meson compile -C builddir
```

## Git hooks

Developers can opt into the repository-provided Git hooks to catch common
mistakes before they land in a commit.  The pre-commit hook currently rejects
any staged file that tries to include private headers located under `src/`
because public tools such as the CLI converters must restrict themselves to
the installed headers in `include/`.  It also inspects any staged
`Makefile.am`/`Makefile.in` to ensure recipe lines remain tab-indented, which
prevents accidental replacement of tabs with spaces.

Enable the hooks once per clone by pointing Git at the bundled hook directory:

```sh
git config core.hooksPath .githooks
```

The check is also available as a standalone script, so continuous-integration
jobs can run `tools/check_private_includes.py` directly without installing the
hook.  Likewise, `tools/check_makefile_recipes.sh` can be executed manually or
from CI to enforce tab-indented recipes:

```sh
tools/check_makefile_recipes.sh path/to/Makefile.am
```

### macOS Quick Look (Meson)

Quick Look support is controlled by the feature option `-Dquicklook_extension`.
When the extension is built and you run `meson install`, Meson executes
`tools/quicklook-extension.bash install …` automatically to register the
extensions for the current user.  If you are staging into a `DESTDIR`, the user
registration step is skipped.  To unregister later, invoke the run target:

```sh
ninja -C builddir uninstall-quicklook-extension
```

### Windows WIC codec (Meson)

Enable the codec via

```sh
meson setup builddir -Dwiccodec=enabled -Dregister_dll=true …
```

When `register_dll` is true, `meson install` runs `regsvr32 /s` on the installed
`libwicsixel.dll` (skipping registration when installing into a `DESTDIR`).  To
deregister it manually, run:

```sh
ninja -C builddir uninstall  # removes files
# then unregister the codec
regsvr32 /u /s "$PREFIX\lib\libsixel\libwicsixel.dll"
```

Note: Meson does not currently run unregister commands during `ninja uninstall`,
so deregistration must be performed manually or through a custom script.

## Platform-specific notes

- **MSVC (Visual Studio):** Configure with Meson using `meson setup builddir
  --buildtype=release --default-library=static`.  Run from a Developer Command
  Prompt to ensure environment variables are set, then use `meson compile`.

- **MSYS2 MinGW:** Install dependencies via `pacman -S --needed base-devel mingw-w64-x86_64-toolchain`.
  Use either Autotools or Meson as described above.

## Cleaning

- Autotools: `make clean` or `make distclean`
- Meson: `meson compile -C builddir --clean` or remove the build directory.

## Troubleshooting

- Verify all required dependencies are installed and available via `pkg-config`.
- On Windows, ensure `regsvr32` is accessible and run shell/PowerShell sessions
  with the necessary privileges when registering DLLs.
- For Meson, inspect `builddir/meson-logs/meson-log.txt` if configuration or
  compilation fails.
- Use `meson configure builddir` to review active options.
- In POSIX `sh`, environment assignments written before a shell function
  call (for example `VAR=value run_test_runner ...`) are not reliably exported
  to child processes across all shells. Prefer `env VAR=value command ...` or
  an explicit `export VAR` before invoking helper functions in TAP scripts.

For additional details, see the project README and the `.github/workflows`
directory for platform-specific build recipes.
