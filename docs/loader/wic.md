# Windows Imaging Component loader

For the optional SIXEL codec installed for Windows applications, see the [WIC codec companion](../programs/wic-codec.md). This input loader does not install or register that codec.

The `wic` loader uses Windows Imaging Component to read an image container and return one selected frame. Available formats depend on installed WIC codecs. The adapter's normalization and frame-selection behavior are narrower than the complete feature set of those codecs.

```sh
img2sixel -L 'wic!' image.png
img2sixel -L 'wic!' -T 1 multiframe.gif
img2sixel -L 'wic:ico_minsize=64!' application.ico
```

Use `!` to stop another loader from hiding a WIC failure. The component is available only in builds with the WIC implementation.

## Frame selection is not animation playback

For non-ICO multi-frame containers, `-T` selects a frame by index, including a negative index relative to the frame count. The adapter returns after that selected frame. It does not implement timed GIF playback, disposal composition, or source loop repetition; `-S` and `-l` do not supply the missing player. See the [animation matrix](../encoder/animation.md).

ICO entries are alternative image sizes rather than animation frames. The `ico_minsize=COUNT` (`I`) suboption uses each entry's longer dimension as its size metric, chooses the smallest entry meeting the requested minimum, or chooses the largest available entry when none meets it. The environment fallback is `SIXEL_LOADER_WIC_ICO_MINSIZE`. Without that policy, do not assume the first entry is the largest or sharpest icon.

## Representation and metadata

The adapter retains eligible indexed pixels with byte RGB palette entries; otherwise it requests `GUID_WICPixelFormat32bppRGBA`, an 8-bit-per-channel conversion. libsixel then normalizes alpha/background and hands a typed frame to the shared encoder. High-depth sources therefore cross a byte decode boundary in this adapter, even if later processing promotes the frame to float32. The adapter does not add the ICC/orientation integration documented for libpng, libjpeg, or CoreGraphics.

WIC itself provides [frame color contexts](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-iwicbitmapframedecode-getcolorcontexts) and [color transforms](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-colormanagement). The current adapter does not retrieve those contexts or invoke `IWICColorTransform`; `IWICFormatConverter` and `CopyPixels` are not that connection. This is an adapter integration gap, not a claim that WIC cannot expose ICC data. The [CMS integration policy](color-management/README.md#cms-integration-policy) prioritizes using these backend APIs over adding a separate file-header parser. That pending connection does not remove the independent byte-precision limit above.

There is no `wic:orientation` or `wic:cms_engine` selector in the current backend-specific option registry. Do not transfer another loader's option spelling or metadata guarantees to WIC. Use the frame representation and controlled reference pixels to establish the actual route, especially for transparent icons and multi-frame GIF extraction.

## Implementation and validation

[`loader-wic.c`](../../src/loader-wic.c) owns COM decoder/converter lifetime, frame selection, ICO size selection, and normalization. The [WIC suite](../../tests/loader/wic/) covers frame indexes and bounds, forced decoding, ICO policies, alpha, and allocation/error paths. It is conditional on Windows/WIC and the required fixtures. A test skipped on another platform is not receiver validation, and extracting a GIF frame is not proof of complete animated-GIF semantics.
