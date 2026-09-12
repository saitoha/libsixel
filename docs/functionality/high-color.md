# High-Color Output

`img2sixel -I` enables the legacy high-color path. It can paint more than 256 colors by redefining palette registers during an image, but it is not a general quality upgrade. Its color assignment and pass scheduling remain serial, it does not benefit from the complete normal palette pipeline, and current measurements expose visible tone steps and a malformed-output stress case. Keeping this mode useful requires work on correctness, independent band encoding, and color representation together.

```console
img2sixel -I image.png
img2sixel --high-color image.png
```

The familiar “maximum 15bit color” description refers to its 32,768 internal lookup keys. It does **not** promise 32,768 colors in each image, or even impose that exact maximum on the distinct RGB values painted over the complete stream. See [what the color counts mean](high-color/measurement.md#what-32768-means) and the [side-by-side comparisons](high-color/measurement.md#visual-comparisons).

Araki Ken introduced the encoder in [commit `0382aef`](https://github.com/libsixel/libsixel/commit/0382aef1afa5333ad2d69f4412900accca866ef2). His 2014 article [“libsixelによるSixel Graphicsへの変換処理について”](https://qiita.com/arakiken/items/4a216af6547d2574d283) explains the original algorithm and terminal distinction. The following describes the current implementation, including differences from that historical design.

## The terminal decides whether it works

The extension relies on paint-time palette semantics. Suppose a stream defines register `#0` as red, paints a pixel, redefines `#0` as blue, and paints another pixel. A paint-time renderer resolves the RGB value when each pixel is drawn, leaving one red and one blue pixel. A live-register renderer retains the register reference, so redefining `#0` also changes the earlier pixel to blue.

<picture>
  <source media="(max-width: 640px)" srcset="high-color-figures/high-color-register-mobile.svg">
  <img alt="A stream defines register zero as red and paints the left pixel, then redefines register zero as blue and paints the right pixel. A live-register display makes both pixels blue, while a paint-time display preserves red then blue." src="high-color-figures/high-color-register-wide.svg">
</picture>

*Figure 1. The same wire events produce different pixels under the two register models.*

Historical DEC hardware such as the VT340 follows the live-register model, so high color is not a guarantee of the original hardware semantics. Use `-I` only with a receiving terminal known to implement paint-time register resolution. Terminal support can change independently of libsixel and must be verified on the actual receiver.

## How repeated passes extend the palette

The encoder takes the top five bits of each RGB channel and packs them into a 15-bit lookup key. It registers the original RGB representative, not necessarily the value with its low three bits cleared. It uses registers `#0` through `#254` for paint colors and reserves `#255` as a skip key. A pass assigns available registers to new keys, paints matching pixels with their current representatives, and marks them complete. If colors remain, later passes reuse registers with new definitions and paint only the unfinished pixels.

<picture>
  <source media="(max-width: 640px)" srcset="high-color-figures/high-color-passes-mobile.svg">
  <img alt="The top five bits of each RGB channel form lookup keys. Up to 255 colors are assigned to registers zero through 254 while register 255 is the skip key. Painted pixels are marked, registers are redefined, and passes repeat until all pixels are complete." src="high-color-figures/high-color-passes-wide.svg">
</picture>

*Figure 2. Conceptual high-color loop. Exact pass boundaries depend on register availability, color reuse, and already-painted marks.*

This explains why high color can improve fidelity and enlarge the stream at the same time: the image can use far more distinct colors, but it may carry repeated palette definitions and multiple paint passes.

## Pipeline and option interactions

High color is an alternate encoding path, not a larger value for `-p`.

- It bypasses fixed-palette construction and fixed-palette nearest-color lookup. `-p`, `-b`, `-m`, and `-e` select incompatible color modes and are rejected when combined with `-I`.
- The current CLI resolves palette-space diffusion to `none` for `-I`. Supplying `-d fs` or another diffusion method does not recreate the historical high-color diffusion path described in the 2014 article.
- Register `#255` is an internal skip value for multi-pass painting. Transparent offset is supported as a separate geometry feature; source alpha semantics still follow the [alpha policy](../loader/alpha-policy.md).
- Validation must use a direct decoder. A final indexed plane and final palette cannot represent the earlier colors painted before a register redefinition. Use `sixel2png --direct` (`-D`) rather than an indexed snapshot. See the [PNG writer boundary](../writers/png.md) for details.

## Parallelism: the current boundary

The high-color front end in [`sixel_encode_highcolor()`](../../src/encoder-core-highcolor.c) walks pixels and chooses register replacements serially. `rgbhit`, `rgb2pal`, palette representatives, and six-row `marks` carry information between passes; a pass can cover several bands and then return to an unfinished band. Clearing `palstate` and hit counters at `next:` is a **pass reset**, not an independently initialized image band.

The common [`sixel_encode_body()`](../../src/encoder-core-encode.c) can already send a prepared pass through its band workers when there is more than one band and more than one requested thread. Thus, there is some parallel serialization inside `-I`; the complete high-color operation is not independently parallelized by band. Many short passes, serial register assignment, and repeated scheduling limit the benefit. Merely increasing `--threads` does not remove that dependency.

### Required encoder direction: reset state at each band

The prerequisite for useful encode scaling is a complete, independently encodable six-row band, including all of its palette-redefinition passes. A proposed implementation should:

1. Start each band with worker-local key maps, representatives, register-use state, and painted marks. No register choice may depend on a preceding band's cache.
2. Define every register used by that band before its first paint. This is an explicit initialization contract, not a new SIXEL “reset palette” opcode; clearing an encoder array alone would leave the receiver's palette undefined for the fragment.
3. Finish all repaint passes for the same band inside that job. `$` returns to the start of the current six-row band, while `-` advances to the next one; concatenation must preserve that distinction and emit the image header/footer only once.
4. Publish complete band fragments in source order, with correct handling of the final partial band, alpha/skip pixels, offsets, and cancellation. Bounded queues must prevent slow bands from accumulating an entire image of pending fragments.

This design has not been implemented. Resetting at every band also discards potentially useful color reuse and can increase register-definition bytes. It can change which source sample becomes a key's representative and create stronger seams. Compare source pixels, band boundaries, throughput, and transport size together before accepting it; speed alone would not establish an improvement.

### Why direct decode is harder

An arbitrary byte offset does not reveal its destination row or the current palette. High color also changes register meaning between paint operations, sometimes repainting the same six-row band through several passes. A single final index plane and palette cannot reconstruct those historical colors.

The current [parallel decoder](../threading/decoder.md) initially establishes raster and palette state, scans byte spans, and proves that paint ranges are disjoint. Its workers in [`decoder-parallel.c`](../../src/decoder-parallel.c) reject palette definitions inside those spans, so typical high-color streams return to serial parsing. More decode workers may add an unsuccessful scan without accelerating paint.

This is a difficult state-reconstruction problem, not a proof that parallel decoding is mathematically impossible. A future decoder could serially discover boundaries and capture each band's entering palette and cursor state, then paint complete bands independently in RGBA. That still pays for a serial prepass, snapshots, validation, and within-band operation order. Streams that independently initialize their bands would simplify that task, but the present byte-span parser does not gain that capability merely because the encoder changes. Arbitrary existing high-color streams need their own fallback policy. General high-color decode parallelization remains unresolved.

## What the measurements show

The [measurement reference](high-color/measurement.md) contains photographs, smooth color and grayscale ramps, a key-reuse diagnostic, exact RGB counts and histograms, retained SIXEL/PNG artifacts, and all timing samples. These are measurements of revision `3c92b3a1c` with an independent `-O3` build, not terminal screenshots.

| Fixture | Requested 256 colors, no diffusion | `-I`, no diffusion | Interpretation |
| --- | --- | --- | --- |
| Snake, 600×450 | 254 actual RGB colors; MS-SSIM 0.986100; 247,009 bytes | 14,838 actual RGB colors; MS-SSIM 0.989064; 675,644 bytes | More colors and lower mean color error, at 2.74× the size; 256-color FS still has higher MS-SSIM (0.991198). |
| Smooth gradient, 600×450 | 255 colors; MS-SSIM 0.904965 | 1,714 colors; MS-SSIM 0.979565 | A substantial improvement, but visible rectangles and tone steps remain. |
| Gray ramp, 768×96 | 64 colors; MS-SSIM 0.983637 | 32 colors; MS-SSIM 0.937472 | The 15bit key has only 32 positions on the neutral axis; this mode can preserve fewer gray levels. |

![Full HD RGB encode and fixed-stream direct decode API times by requested thread budget, with medians and interquartile ranges](high-color/study/thread-scaling.png)

The new graph uses one API boundary at every thread count. A [later full repetition](high-color/measurement.md#repeat-run-variability) showed substantial runtime variation, so precise speedup ratios remain provisional. RGB encode includes palette construction, palette application/dither, and SIXEL output to `/dev/null`; direct decode starts from a fixed in-memory stream and excludes PNG writing. The normal encoder changes some decoded pixels with thread budget in this snapshot, so this is a comparison of the requested policies, not a byte-identical serialization benchmark. Exact quality and size checks at one and eight threads are retained in the measurement record. High-color decoded pixels match between those budgets for all six fixtures.

### Why the old two-to-three-thread graph was misleading

The earlier graph measured the first encode-worker start through the last worker completion. At two threads, dither had already finished when that interval began. At three threads, the [pipeline starts overlapping dither and encode](../threading/encoder.md), so the interval also included waits for bands still being dithered. The old 14.287 → 52.901 ms discontinuity compared different included work; it did not show that an otherwise identical encode became almost four times slower on an extra core. The axis represented requested threads, not physical cores.

That historical worker-window graph is superseded for scaling claims. Its [raw samples](high-color/measurements/high-color-speed.csv) remain available as historical scheduling evidence, but the new API graph and [timing definitions](high-color/measurement.md#speed-and-thread-budgets) govern comparisons in this document.

## Work needed before treating this as a quality mode

The measured key-reuse diagnostic produces missing pixels and paints beyond its declared height. Those correctness failures need regression coverage and repair before performance work is accepted. A band-local encoder must not preserve such behavior for byte compatibility.

For color quality, distinguish the key from the representative. The current first-encountered RGB sample, its lifetime in a reused slot, and rounding to SIXEL's integer percentage channels can create biased steps. Candidate changes include deterministic representatives, perceptual or adaptive quantization, and a deliberate high-color diffusion policy; each requires a new quality/size/speed comparison. Restoring an old diffusion routine through `-d` without defining ownership and cross-band error propagation would reintroduce dependencies and is not a complete solution.

Retain this mode as an explicitly limited alternate path until it has a correctness baseline, independent band encoding, a documented representation policy, and evidence that the extra colors improve the intended images. Increasing the theoretical color count by itself addresses none of those requirements.

## Implementation and existing coverage

The repeated-pass implementation is in [`encoder-core-highcolor.c`](../../src/encoder-core-highcolor.c), CLI mode selection and diffusion bypass are in [`encoder.c`](../../src/encoder.c), and paint-time palette resolution is in [`fromsixel.c`](../../src/fromsixel.c). Focused existing tests exercise the 255 paint-register boundary in [`0166_high_color_sixel_direct_palette_slots.t`](../../tests/quant/palette/usage/0166_high_color_sixel_direct_palette_slots.t), preservation across direct decoding in [`0023_decoder_high_color_dequantize_bypass.c`](../../tests/processing/decoder/0023_decoder_high_color_dequantize_bypass.c), and transparent-offset geometry in [`0175_transparent_offset_high_color_geometry.t`](../../tests/quant/palette/usage/0175_transparent_offset_high_color_geometry.t). These tests do not establish correctness for every palette-reuse or final-band case.

The [study tool](../../tools/highcolor/study.py) verifies retained hashes, dimensions, painted color counts, histograms, register bounds, and timing-sample coverage. Its diagnostic measurements explicitly record existing failures; they are not a passing quality regression threshold. Regenerate the conceptual figures with `tools/reproduce_encoding_mode_figures.sh` and verify them with `--check`.
