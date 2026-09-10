# Encoding Policy

`img2sixel -E POLICY` controls how the already indexed image is serialized into SIXEL bytes. It does not choose the palette, assign pixels to palette entries, or change dithering. Use `-E size` when stream size matters and accept a little extra encoding work; use the default `auto` or explicit `fast` for the direct mask-preserving path.

```console
img2sixel -E size image.png
img2sixel --encode-policy=size image.png
```

The option and its size-oriented overpainting idea were introduced by Araki Ken in [commit `c8283ed`](https://github.com/libsixel/libsixel/commit/c8283ed460aab6baafd72a15f56882e904c8cc8e). His Japanese article ["SIXEL Graphics: 100% Practical Know-How"](https://qiita.com/arakiken/items/4a216af6547d2574d283) is the historical source for the explanation below. This document updates that model for the current implementation and measurements.

## Where `-E` acts

SIXEL divides an image into bands six pixels high. Within a band, a SIXEL data character stores six vertical paint bits. The encoder selects a palette register with `#n`, writes that color's mask, uses `$` to return to the band's left edge, and uses `-` to advance to the next band. Repeated data characters can be shortened with `!n` run-length encoding.

<picture>
  <source media="(max-width: 640px)" srcset="encode-policy-figures/encode-policy-band-mobile.svg">
  <img alt="An eight by six indexed grid uses blue, amber, green, and pink. Four masks are paired with their literal SIXEL characters. The auto and fast paint body is #1o{BN#3o{BN$#0NB{o#2NB{o, while size uses #1!4~#3!4~$#0NB{o#2NB{o." src="encode-policy-figures/encode-policy-band-wide.svg">
</picture>

*Figure 1. Literal paint bodies emitted for the illustrated 8 by 6, four-color band. The DCS wrapper, raster attributes, palette definitions, and string terminator are omitted so the part controlled by `-E` remains visible.*

For this concrete band, the current encoder emits the following paint bodies:

```text
auto / fast  #1o{BN#3o{BN$#0NB{o#2NB{o
size         #1!4~#3!4~$#0NB{o#2NB{o
```

These are actual SIXEL characters, not pseudocode. Subtracting ASCII `?` from a data character recovers its six vertical paint bits: `o`, `{`, `B`, and `N` describe the holes in the exact masks, while `~` sets all six bits. Thus `!4~` paints four full-height columns. The size plan first lays down four amber columns and four pink columns, returns left with `$`, and repairs the blue and green pixels. Its 23-byte paint body is two bytes shorter than the 25-byte exact body, yet both decode to the same indexed pixels. A taller image would use `-` between six-row bands.

This boundary is useful when diagnosing an option. If changing `-E` changes decoded pixels for the same indexed input, that is a correctness problem rather than an expected quality tradeoff.

## Why `size` can save bytes

The ordinary path preserves the holes in every color mask: it never temporarily paints a pixel belonging to another color. The `size` path may instead fill a span with an early color and let later colors repaint their own pixels. Solid or repeated columns can form shorter `!n` runs, so a little overdraw can produce fewer bytes.

<picture>
  <source media="(max-width: 640px)" srcset="encode-policy-figures/encode-policy-overpaint-mobile.svg">
  <img alt="The same eight-by-six blue, amber, green, and pink band is encoded twice. Auto and fast preserve all four mask shapes in a 25-byte body. Size paints solid amber and pink blocks, repairs their blue and green pixels, and reaches the identical result with a 23-byte body." src="encode-policy-figures/encode-policy-overpaint-wide.svg">
</picture>

*Figure 2. The same four-color band and literal bodies as Figure 1, now shown as paint order. `size` replaces two masks containing holes with two `!4~` rectangles, then repairs their temporary overdraw. The decoded pixels remain identical while this paint body shrinks from 25 to 23 bytes.*

The optimization is not a compressor applied after encoding; it changes the paint plan itself. It is therefore content-dependent. Large spans with reusable fills tend to help, while noisy masks may provide little benefit and can occasionally offset savings with extra paint commands.

## Where fill-and-repaint cannot combine

The central weakness belongs specifically to the fill-and-repaint optimization behind `-E size`, not to the `-E` parser or the exact-mask `auto` and `fast` paths. Filling a hole is safe only when a later paint is guaranteed to restore that pixel. Several other modes give a hole the opposite meaning, so their defining behavior and the Figure 2 optimization cannot operate on the same pixels.

| Other feature | Why the fill-and-repaint idea conflicts |
| --- | --- |
| `-I` high color | Later passes intentionally omit pixels completed by earlier passes while redefining palette registers. Filling those holes could overwrite completed pixels, and no later pass is guaranteed to restore them. |
| `-O` OR mode | Painting sets color-plane bits by OR. A later paint can add another bit but cannot clear a bit introduced by a solid underpaint, so the Figure 2 repair step is impossible. |
| Transparent pixels or `--transparent-offset` | Under DCS `P2=1`, an omitted cell means “keep the destination pixel.” Painting through that cell destroys the screen content it was meant to preserve. |
| 6delta encoding | An omitted/key-color cell means “this pixel is unchanged from the retained plane.” Underpainting it would turn an unchanged pixel into a changed one and defeat delta encoding. |

This is an algorithmic incompatibility, not necessarily a command-line rejection. The current implementation can accept some of these option combinations, but it must fence off the generic overpaint opportunity wherever a protected hole occurs. Its OR-mode `size` path can skip an empty bit plane, for example, but that is a separate size optimization rather than fill-and-repaint. Similarly, transparent-offset clipping can still optimize a fully opaque safe span, but not a transparent hole within it. In practice the characteristic `-E size` saving is strongest for one opaque, indexed, replace-mode image; it cannot simply be stacked with the characteristic savings or semantics of `-I`, `-O`, transparent reuse, or 6delta on the same region.

## Policies

| Policy | Current behavior | Practical use |
| --- | --- | --- |
| `auto` | The default. It currently follows the same non-size body path as `fast`. | Preserve the default interface so future automatic selection remains possible. |
| `fast` | Preserves each color mask without the size-policy fill and repaint strategy. | Request the direct serialization path explicitly. |
| `size` | Enables safe fill and repaint opportunities and skips some empty work in specialized paths. | Prefer smaller streams when the receiver or transport is the bottleneck. |

`auto` is a policy name, not currently an image-adaptive choice between the other two modes. Both values take the same non-size encoder path, and the measurement below produced byte-identical streams. They are therefore not distinct performance modes in the current implementation: any observed timing difference between them is measurement noise. Code should nevertheless pass `fast` when that exact intent matters instead of relying on today's implementation of `auto`.

Transparency is therefore more nuanced than the original 2014 description. The current size-policy code checks whether each band can be filled safely and clips transparent-offset work at image boundaries; it does not categorically reject the option combination. The [alpha policy](../loader/alpha-policy.md) still owns whether a source pixel is composited or omitted, while `-E` only decides how the surviving safe masks are serialized.

## Measured quality, speed, and size

The controlled comparison used the repository's `images/snake.png` fixture at revision `24e46743fd78167db78ca9d72754e50b6a834947`. All runs used the builtin loader, RGB palette space, 8-bit precision, high quality, a fixed 256-color budget, no diffusion, and no GPU. Quality and size used the original 600 by 450 image with one thread; each SIXEL stream was decoded through `sixel2png --direct` before assessment.

Speed was measured separately on the same image scaled to 1920 by 1080. For every policy and every `--threads` value from 2 through 12, the harness performed two warmups and recorded 21 JSON timelines. One sample is the wall interval from the earliest `encode/worker/worker_start` event to the latest `encode/worker/worker_done` event. Image loading, palette construction, dither work before the first encode worker starts, and ordered writer work are therefore excluded. Once the first encode worker has started, however, waiting for later banded-dither results remains inside the interval. One-thread mode has no encode-worker events and is intentionally absent rather than being measured with a different boundary.

![Quality identity, runtime, and SIXEL byte size for auto, fast, and size encoding policies](encode-policies/measurements/encode-policy-results.png)

*Figure 3. Thread count is the horizontal axis of the speed panel. Each box spans the interquartile range, its line is the median, whiskers extend to 1.5 times the IQR, and isolated points are outliers. `auto` and `fast` use the same encoder path, so small separations between their boxes are sampling noise rather than a policy effect. Size bars report exact stream bytes converted to KiB.*

| Policy | MS-SSIM | Mean Delta E00 | SIXEL bytes |
| --- | ---: | ---: | ---: |
| `auto` | 0.986100 | 2.403138 | 247,009 |
| `fast` | 0.986100 | 2.403138 | 247,009 |
| `size` | 0.986100 | 2.403138 | 226,666 |

| Threads | `auto` median [Q1--Q3] | `fast` median [Q1--Q3] | `size` median [Q1--Q3] |
| ---: | ---: | ---: | ---: |
| 2 | 13.669 [13.522--13.763] ms | 13.538 [13.461--13.789] ms | 13.178 [13.083--13.346] ms |
| 3 | 48.003 [47.801--48.327] ms | 47.928 [47.817--48.107] ms | 47.932 [47.639--48.234] ms |
| 4 | 35.284 [34.573--36.127] ms | 34.747 [34.400--36.277] ms | 34.700 [34.360--35.181] ms |
| 8 | 20.740 [20.503--21.250] ms | 20.961 [20.816--21.263] ms | 20.760 [20.387--21.088] ms |
| 12 | 17.189 [17.049--17.462] ms | 17.044 [16.648--17.547] ms | 17.133 [16.901--17.374] ms |

All three direct-decode PNG files had the same SHA-256 digest, so quality was pixel-identical rather than merely equal after metric rounding. `auto` and `fast` also had the same encoded-stream digest. Across the full thread grid their median gaps were at most 0.537 ms and their boxes overlapped; the data provides no evidence that they are distinct speed modes. `size` reduced the stream by 20,343 bytes, or 8.24%, while its encode-window distribution remained interleaved with the same-path policies. Ordered writing is outside this timing boundary, so the smaller byte count does not automatically make the measured interval shorter.

The 2-thread plan finishes dithering before encode workers begin, so its short interval must not be read as an end-to-end speedup over 3 threads. From 3 threads onward, dither and encode overlap and the interval includes later-band arrival waits. The boxes widen sharply when the dither side first becomes parallel at 4 threads: the observed IQR was 0.821--1.877 ms across policies, versus 0.241--0.328 ms at 2 threads. They do not then widen monotonically; at 12 threads the range was 0.413--0.899 ms. The result is consistent with uneven band completion becoming visible to encode, but this fixture does not support the stronger claim that variability continually increases with thread count.

This single fixture demonstrates the intended pixel invariant, one realistic byte saving, and one host's scheduling behavior. It is not a universal compression ratio or speed ranking.

## Reproduce the comparison

Run the measurement only from a clean tracked worktree so the recorded revision identifies the binary under test:

```console
PYTHON=/path/to/python-with-matplotlib-and-numpy \
tools/reproduce_encoding_mode_measurements.sh
```

The script rebuilds the selected tree, records commands and provenance, measures both this comparison and the companion [high-color comparison](high-color.md), regenerates the plots, and validates semantic invariants. The durable artifacts are the [quality and size CSV](encode-policies/measurements/encode-policy-comparison.csv), the [raw speed samples](encode-policies/measurements/encode-policy-speed.csv), and the [JSON run record](encode-policies/measurements/encode-policy-run.json). Regenerate the explanatory figures with `tools/reproduce_encoding_mode_figures.sh`; its `--check` mode verifies byte-for-byte freshness.

## Implementation and existing coverage

Policy parsing is in [`encoder.c`](../../src/encoder.c), the public constants are in [`sixel.h`](../../include/sixel.h), and mask serialization is in [`encoder-core-encode.c`](../../src/encoder-core-encode.c). Existing focused coverage includes transparent-offset fill clipping in [`0173_transparent_offset_encode_policy_size.t`](../../tests/quant/palette/usage/0173_transparent_offset_encode_policy_size.t) and size-policy handling of empty OR planes in [`0006_encoder_core_ormode_body_skips_empty_planes.c`](../../tests/processing/encoder-core/0006_encoder_core_ormode_body_skips_empty_planes.c).
