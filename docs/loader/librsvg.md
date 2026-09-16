# librsvg image loader

The `librsvg` loader rasterizes SVG through librsvg and Cairo, producing a still raster for the ordinary palette/SIXEL pipeline. The result has a finite raster resolution; later `-w`/`-h` resizing does not make the SIXEL output vector graphics or rerender the source at arbitrary detail.

```sh
img2sixel -L 'librsvg!' drawing.svg
img2sixel -L 'librsvg:relative_resources=1!' drawing.svg
img2sixel -L 'librsvg:stdin_svgz=1!' < drawing.svgz
```

## Geometry and pixels

The adapter first tries intrinsic pixel dimensions, then available intrinsic dimensions/viewBox information. When no usable size can be obtained, it uses the conventional 300×150 viewport. It checks the resulting allocation bounds before creating a Cairo surface. Source dimensions and shared post-load crop/resize are different stages.

Cairo renders premultiplied channels. The adapter normalizes those channels for its RGB/RGBA frame boundary, retaining alpha when needed and processing an explicit background during rendering. The result is gamma byte RGB/RGBA, not a general float/HDR SVG color pipeline. Fonts and rendering features depend on the installed librsvg/font stack; identical source XML alone does not fix text metrics across hosts.

## Resource and compressed-input policy

| Suboption | Default | Behavior |
| --- | --- | --- |
| `relative_resources=0|1` (`A`) | `0` | Permit the local source-file route used to resolve relative resources. Environment: `SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES`. |
| `stdin_svgz=0|1` (`S`) | `0` | Permit gzip SVG input without a usable local source-file route by staging it in a temporary `.svgz` file. Environment: `SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ`. |

A local `.svgz` file has its own file-based route. The `stdin_svgz` setting is therefore not a universal enable/disable switch for every compressed SVG. With the temporary route, filesystem creation/write/close failures can fail loading before rendering.

Enabling relative resources changes how the document is supplied to librsvg; it is not a promise to allow arbitrary remote URLs or bypass librsvg's own resource restrictions. Conversely, the adapter's default byte route should not be described as a complete sandbox for all SVG behavior. Use self-contained documents when reproducible resource resolution matters.

This adapter has no SVG animation timeline and does not expose an independent backend-specific ICC engine selector. Shared loader fields do not imply that a renderer consumes every raster-codec metadata policy.

## Implementation and validation

[`loader-librsvg.c`](../../src/loader-librsvg.c) owns recognition, handle creation, geometry, Cairo rendering, channel normalization, and temporary-file cleanup. The [librsvg suite](../../tests/loader/librsvg/) covers SVG recognition, fallback/viewBox/explicit geometry, alpha/background behavior, compressed input, relative resources, and failure paths. Real fonts and external resources still need a controlled host setup; geometry tests do not prove portable typography or full SVG feature conformance.

The [loader architecture](README.md) explains strict selection and fallback; [crop and resize](../functionality/crop-resize.md) explains what happens after rasterization.
