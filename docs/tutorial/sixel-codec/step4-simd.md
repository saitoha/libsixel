# Step 4: SIMD

[Step 3](step3-column-words.md) processes eight bytes at a time inside a 64-bit integer. SIMD registers hold sixteen bytes and compare, mask and shuffle all of them in one instruction. This step uses them in the two places where SIXEL's layout is most regular: classifying the pixels of a block of columns, and turning column words back into rows. The source is [`code/step4.c`](code/step4.c); its output is byte-identical to steps 2 and 3.

The code uses only the baseline vector instruction sets of the two common 64-bit targets: SSE2 on x86-64 and Advanced SIMD (NEON) on AArch64. Both are always present on those targets, so no runtime detection is needed. Elsewhere, or when built with `-DTUTORIAL_NO_SIMD`, the step 3 code runs unchanged, and it also handles the columns left over at the right edge of each band.

## A few vector operations

The encoder needs only eight operations, wrapped in macros so the algorithm reads the same on both targets:

| Operation | SSE2 | NEON | Meaning |
| --- | --- | --- | --- |
| `v_load`, `v_store` | `_mm_loadu_si128`, `_mm_storeu_si128` | `vld1q_u8`, `vst1q_u8` | Move 16 bytes |
| `v_set1(c)` | `_mm_set1_epi8` | `vdupq_n_u8` | Copy one byte to all 16 lanes |
| `v_eq(a, b)` | `_mm_cmpeq_epi8` | `vceqq_u8` | `0xff` where lanes are equal, else `0x00` |
| `v_and`, `v_or`, `v_zero` | `_mm_and_si128`, `_mm_or_si128`, `_mm_setzero_si128` | `vandq_u8`, `vorrq_u8`, `vdupq_n_u8(0)` | Bitwise logic |
| `v_movemask(v)` | `_mm_movemask_epi8` | (see below) | One bit per lane, as a 16-bit integer |

NEON has no movemask instruction. For compare results, whose lanes are either `0x00` or `0xff`, the tutorial keeps a different power of two in each lane of each half and adds up the halves:

```c
static inline unsigned int
v_movemask(uint8x16_t v)
{
    static uint8_t const weights[16] = {
        1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128
    };
    uint8x16_t m = vandq_u8(v, vld1q_u8(weights));

    return (unsigned int)vaddv_u8(vget_low_u8(m)) |
           (unsigned int)vaddv_u8(vget_high_u8(m)) << 8;
}
```

## Encoder: one color against sixteen columns

For a block of sixteen columns, load the six rows of the band into six vectors. Comparing each row with a vector filled with color *c* marks the lanes where that row has the color; weighting the marks by the row bit and combining them gives the sixel characters of color *c* for all sixteen columns at once:

```text
row 0:  5 5 2 2 2 2 7 7 7 7 7 7 2 2 2 2      == 2 ->  . . 1 1 1 1 . . . . . . 1 1 1 1   x  1
row 1:  5 5 2 2 2 2 7 7 7 7 7 7 2 2 2 2      == 2 ->  . . 1 1 1 1 . . . . . . 1 1 1 1   x  2
  ...                                                                                     ...
row 5:  5 5 5 5 2 2 2 2 7 7 7 7 7 7 2 2      == 2 ->  . . . . 1 1 1 1 . . . . . . 1 1   x 32
                                                     OR of the weighted rows = sixel bits of color 2
```

```c
for (x = 0; x < end; x += 16) {
    e->stamp++;
    for (r = 0; r < rows; r++) {
        row[r] = v_load(src[r] + x);
    }
    for (r = 0; r < rows; r++) {
        for (lane = 0; lane < 16; lane++) {
            c = src[r][x + lane];
            if (e->seen[c] == e->stamp) {
                continue;
            }
            e->seen[c] = e->stamp;
            color = v_set1(c);
            bits = v_zero();
            for (k = r; k < rows; k++) {    /* rows above r lack c */
                bits = v_or(bits, v_and(v_eq(row[k], color), weight[k]));
            }
            v_store(e->map + (size_t)c * e->width + x, bits);
            occupied = ~v_movemask(v_eq(bits, v_zero())) & 0xffff;
            e->occupancy[(size_t)c * e->words + (size_t)x / 64] |=
                (uint64_t)occupied << (x % 64);
            e->used[c] = 1;
        }
    }
}
```

The scalar loop over the block's 96 pixels only finds the distinct colors; `seen[c] == stamp` marks colors already handled in this block, and incrementing `stamp` forgets them all for the next block without clearing anything. Each distinct color then costs up to six compares, one 16-byte store into the map and one movemask for the occupancy bitmap. Lanes without the color store zero, which is what the map already holds there, so the store needs no mask.

The cost is now proportional to the number of distinct colors per 16×6 block rather than per column. That number is small when neighboring pixels repeat colors, as in the enlarged photograph and the two-color pattern, and large in random noise, where a block can hold almost 96 colors. The measurements show both effects.

## Decoder: transposing bytes

Step 3's decoder ends by copying byte *r* of every column word to row *r* of the image. Eight column words are an 8×8 matrix of bytes, and this copy is its transpose. SIMD computes a transpose with three rounds of interleaving: bytes of pairs of columns, then 16-bit pairs, then 32-bit quads. Each round doubles the length of the runs of same-row bytes until each row is eight contiguous bytes:

```text
input    c0 = [r0 r1 r2 r3 r4 r5 0 0]  c1 = [...]  ...  c7 = [...]
round 1  c0.r0 c1.r0 c0.r1 c1.r1 ...                      (unpack bytes)
round 2  c0.r0 c1.r0 c2.r0 c3.r0  c0.r1 c1.r1 ...          (unpack 16-bit pairs)
round 3  c0.r0 ... c7.r0  c0.r1 ... c7.r1                 (unpack 32-bit quads)
```

```c
__m128i a0 = _mm_unpacklo_epi8(_mm_loadl_epi64((void const *)&col[0]),
                               _mm_loadl_epi64((void const *)&col[1]));
/* ... a1, a2, a3 from columns 2-3, 4-5 and 6-7 ... */
__m128i b0 = _mm_unpacklo_epi16(a0, a1);
__m128i b1 = _mm_unpackhi_epi16(a0, a1);
/* ... b2, b3 from a2 and a3 ... */
out[0] = _mm_unpacklo_epi32(b0, b2);    /* rows 0 and 1 */
out[1] = _mm_unpackhi_epi32(b0, b2);    /* rows 2 and 3 */
out[2] = _mm_unpacklo_epi32(b1, b3);    /* rows 4 and 5 */
```

The NEON version is the same network with `vzip1q` and `vzip2q`. Each column word is read once instead of once per row, and each row receives one 8-byte store instead of eight byte stores.

## Results

<!-- measurements:begin -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 1.04 ms | 1.2× | 0.60 ms | 1.2× | 213,711 bytes |
| Photograph, 2400×1800 | 12.7 ms | 1.2× | 7.04 ms | 1.1× | 2,787,156 bytes |
| Two colors, 2400×1800 | 5.76 ms | 1.4× | 3.44 ms | 1.2× | 1,441,842 bytes |
| Random noise, 2400×1800 | 161.8 ms | 0.9× | 55.2 ms | 1.0× | 19,399,499 bytes |
<!-- measurements:end -->

The encoder gains on both photographs and most on the two-color pattern, where each block holds only two colors; on noise it loses a little, because distinct colors per block approach the number of pixels. The transpose makes materialization about 3.4 times faster, 0.38 ms instead of 1.3 ms for 2400×1800 pixels, but materialization was only a sixth of step 3's decoding time, so the decoder as a whole gains 10–20%.

Neither direction is limited by these loops any more. For the enlarged photograph, materialization now takes less than 0.4 ms of the 7.6 ms decode, and emitting the stream takes about 60% of the encode. Emitting and parsing, one byte at a time with a branch per byte, are essentially the same work in steps 2 to 4. The remaining way to go faster is to do it on several cores, which is [step 5](step5-threads.md).

libsixel uses SIMD in [color-space conversion](../../encoder/colorspace-handling/README.md) and [resampling](../../encoder/preprocessing/resampling/README.md); its SIXEL band encoder and decoder use the 64-bit word techniques of step 3.
