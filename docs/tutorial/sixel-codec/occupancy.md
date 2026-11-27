# Remember Where Each Color Is

Cutting nodes has been the most expensive part of encoding a photograph since the [nodes](nodes.md) chapter: for every color present in a band, it scans every slot of that color's drawer, and most slots are empty. This chapter notes, while it sorts the pixels into drawers, which columns each color occupies, so that cutting nodes walks only those columns. The decoder is the [masks](masks.md) chapter's, and the stream is still the [packing](packing.md) chapter's. The source is [`code/occupancy.c`](code/occupancy.c).

**You will learn:**

- how a row of bits per color records where the color appears,
- how cutting nodes walks those bits instead of the slots,
- why one extra store per pixel can save far more than it costs.

## Lights for occupied columns

For every color, keep a row of lights: one bit per column, on when the color appears there. Sixty-four lights fit in one 64-bit word. To find the next occupied column, the code does not look at the empty ones at all; `ctz64()` jumps straight to the next light that is on:

![Sixteen columns, of which 2, 3, 9 and 14 are occupied. Count trailing zeros jumps from one set bit to the next](figures/occupancy-lights.svg)

Sorting into drawers sets the light at the moment it fills the slot, which costs one more OR per pixel:

```c
for (int r = 0; r < rows; r++) {
    unsigned char const *row = img->pixels + (size_t)(y + r) * e->width;
    for (int x = 0; x < e->width; x++) {
        e->map[row[x] * e->width + x] |= (unsigned char)(1 << r);
        e->occupancy[row[x] * e->words + (size_t)x / 64] |= UINT64_C(1) << (x % 64);
        e->used[row[x]] = 1;
    }
}
```

Cutting nodes then walks the lights. A light within `NODE_GAP` columns of the current node extends it; any other light starts a new node. Walking a word also clears it, ready for the next band:

```c
for (size_t i = 0; i < e->words; i++) {
    for (uint64_t bits = occ[i]; bits != 0; bits &= bits - 1) {
        int x = (int)(i * 64) + ctz64(bits);
        if (sx >= 0 && x - mx <= NODE_GAP) {
            mx = x + 1;                 /* extend the current node */
            continue;
        }
        if (sx >= 0) add_node(e, c, sx, mx);
        sx = x;                         /* start a new one */
        mx = x + 1;
    }
    occ[i] = 0;
}
add_node(e, c, sx, mx);
```

The nodes are the same as before, so the stream does not change. [Next node](next-node.md) used the same kind of row for the start columns of nodes; here a row per color serves cutting. The encoder also keeps its work areas in one `encoder_t` now and encodes one band at a time in `encode_band()`, which later chapters build on.

## Where this idea came from

chafa's SIXEL encoder kept a bitmap per color from its first version in 2019, which lets a pass skip a group of columns in which that color does not appear. The [implementation history](../../sixel-format/implementation-history.md#chafa-column-words-and-parallel-batches) describes it.

> **Try it.** Set a light only when the slot was still empty, `if (e->map[...] == 0)`, and measure the photograph and the two-color pattern. Does saving stores pay for the extra branch?

## How fast is it?

<!-- measurements:begin:occupancy -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 2.04 ms | 1.7× | 0.77 ms | 1.0× | 328,750 bytes |
| Photograph, 1920×1080 | 10.6 ms | 2.0× | 4.31 ms | 1.0× | 1,663,155 bytes |
| Two colors, 1920×1080 | 5.00 ms | 0.4× | 1.49 ms | 1.0× | 692,322 bytes |
| Random noise, 1920×1080 | 67.7 ms | 1.5× | 18.9 ms | 1.0× | 9,316,560 bytes |
<!-- measurements:end -->

Cutting nodes takes 2.3 ms instead of 15 ms on the photograph, so the whole encoder is twice as fast, and random noise is 1.5 times faster. The two-color pattern loses: every pixel of a 64-column stretch sets a bit in the same occupancy word of one of only two colors, so the stores wait for one another, and cutting nodes was already cheap there. The [vector-lanes](vector-lanes.md) chapter wins that time back. libsixel faces the same trade-off: its [`sixel_band_classify()`](../../../src/encoder-core-encode.c) uses plain per-pixel stores plus occupancy bits for most palettes and a different path for palettes of one or two colors.

Writing the stream is now more than half of the photograph's time. The [next chapter](direct-writes.md) works on that.

Previous: [Find the next node without a search](next-node.md) · Next: [Write without checking every byte](direct-writes.md)
