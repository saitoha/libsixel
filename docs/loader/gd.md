# GD image loader

The `gd` adapter uses supported GD entry points for selected still formats and converts GD's indexed or true-color image into a libsixel frame. Its accepted set depends on the linked GD build and compile-time wrapper support. The adapter deliberately does not accept every format that GD itself may decode.

```sh
img2sixel -L 'gd!' image.png
img2sixel -L gd animation.gif
```

The second command permits fallback. The current adapter rejects GIF so that a later loader can handle its animation semantics; `-L 'gd!' animation.gif` therefore fails instead of silently returning one frame. This is a deliberate backend boundary, not evidence that GD has no GIF decoder.

## Formats and representation

Dispatch includes PNG, JPEG, BMP, TIFF, WBMP, TGA, GD/GD2, and WebP where the corresponding wrapper is available. GD/GD2/WebP availability varies with library/build support. This path returns a still image; it does not preserve an animated WebP timeline.

Indexed images can remain palette-indexed when the requested output constraints permit it. Otherwise the adapter expands pixels and normalizes transparency. GD's alpha convention is a seven-bit inverse-opacity value, not an ordinary 0..255 straight-alpha byte; the adapter converts it explicitly. Source precision observations and background/alpha processing can promote the result to linear float32, while other paths remain byte RGB or indexed.

Promotion is a representation decision after GD decoding. It cannot restore channel detail already reduced by GD. The current adapter is also not a general ICC/Exif metadata-preservation layer: it has no backend-specific `cms_engine` or `orientation` field. Common color/background policy must be interpreted at the actual consumer, not assumed from another loader's implementation.

For alpha-bearing images, compare palette transparency, full alpha, explicit background composition, and alpha-zero masks separately. An indexed result with one transparent entry and a true-color result with a mask need not take the same downstream route even when their visible pixels agree.

## Implementation and validation

[`loader-gd.c`](../../src/loader-gd.c) owns format dispatch, GD wrapper checks, indexed retention, alpha conversion, and float/byte output selection. The [GD test category](../../tests/loader/gd/) includes exact representation/mask cases, format dispatch, intentional GIF fallback, optional-format builds, and defensive paths. The GIF fallback tests require a later loader; they do not claim that forced GD accepts GIF.

See [loader architecture](README.md) for chain construction and [pixel formats](../concepts/pixelformat.md) for the handoff contract.
