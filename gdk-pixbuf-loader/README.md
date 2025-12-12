# libpixbufloader-sixel (gdk-pixbuf SIXEL loader)

## Overview

`sixel-pixbuf-loader` is a gdk-pixbuf module that decodes SIXEL images into
`GdkPixbuf`. It parses terminal escape sequence based SIXEL data and provides
RGB or RGBA pixel buffers to applications. The loader sets
`GDK_PIXBUF_FORMAT_THREADSAFE` and keeps all mutable state inside each load
context to support concurrent decoding.

### Key features
- Incremental loading: data is buffered and decoded at `stop_load`.
- Default background is solid black for RGB. RGBA output keeps transparency.
- Missing ST (ESC \\) / BEL terminators are tolerated; decodable content is
  returned.
- Inputs larger than 6000x6000 pixels are rejected with an error.
- Signature priorities: anchored `ESC P q` is high, unanchored `ESC P` is low.
  MIME type is `image/x-sixel` (alias `image/sixel`).
- File extensions: `sixel`, `six`.

## Requirements
- `gdk-pixbuf-2.0 >= 2.28`
- `gmodule-2.0`
- `libsixel`

## Build and install

### Autotools
1. The loader is **disabled by default** to avoid failing on systems without
   `gdk-pixbuf-2.0`. Enable it explicitly when the dependency is available:
   ```sh
   ./configure --enable-gdk-pixbuf-loader
   ```
2. Build and install:
   ```sh
   make -j$(nproc)
   make install
   ```
3. The gdk-pixbuf binary version is obtained with
   `pkg-config --variable=gdk_pixbuf_binary_version gdk-pixbuf-2.0`. The module
   installs into
   `$(libdir)/gdk-pixbuf-2.0/$(gdk_pixbuf_binary_version)/loaders`.
4. Update the loader cache after installation:
   ```sh
   gdk-pixbuf-query-loaders --update-cache
   ```

### Meson
1. Configure with the loader enabled:
   ```sh
   meson setup builddir -Dgdk_pixbuf_loader=enabled
   ```
2. Build and install:
   ```sh
   meson compile -C builddir
   meson install -C builddir
   ```
3. The install destination matches the Autotools layout.
4. Run `gdk-pixbuf-query-loaders --update-cache` to refresh the cache.

## Usage

1. Once installed and cached, SIXEL files load transparently in any
   gdk-pixbuf-based application. For example, thumbnail creation works with
   `gdk-pixbuf-thumbnailer`:
   ```sh
   gdk-pixbuf-thumbnailer sample.sixel sample.png
   ```
2. GLib applications can call APIs such as `gdk_pixbuf_new_from_file()` with
   `.sixel` or `.six` files; this loader is selected automatically.

## Troubleshooting
- If the loader is not detected, check `gdk-pixbuf-query-loaders` output for
  `sixel` and rerun with `--update-cache` if missing.
- Inputs larger than 6000x6000 pixels or heavily corrupted data return errors.
- For inputs missing terminators, the loader returns the decodable portion. If
  nothing is rendered, the input is likely invalid.

## Further reading
- Detailed behavior and rationale are documented in
  `docs/gdk-pixbuf-loader.md`.
