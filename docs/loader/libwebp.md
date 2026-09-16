# libwebp image loader

The `libwebp` loader uses the linked WebP codec/demux APIs for static images and animation, then normalizes frames through libsixel's metadata, alpha, and CMS policies. This is a separate implementation from [builtin WebP](builtin/webp.md); success or coverage in one path does not prove the other path behaves identically.

```sh
img2sixel -L 'libwebp!' image.webp
img2sixel -L 'libwebp!' -S -T 1 animation.webp
img2sixel -L 'libwebp:max_output_frames=100!' -g -l disable animation.webp
```

Use the trailing `!` when backend identity matters. Available compressed formats and animation APIs remain build-dependent; inspect `img2sixel --version` before forcing this component.

## Static and animated paths

Static lossless input and alpha-preserving cases can use byte RGB/RGBA decoding. An eligible static lossy image instead follows a YUV-to-float path: the current selector requires lossy input, no forced RGB decode, and either no alpha or an explicit composition background. This preserves intermediate conversion precision; it does not turn the source into a higher-bit-depth WebP file. CMS and `prefer_8bit` can further change the emitted representation.

Animated input uses `WebPAnimDecoder` composed canvases and timestamps. libsixel derives delays from timestamp differences, applies frame selection and repetition, and can cache finalized first-pass frames for reuse. Blend/disposal composition is therefore owned differently from the builtin decoder's WebP container/codec implementation. See [animation playback](../functionality/animation.md) for loop counts, timing units, `-S`, `-T`, and the current one-frame-tail replay boundary.

| Control | Meaning |
| --- | --- |
| `orientation=0|1` (`O`) | Enable Exif orientation; default enabled. Fallback: `SIXEL_LOADER_LIBWEBP_ORIENTATION`, then shared orientation policy. |
| `cms_engine=...` (`E`) | Select the external WebP adapter's CMS engine. Fallback: `SIXEL_LOADER_LIBWEBP_CMS_ENGINE`, then shared CMS policy. |
| `max_output_frames=COUNT` (`M`) | Bound animation frame emission (1..262144, default 262144). It is a resource/output guard, not a requested frame rate or a substitute for `-l`. Fallback: `SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES`. |
| Common CMS and background fields | Govern profile target, rendering-intent preference, byte preference, and composition interpretation. See [color management](color-management.md) and [background policy](background-policy.md). |

ICC extraction, Exif orientation, transparent output, and background composition are distinct steps. A valid ICC chunk does not prove that a transform was applied, and an animation's opaque canvas does not establish how an uncomposited static alpha image is represented. Use diagnostics and a controlled fixture when comparing routes.

## Implementation and validation

[`loader-libwebp.c`](../../src/loader-libwebp.c) owns static-path selection, metadata extraction, animation iteration/cache, output-frame bounds, and normalization. The [libwebp test category](../../tests/loader/libwebp/) covers still/animated VP8 and VP8L, delays and finite loops, start frames, frame limits, metadata bounds, orientation, CMS, alpha, and failure cleanup. Some tests assert exact streams or metadata; LSQA cases establish only their quality thresholds. Disabled codec/demux support produces skips rather than evidence of working animation.

The [loader manager](README.md) controls cross-backend fallback. Once this adapter has delivered a frame, a later decode or callback error terminates the load; it cannot safely replace the partial animation with another loader's output.
