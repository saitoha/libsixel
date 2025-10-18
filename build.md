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
```

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
| `--enable-quicklook-extension` | `auto` | Build the macOS Quick Look extension bundle (requires macOS ≥ 10.15). |
| `--enable-quicklook-preview` | `auto` | Use macOS Quick Look to render previews for non-image inputs. |
| `--with-coregraphics[=auto]` | `auto` | Use CoreGraphics for macOS-only rendering helpers. |
| `--with-libcurl[=auto]` | `auto` | Link against libcurl to enable network transfers. |
| `--with-jpeg[=auto]` | `auto` | Link against libjpeg to decode JPEG input. |
| `--with-png[=auto]` | `auto` | Link against libpng to decode PNG input. |
| `--with-gdk-pixbuf2` | `no` | Build the optional gdk-pixbuf2 loader module. |
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
| `--enable-debug` | `no` | Enable debug macros and apply extra diagnostic compiler flags. |
| `--enable-gcov` | `no` | Compile with gcov coverage instrumentation. |
| `--enable-tests` | `no` | Build the optional test suites. |

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
codec by configuring with `--with-wiccodec`.  If `--register-dll` is specified,
`make install` calls `regsvr32` to register `libwicsixel.dll`.  To unregister
later:

```sh
make uninstall
```

The uninstall target invokes `regsvr32 /u` automatically when the DLL was
registered during install.

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
| `-Djpeg=` | feature, `auto` | Link against libjpeg for JPEG decoding. |
| `-Dcurl=` | feature, `auto` | Link against libcurl for network transfers. |
| `-Dgdk_pixbuf2=` | feature, `disabled` | Build the gdk-pixbuf2 loader module. |
| `-Dgd=` | feature, `disabled` | Enable helpers based on the GD image library. |
| `-Dcoregraphics=` | feature, `auto` | Use the CoreGraphics framework on macOS. |
| `-Dquicklook_extension=` | feature, `auto` | Build the macOS Quick Look extension bundle. |
| `-Dquicklook_preview=` | feature, `auto` | Use macOS Quick Look to render previews for non-image inputs. |
| `-Dwic=` | feature, `auto` | Use Windows Imaging Component (WIC) libraries. |
| `-Dwiccodec=` | feature, `auto` | Build the Windows WIC codec DLL. |
| `-Dregister_dll=` | boolean, `false` | Register the WIC codec DLL during `meson install` (requires admin rights). |
| `-Dwinhttp=` | feature, `auto` | Use WinHTTP on Windows for network operations. |
| `-Dwinpthread=` | feature, `disabled` | Link against `libwinpthread`. |
| `-Dbashcompletiondir=` | string, `disabled` | Install the bash completion script (`auto` selects the system default path). |
| `-Dzshcompletiondir=` | string, `disabled` | Install the zsh completion script (`auto` selects the system default path). |
| `-Dsimd=` | feature, `enabled` | Control SIMD acceleration; `auto` defers to detection, `disabled` forces off. |
| `-Dpython=` | feature, `disabled` | Build and install the Python bindings. |
| `-Druby=` | feature, `disabled` | Build and install the Ruby bindings. |
| `-Dtests=` | boolean, `false` | Build the test suites. |
| `-Dgcov=` | boolean, `false` | Enable gcov coverage instrumentation. |
| `-Dimg2sixel=` | feature, `enabled` | Build the `img2sixel` CLI tool. |
| `-Dsixel2png=` | feature, `enabled` | Build the `sixel2png` CLI tool. |

To install shell completion scripts, add `-Dbashcompletiondir=auto` or
`-Dzshcompletiondir=auto` to the command above, or supply the target directory
path explicitly.

Then build and run tests:

```sh
meson compile -C builddir
meson test -C builddir
meson install -C builddir  # may require sudo on Unix-like systems
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

For additional details, see the project README and the `.github/workflows`
directory for platform-specific build recipes.
