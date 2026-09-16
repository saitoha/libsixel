# Quick Look preview loader

The `quicklook` loader asks the macOS preview infrastructure for a representative raster image. It can be useful for documents and media that the ordinary image decoders cannot read, but a preview is not a lossless decode of the original document or an enumeration of its pages or animation frames.

```sh
img2sixel -L 'quicklook!' document.pdf
```

The result depends on installed Quick Look providers, OS APIs, source-path access, and the available desktop/service environment. A codec/file type that works through CoreGraphics may still behave differently through a preview provider. The build uses its available modern thumbnail helper and/or legacy `QLThumbnailImageCreate` path; this guide does not promise identical output across those paths.

## Resolution, alpha, and content

The adapter requests a bounded preview using the thumbnail-size hint. The provider chooses the actual returned raster; the shared encoder can then crop or resize it. Increasing output size after loading cannot recover document detail omitted from the thumbnail.

Quick Look output is normalized to gamma RGB888 or RGBA8888. The adapter unpremultiplies provider channels when retaining alpha and can use an explicit background during rendering. It returns a still frame with no original animation timing. The provider may choose a poster frame, page, icon, or other representation, so dimensions and visible content are part of a host-specific preview result.

There are no Quick Look-specific typed loader suboptions in the current registry. In particular, selecting common CMS fields does not establish that the preview retained the source's original ICC profile or sample precision. Quick Look rendering has already interpreted the source before libsixel receives its raster. For ordinary ImageIO source decoding, see [CoreGraphics](coregraphics.md).

## Failure and fallback

`-L quicklook` permits the shared manager to continue according to its fallback rules; the trailing `!` in the example makes failures observable. Missing providers, inaccessible source paths, unavailable services, and thumbnail creation failures can all prevent loading. An extension is only a hint, not proof that a provider can render the file.

Preview generation may involve OS services and provider code. It has different startup, I/O, and memory costs from a small in-process image decoder. Record provider/OS conditions before making quality or latency comparisons; a successful image fixture does not establish every document provider's behavior.

## Implementation and validation

[`loader-quicklook.c`](../../src/loader-quicklook.c) owns path-based thumbnail acquisition, pixel normalization, and frame handoff. The [Quick Look suite](../../tests/loader/quicklook/) exercises pixel formats, representative image/document/media previews, alpha, malformed inputs, and failure cleanup on supported hosts. Many checks depend on provider availability. Perceptual thresholds do not prove byte-identical previews across OS releases or preservation of all document content.
