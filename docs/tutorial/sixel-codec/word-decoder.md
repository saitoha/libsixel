# Decode into Column Words

The [masks](masks.md) decoder paints a character with six stores, one per row, each a whole row of the picture away from the last. The encoder chapters showed how to keep a column in one word. This chapter gives the decoder the same layout: it paints each character with one masked store into a column word and turns columns back into rows once at the end. The encoder is the [occupancy](occupancy.md) chapter's. The source is [`code/word-decoder.c`](code/word-decoder.c).

**You will learn:**

- how a mask of whole bytes replaces six byte stores,
- how six bits spread out into six bytes with one multiplication,
- what the extra pass at the end costs.

## One store per character

The decoder keeps each band as an array of column words. A sixel character with color *c* must replace the bytes of the rows it paints and keep the others. With a **mask**, a number that is `0xff` in the bytes to replace and `0x00` elsewhere, that is a single operation:

![The data character e sets rows 1, 2 and 5. A mask with ff in those bytes replaces them with color 3 and keeps the other rows](figures/word-decoder-masked-store.svg)

```c
mask = row_mask((unsigned int)bits);    /* 0xff in every painted row */
paint = color6 & mask;                  /* color6 = c in all six bytes */
col = d.columns + (size_t)band * d.width + x;
for (int i = 0; i < repeat; i++)
    col[i] = (col[i] & ~mask) | paint;
```

`col & ~mask` clears the painted rows, and `| paint` fills them with the color. A repeat simply does the same for the next words, which lie side by side in memory. `row_mask()` is the gathering trick of the [SWAR](swar.md) chapter in reverse: it spreads six bits out to six bytes.

```c
static inline uint64_t row_mask(unsigned int bits)
{
    return ((bits * UINT64_C(0x0002040810204081)) & BYTES_01) * 0xff;
}
```

The inner loop of the [fewer questions](fewer-questions.md) chapter stays, with this one store in place of six.

## Back to rows at the end

When the stream ends, `materialize()` copies byte *r* of every column word into row *r* of the picture, once per pixel. The [transpose](transpose.md) chapter shows how to do that quickly.

Unpainted bytes stay zero, which is also register 0, so the tutorial needs no record of which pixels were painted. libsixel's decoder keeps such a record in byte 6 of the word, as described under [ordinary-mode band columns](../../decoder/pipeline/README.md#ordinary-mode-band-columns), because its callers can ask which pixels were painted and because a pixel painted with register 0 must look different from an untouched one.

## Where this idea came from

Tetsuya Isaki's SIXEL support for the NetBSD/x68k `ite` console replaced six row arrays with one 32-bit word per column in 2016, updated with one table lookup, one multiplication and one OR. That is the earliest decoder found that paints into column words. The [implementation history](../../sixel-format/implementation-history.md#decoder-side-transposition-in-ite) and the [OR-mode history](../../encoder/output/or-mode/x68k.md#history) give the details.

> **Try it.** Write `row_mask()` yourself and check it on the figure: `row_mask(38)` must be `0x0000ff0000ffff00`. Then try all 64 values against a plain loop over the six bits.

## How fast is it?

<!-- measurements:begin:word-decoder -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 2.30 ms | 1.0× | 0.61 ms | 1.2× | 328,750 bytes |
| Photograph, 1920×1080 | 11.8 ms | 1.0× | 3.09 ms | 1.3× | 1,663,155 bytes |
| Two colors, 1920×1080 | 3.91 ms | 1.0× | 1.14 ms | 1.3× | 692,322 bytes |
| Random noise, 1920×1080 | 68.4 ms | 1.0× | 15.8 ms | 1.2× | 9,316,560 bytes |
<!-- measurements:end -->

One store instead of six makes decoding about 1.3 times faster on the photograph and on the two-color pattern, including the pass back to rows, which takes about 0.6 ms for a Full HD picture. The encoder has not changed.

Previous: [Remember where each color is](occupancy.md) · Next: [Sixteen columns in one instruction](vector-lanes.md)
