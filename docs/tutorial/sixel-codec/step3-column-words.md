# Step 3: Six Pixels in a 64-bit Word

A sixel character describes one column of a band: six pixels stacked vertically. In memory, however, an image is stored row by row, so those six pixels are `width` bytes apart. This step makes the column the unit of work by packing the six palette indexes of a column into one 64-bit integer, a *column word*, and then uses word arithmetic to process all six at once. It applies the same idea twice more in the encoder, with words that hold one bit per column. The source is [`code/step3.c`](code/step3.c); its output is byte-identical to step 2.

```text
byte:    7   6    5    4    3    2    1    0
word:  [ 0 | 0 | r5 | r4 | r3 | r2 | r1 | r0 ]     rN = palette index of row N
```

This is the representation used by libsixel's decoder, described under [ordinary-mode band columns](../../decoder/pipeline/README.md#ordinary-mode-band-columns), and by its encoder for one- and two-color palettes.

## Encoder

### Column words and SWAR

The encoder first transposes the six rows of the band into one word per column. Accumulating a whole row at a time, `columns[x] |= row[x] << 8 * r`, keeps the loops sequential so that the compiler vectorizes them.

The question the encoder has to answer for each column is "which rows have color *c*?". With the column in one word, the answer takes a handful of integer operations on all eight bytes in parallel, a technique called SWAR (SIMD within a register):

```c
/* Bit 7 of each byte of the result is set when that byte of v is zero:
 * adding 0x7f to the low seven bits carries into bit 7 unless they are
 * all zero, and the byte's own bit 7 must be clear as well. */
static inline uint64_t
zero_bytes(uint64_t v)
{
    return ~(((v & BYTES_7F) + BYTES_7F) | v | BYTES_7F);
}

/* Bit i of the result is set when byte i of word equals c. The multiply
 * gathers the eight byte flags into the top byte. */
static inline unsigned int
equal_bytes(uint64_t word, unsigned int c)
{
    uint64_t zero = zero_bytes(word ^ (c * BYTES_01));

    return (unsigned int)((((zero >> 7) & BYTES_01) *
                           UINT64_C(0x0102040810204080)) >> 56);
}
```

Multiplying `c` by `0x0101010101010101` copies it into every byte, and the exclusive or turns equal bytes into zero bytes. `zero_bytes()` cannot carry from one byte into the next, because the low seven bits plus `0x7f` never exceed `0xfe`. The final multiply moves the flag of byte *i* to bit 56 + *i* without collisions, so the top byte is exactly the six-bit sixel value. For example, a column whose rows hold the indexes 5, 2, 2, 5, 7, 2 gives `equal_bytes(word, 2) = 0b100110`.

Each column must be visited once per distinct color in it. A byte names a new color when no lower byte holds the same index, and comparing the word with copies of itself shifted by one to five bytes finds all repeated bytes at once:

```c
for (x = 0; x < e->width; x++) {
    word = e->columns[x];
    repeat = 0;
    for (k = 1; k < rows; k++) {
        repeat |= zero_bytes(word ^ (word << (8 * k))) &
            (~UINT64_C(0) << (8 * k));
    }
    for (firsts = lanes & ~repeat; firsts != 0; firsts &= firsts - 1) {
        c = (unsigned int)(word >> (ctz64(firsts) & ~7)) & 0xff;
        bits = equal_bytes(word, c) & ((1u << rows) - 1);
        e->map[(size_t)c * e->width + x] = (unsigned char)bits;
        e->occupancy[c * e->words + (size_t)x / 64] |=
            UINT64_C(1) << (x % 64);
        e->used[c] = 1;
    }
}
```

`firsts` marks the first occurrence of every color in the column. The iterations of the inner loop do not depend on each other, so a superscalar CPU can overlap them. Finding the colors one after another instead, each search waiting for the previous color's rows to be removed, would form one long chain of dependent operations per column. Each (color, column) pair costs one map store, not one store per pixel.

### Bitmaps of occupied columns

The same loop sets bit *x* of an *occupancy* bitmap for color *c*: 64 columns per word. Composition no longer scans each color's full row for nonzero bytes. It walks the set bits with count-trailing-zeros, so its cost follows the occupied columns, and clears each word as it goes:

```c
for (i = 0; i < e->words; i++) {
    for (bits = occ[i]; bits != 0; bits &= bits - 1) {
        x = (int)(i * 64) + ctz64(bits);
        /* extend the current node or start a new one, as in step 2 */
    }
    occ[i] = 0;
}
```

### A bitmap of node starts

Step 2 walks the whole remaining node list for every graphics line and skips nodes that overlap the cursor. Step 3 keeps the nodes in their per-column buckets and marks non-empty buckets in another bitmap. The next node for a line is the longest node in the first non-empty bucket at or after the cursor, which one bit scan finds:

```c
static int
next_start(encoder_t const *e, int x)
{
    size_t i = (size_t)x / 64;
    uint64_t bits;

    if (x >= e->width) {
        return -1;
    }
    bits = e->starts[i] & (~UINT64_C(0) << (x % 64));
    while (bits == 0) {
        if (++i == e->words) {
            return -1;
        }
        bits = e->starts[i];
    }
    return (int)(i * 64) + ctz64(bits);
}
```

This chooses exactly the nodes that step 2's list walk would choose, in the same order, so the output does not change.

## Decoder

The decoder stores each band as an array of column words. A data character with bits `b` and a selected color `c` replaces the bytes of the set rows and keeps the others, which is one masked store:

```c
mask = row_mask((unsigned int)bits);    /* 0xff in every selected byte */
paint = color6 & mask;                  /* color6 = c * 0x010101010101 */
col = d.columns + (size_t)band * d.width + x;   /* band = y / 6 */
for (i = 0; i < repeat; i++) {
    col[i] = (col[i] & ~mask) | paint;
}
```

`row_mask()` is the inverse of the gather above. Multiplying the six bits by `0x0002040810204081` places a copy of bit *i* at bit 8*i*, the copies being seven bits apart so that they never overlap; masking with `0x0101010101010101` keeps those bits, and multiplying by `0xff` widens each to a full byte:

```c
static inline uint64_t
row_mask(unsigned int bits)
{
    return ((bits * UINT64_C(0x0002040810204081)) & BYTES_01) * 0xff;
}
```

Consecutive characters now update consecutive words of one array instead of six rows of the image. After the stream ends, `materialize()` converts the column words back to rows, once per pixel.

Unpainted bytes stay zero, which is also the background index, so the tutorial needs no record of which pixels were painted. libsixel's decoder keeps a coverage bit per row in byte 6 of the word, because its callers can ask for a paint mask and because register 0 painted must remain distinguishable from an untouched pixel.

## Results

<!-- measurements:begin -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 1.20 ms | 1.0× | 0.71 ms | 1.6× | 213,711 bytes |
| Photograph, 2400×1800 | 15.3 ms | 1.0× | 7.95 ms | 1.6× | 2,787,156 bytes |
| Two colors, 2400×1800 | 8.27 ms | 0.6× | 4.24 ms | 0.9× | 1,441,842 bytes |
| Random noise, 2400×1800 | 150.7 ms | 2.2× | 56.2 ms | 1.3× | 19,399,499 bytes |
<!-- measurements:end -->

The decoder is about 1.6 times faster on the photographs and 1.3 times on noise. On the two-color pattern it is slightly slower: step 2 paints only two lines per band there, and the extra pass that turns column words back into rows, about 1.3 ms for 2400×1800 pixels, costs more than the scattered stores save. [Step 4](step4-simd.md) makes that pass cheaper.

The encoder's result depends on the image, which is the main lesson of this step. Composition becomes more than twice as fast, and on random noise, where every band uses all 256 colors in scattered columns, the encoder is more than twice as fast as step 2. Classification, however, now costs a few dozen operations for each distinct color in each column, against step 2's six independent byte stores per column. On the two-color pattern, where step 2's composition was already cheap, step 3 is slower overall.

libsixel faces the same trade-off and resolves it differently: its [`sixel_band_classify()`](../../../src/encoder-core-encode.c) uses column words only for one- and two-color palettes, and per-pixel row writes with occupancy updates otherwise. Which side wins depends on the rest of the pipeline, which is why every optimization in this tutorial is measured rather than assumed.
