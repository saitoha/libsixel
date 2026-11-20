# Turn Columns Back into Rows

The [word decoder](word-decoder.md) ends by copying byte *r* of every column word into row *r* of the picture, one byte at a time. Look at eight column words as a square of 8×8 bytes: the copy turns its columns into rows. This is called a **transpose**, and the vector registers of the [previous chapter](vector-lanes.md) do it with a few shuffles. The encoder is the [vector-lanes](vector-lanes.md) chapter's. The source is [`code/transpose.c`](code/transpose.c).

**You will learn:**

- how interleaving two vectors works, like the teeth of a zipper,
- how three rounds of interleaving transpose an 8×8 block of bytes,
- why a large speed-up of one pass can be a small speed-up of the whole program.

## A zipper in three rounds

SIMD transposes with three rounds of *zipping*, taking pieces from two vectors in turn like the teeth of a zipper:

![Eight column words of eight bytes are transposed in three rounds of interleaving, bytes then pairs then quads, until each row is eight contiguous bytes](figures/transpose-zip.svg)

Round 1 zips single bytes of two columns, so the bytes of each row come in pairs. Round 2 zips those pairs, giving groups of four. Round 3 zips the groups, and now each row's eight bytes sit side by side, ready to be stored with one instruction:

```c
__m128i a0 = _mm_unpacklo_epi8(LOAD8(0), LOAD8(1));    /* columns 0 and 1 */
/* ... a1, a2, a3 from columns 2-3, 4-5 and 6-7 ... */
__m128i b0 = _mm_unpacklo_epi16(a0, a1), b1 = _mm_unpackhi_epi16(a0, a1);
/* ... b2, b3 from a2 and a3 ... */
__m128i out[3] = {
    _mm_unpacklo_epi32(b0, b2),     /* rows 0 and 1 */
    _mm_unpackhi_epi32(b0, b2),     /* rows 2 and 3 */
    _mm_unpacklo_epi32(b1, b3),     /* rows 4 and 5 */
};
```

The NEON version is the same zipper with `vzip1q` and `vzip2q`. Each column word is read once instead of once per row, and each row receives one 8-byte store instead of eight single-byte stores. The columns left over at the right edge, fewer than eight, still take the scalar path.

## Where this idea came from

Transposing 8×8 blocks of bytes with interleaving instructions is a standard SIMD technique, used for example in image and video codecs. Which SIXEL decoder first used it is not known; the earliest SIMD code found in a SIXEL decoder is icy_sixel's SSE2 span filling of 2024, listed in the [implementation history](../../sixel-format/implementation-history.md#encoders-in-other-languages).

> **Try it.** Build this chapter with `-DTUTORIAL_NO_SIMD` and time only the pass back to rows, for example by printing the time around `materialize()`. How close is the speed-up of that pass to eight, the number of bytes in a row of the block?

## How fast is it?

<!-- measurements:begin:transpose -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 2.41 ms | 1.0× | 0.56 ms | 1.1× | 328,750 bytes |
| Photograph, 1920×1080 | 10.9 ms | 1.0× | 2.71 ms | 1.1× | 1,663,155 bytes |
| Two colors, 1920×1080 | 2.75 ms | 1.0× | 0.72 ms | 1.6× | 692,322 bytes |
| Random noise, 1920×1080 | 73.8 ms | 1.0× | 15.2 ms | 1.1× | 9,316,560 bytes |
<!-- measurements:end -->

The transpose makes the pass back to rows about 3.4 times faster, 0.17 ms instead of 0.57 ms for a Full HD picture. But that pass was less than a fifth of decoding the photograph, and the parsing around it is unchanged, so the whole decoder gains only about 1.1 times there. The two-color pattern, whose parsing is cheap, gains 1.6 times.

libsixel's own [vector band-word paths](../../decoder/pipeline/README.md#vector-band-word-paths) materialize sixteen column words at a time with table lookups and keep ordinary literal runs scalar, because those runs are short and the scalar store is already cheap.

What remains is work done one byte after another on one core. The next two chapters bring in the others.

Previous: [Sixteen columns in one instruction](vector-lanes.md) · Next: [Encode bands in parallel](parallel-bands.md)
