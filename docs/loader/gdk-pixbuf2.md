# GdkPixbuf image loader

The `gdk-pixbuf2` adapter feeds image data to GdkPixbuf and uses its installed loader modules. That makes the host's module inventory part of effective format support. PNG/JPEG/GIF success does not imply that another container, ICC transform, or animation API is available through the same installation.

```sh
img2sixel -L 'gdk-pixbuf2!' image.png
img2sixel -L 'gdk-pixbuf2!' -S -T 1 animation.gif
img2sixel -L 'gdk-pixbuf2!' -g -l disable animation.gif
```

## Pixels and alpha

The adapter copies GdkPixbuf pixels using their row stride and channel/alpha information. GdkPixbuf's byte pixels are the decode boundary; libsixel can then emit normalized byte RGB or linear float32 when its alpha/background processing requires it. Promoting a decoded byte pixbuf does not recover high-depth samples already reduced by the module.

An explicit background can change both composition and output representation. With no composition background, the adapter records transparent positions for the SIXEL pipeline. Common CMS declarations and a module's own metadata behavior must not be confused with proof that libsixel applied an embedded profile. This backend has no dedicated `cms_engine` or `orientation` suboption in the current registry. Use a specialized codec loader when those exact controls are required.

The adapter currently does not read `gdk_pixbuf_get_option(pixbuf, "icc-profile")` or apply that profile. This is narrower than saying that GdkPixbuf cannot expose ICC: the upstream [PNG](https://github.com/GNOME/gdk-pixbuf/blob/master/gdk-pixbuf/io-png.c) and [JPEG](https://github.com/GNOME/gdk-pixbuf/blob/master/gdk-pixbuf/io-jpeg.c) modules attach Base64 ICC data as a pixbuf option. Availability is module-dependent, and profile access is not color conversion. In particular, the JPEG module can convert CMYK to RGB before returning the pixbuf; applying the original CMYK profile to those RGB samples would be wrong. The [CMS integration policy](color-management/README.md#cms-integration-policy) allows a future connection through exposed metadata after checking its relationship to the delivered samples, without requiring an independent file-header parser. It does not promise original high-depth recovery or current CMS support through this adapter.

## Animation boundary

GdkPixbuf exposes a time-based animation iterator rather than a generic numerical source loop count or total frame count. libsixel advances a synthetic clock and copies the current pixbuf. Negative `-T` selection needs an extra counting traversal, and the adapter's iterator termination/restart rules can differ from source metadata.

In particular, the current adapter cannot preserve an arbitrary finite source repeat count through a generic loop-count query. `auto` and `force` share restart behavior; a finite GIF can repeat beyond its declared count. Use `-l disable` for one traversal or `-S` for a still, and consult the [detailed GdkPixbuf animation discussion](../encoder/animation.md#gdkpixbuf-frame-access-is-not-loop-count-access) before relying on exact counts. Static APNG/WebP support through a module is not evidence of animated support.

## Implementation and validation

[`loader-gdk-pixbuf2.c`](../../src/loader-gdk-pixbuf2.c) owns pixbuf lifetime, pixel copying, alpha normalization, iterator traversal, and frame callbacks. The [GdkPixbuf loader suite](../../tests/loader/gdk-pixbuf/) tests forced loading, pixelformats, GIF traversal/selection, alpha, and errors when the dependency is enabled.

This input adapter is separate from libsixel's [GdkPixbuf loader plugin tests](../../tests/extension/gdk-pixbuf-loader/): that plugin lets GdkPixbuf read SIXEL using libsixel. Plugin success does not test this adapter's GIF loop semantics or installed image modules.
