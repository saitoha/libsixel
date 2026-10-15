# Step 2: kmiya's Nodes

[Step 1](step1-run-length.md) scans each band once per palette color and writes a line for every color. kmiya's `sixel` encoder, the [origin of libsixel](../../project-history.md#kmiyas-sixel), avoids both costs. It reads each band once, records each color's columns in a per-color row, cuts those rows into *nodes*, and packs nodes of different colors side by side onto as few graphics lines as possible. This step implements that design and completes the decoder. The source is [`code/step2.c`](code/step2.c).

The [SIXEL format overview](../../sixel-format/README.md#nodes-and-greedy-non-overlapping-sweeps) illustrates the same node scheduling as libsixel performs it; this page builds it from scratch.

## Encoder

The encoder handles one band in four phases.

### 1. Classify: one pass over the pixels

```c
for (r = 0; r < 6 && y + r < h; r++) {
    unsigned char const *row = img->pixels + (size_t)(y + r) * w;
    for (x = 0; x < w; x++) {
        map[row[x] * w + x] |= (unsigned char)(1 << r);
        used[row[x]] = 1;
    }
}
```

`map` has one row of `w` bytes per palette color. After the six source rows have been read, `map[c * w + x]` holds exactly the six-bit value of the sixel character for color `c` at column `x`. Each pixel is read once, whatever the number of colors.

### 2. Compose: cut rows into nodes

A node is a span `[sx, mx)` of one color's row. It starts at an occupied column and ends after the last occupied column. Gaps of up to nine empty columns stay inside the node, where they cost a short `!n?` run; a longer gap ends the node, because skipping it with a separate node is cheaper.

```c
for (sx = 0; sx < w; sx = mx) {
    for (; sx < w && row[sx] == 0; sx++) {
    }
    if (sx == w) {
        break;
    }
    mx = sx + 1;
    for (x = mx; x < w && x - mx <= NODE_GAP; x++) {
        if (row[x] != 0) {
            mx = x + 1;
        }
    }
    /* ... record node (c, sx, mx) ... */
}
```

### 3. Order: by start column, longest first

Nodes go into one bucket per start column. Within a bucket the longest node comes first, and equal nodes keep color order. Concatenating the buckets from left to right gives the node list sorted by `sx`, then by decreasing `mx`.

### 4. Emit: pack lines greedily

```c
while (list != NULL) {
    if (color >= 0) {
        buf_putc(out, '$');             /* back to column 0 */
    }
    x = 0;
    for (link = &list; (np = *link) != NULL;) {
        if (np->sx < x) {
            link = &np->next;           /* overlaps; next line */
            continue;
        }
        if (np->color != color) {
            buf_putc(out, '#');
            buf_putnum(out, np->color);
            color = np->color;
        }
        buf_putrun(out, '?', np->sx - x);   /* blank columns */
        put_span(out, np->map, np->sx, np->mx);
        memset(np->map + np->sx, 0, (size_t)(np->mx - np->sx));
        x = np->mx;
        *link = np->next;               /* unlink */
    }
}
```

Each pass over the list writes one graphics line. It takes the first node, then every later node that starts at or after the cursor, and leaves overlapping nodes for the next line after `$`. A color with several nodes on one line is selected once, and nodes of different colors share a line whenever they do not overlap. Emitting a node also clears its part of the map, so the map is all zeros again when the band is done without a separate clearing pass.

`color` starts at `-1` for every band, so every band selects its first color explicitly even when the previous band ended with the same one. That costs a few bytes per band and makes the bytes of a band independent of the band before it, which [step 5](step5-threads.md) relies on to encode bands in parallel.

## Decoder

The step 2 decoder accepts everything the step 1 decoder rejects:

- **HLS colors.** `#n;1;h;l;s` defines a register in hue, lightness and saturation. SIXEL places blue at 0°, red at 120° and green at 240°, so the usual formula is applied after rotating the hue; see `set_hls()` in [`code/common.c`](code/common.c).
- **No raster attributes, or paint beyond them.** The canvas grows on demand, at least doubling each time so that the cost stays linear. The final image covers both the declared raster and every painted pixel, which is how libsixel treats [raster declarations versus observed paint](../../decoder/pipeline/README.md#raster-declaration-versus-observed-paint).
- **Repeats.** A repeated character fills the same columns of up to six rows, so the decoder writes each set row with one `memset()` instead of testing six bits for every repeated column. A single character, the common case in photographs, takes plain stores, because a library call per row would cost more than it saves.
- **Bookkeeping per band.** The canvas size is checked before painting with two comparisons, and the lowest painted row is computed once per band from the OR of the band's characters, not once per character.

```c
if (ch >= '?' && ch <= '~') {
    bits = ch - '?';
    if (bits != 0) {
        /* ... grow the canvas to (x + repeat) x (y + 6) if needed ... */
        for (r = 0; r < 6; r++) {
            if ((bits >> r & 1) == 0) {
                continue;
            }
            dst = cv.pixels + (size_t)(y + r) * cv.width + x;
            if (repeat == 1) {
                *dst = (unsigned char)color;    /* the common case */
            } else {
                memset(dst, color, (size_t)repeat);
            }
        }
        band_rows |= bits;
    }
    x += repeat;                /* the source also clamps x to the size limit */
}
```

## Results

<!-- measurements:begin -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 1.21 ms | 17× | 1.12 ms | 2.2× | 213,711 bytes |
| Photograph, 2400×1800 | 15.6 ms | 20× | 12.4 ms | 2.7× | 2,787,156 bytes |
| Two colors, 2400×1800 | 4.84 ms | 0.7× | 3.77 ms | 1.2× | 1,441,842 bytes |
| Random noise, 2400×1800 | 339.1 ms | 1.7× | 71.3 ms | 5.2× | 19,399,499 bytes |
<!-- measurements:end -->

Classification now costs the same for 2 or 216 colors, and the photograph encodes about 20 times faster than in step 1, with a stream 8% smaller. The two-color pattern does not benefit, because step 1 already made only two passes there. The decoder is faster on every input.

Two costs remain, and both scale with something other than the pixels:

- **Composition** scans the full row of every color that occurs in the band, so it is proportional to width × colors present. For a dithered photograph most of those bytes are zero.
- **Decoding** writes up to six image rows for every data character. The rows are `width` bytes apart, so consecutive characters touch six cache lines instead of one.

[Step 3](step3-column-words.md) addresses both by changing how data is laid out in memory.
