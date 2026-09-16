# CoreGraphics and ImageIO loader

The `coregraphics` loader uses Apple's ImageIO source APIs and CoreGraphics to obtain pixels, then performs libsixel's representation, orientation, alpha, and animation integration. Supported containers and codecs depend on the host framework; the name is not a fixed cross-version list of image formats.

```sh
img2sixel -L 'coregraphics!' image.heic
img2sixel -L 'coregraphics:orientation=0!' image.jpg
img2sixel -L 'coregraphics:cache_max_bytes=0!' -l disable animation.gif
```

## Pixels, metadata, and animation

The adapter has indexed, byte RGB, and float32 conversion paths. It can preserve indexed structure when eligible, while source depth, color interpretation, or alpha/background processing can select a different representation. Profile interpretation through Apple image/color APIs and libsixel's later working-space conversions are separate steps. A float frame is not a guarantee that every source format was decoded at its original precision.

`orientation=0|1` (`O`, enabled by default) controls application of source orientation. Its environment fallback is `SIXEL_LOADER_COREGRAPHICS_ORIENTATION`, then shared orientation policy. A rotation is applied before the shared crop/resize stages, so it can change which source coordinate a crop addresses.

The adapter recognizes framework animation dictionaries for GIF, APNG, WebP, and HEICS where the host exposes them. It reads delays and loop properties, prefers unclamped delays, and uses one pass when auto looping has no loop metadata. Other multi-image containers are treated as still-image sources. Static HEIC support is not evidence of HEICS animation support. The [animation guide](../functionality/animation.md) owns `-S`, `-T`, and looping details.

## Frame-cache budget

`cache_max_bytes=BYTES` (`M`) limits the adapter's decoded-frame cache; its default is 64 MiB and zero disables it. The environment fallback is `SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES`. The cache reuses finalized frame storage across loops and stamps per-emission frame/loop metadata separately. This avoids repeated decoding when the animation fits the cache budget.

The setting is not a total-process memory limit. ImageIO state, current frame buffers, palette work, and encoder queues remain additional allocations. Disabling the cache can trade memory for repeated decoding; it does not disable animation. For reproducible timing, record both cache policy and whether a measurement includes the initial decode or subsequent loops.

## Implementation and validation

[`loader-coregraphics.c`](../../src/loader-coregraphics.c) owns source properties, indexed and float routes, orientation, the frame cache, and animation emission. The [CoreGraphics suite](../../tests/loader/coregraphics/) checks representations, start-frame selection, alpha, orientation, metadata, caches, and failures under the available Apple frameworks. Host-specific format/provider checks may skip. Exact metadata tests, output comparisons, and quality thresholds answer different questions; none establishes universal support across macOS releases.

For document previews rather than ordinary ImageIO decoding, see [Quick Look](quicklook.md).
