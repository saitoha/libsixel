/* SPDX-License-Identifier: MIT */
/*
 * Step 4: SIMD.
 *
 * Step 3 works on one 64-bit word at a time. A 128-bit SIMD register holds
 * sixteen bytes, so it can look at sixteen columns of a row, or sixteen
 * bytes of two column words, in one instruction. This step uses only the
 * baseline vector sets of the two common 64-bit targets, SSE2 on x86-64
 * and Advanced SIMD (NEON) on AArch64, and keeps the step 3 code as the
 * portable fallback and for the columns left over at the right edge.
 * Define TUTORIAL_NO_SIMD to build the fallback alone.
 *
 * Encoder: for a block of 16 columns, one compare tests a color against a
 * whole row, so six compares produce the sixel characters of that color
 * for all 16 columns. The block is visited once per distinct color in it,
 * not once per pixel, which pays off whenever neighboring pixels repeat.
 *
 * Decoder: turning column words back into rows is an 8x8 byte transpose,
 * three rounds of interleaving instructions for eight columns at a time.
 *
 * The output is still byte-identical to steps 2 and 3.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

#define NODE_GAP 9
#define BYTES_01 UINT64_C(0x0101010101010101)
#define BYTES_7F UINT64_C(0x7f7f7f7f7f7f7f7f)

static inline int
ctz64(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(v);
#else
    int n = 0;

    while ((v & 1) == 0) {
        v >>= 1;
        n++;
    }
    return n;
#endif
}

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

/* Inverse direction: expand six row bits to a mask of 0xff bytes. */
static inline uint64_t
row_mask(unsigned int bits)
{
    return ((bits * UINT64_C(0x0002040810204081)) & BYTES_01) * 0xff;
}

/* ------------------------------------------------------------------ */
/* The few vector operations this step needs                           */

#if defined(TUTORIAL_NO_SIMD)
/* scalar build for comparison: the step 3 code paths only */
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define HAVE_SIMD 1
typedef __m128i v128;
#define v_load(p)     _mm_loadu_si128((__m128i const *)(void const *)(p))
#define v_store(p, v) _mm_storeu_si128((__m128i *)(void *)(p), (v))
#define v_set1(c)     _mm_set1_epi8((char)(c))
#define v_eq(a, b)    _mm_cmpeq_epi8((a), (b))
#define v_and(a, b)   _mm_and_si128((a), (b))
#define v_or(a, b)    _mm_or_si128((a), (b))
#define v_zero()      _mm_setzero_si128()
/* One bit per byte lane, taken from the lane's top bit. */
#define v_movemask(v) ((unsigned int)_mm_movemask_epi8(v))

/* Rows r0..r5 of eight columns from eight column words: interleave bytes,
 * then 16-bit pairs, then 32-bit quads. Each round doubles the run of
 * same-row bytes until a row is eight bytes long. */
static inline void
transpose_columns(uint64_t const *col, unsigned char **rows, int nrows)
{
    __m128i a0 = _mm_unpacklo_epi8(_mm_loadl_epi64((void const *)&col[0]),
                                   _mm_loadl_epi64((void const *)&col[1]));
    __m128i a1 = _mm_unpacklo_epi8(_mm_loadl_epi64((void const *)&col[2]),
                                   _mm_loadl_epi64((void const *)&col[3]));
    __m128i a2 = _mm_unpacklo_epi8(_mm_loadl_epi64((void const *)&col[4]),
                                   _mm_loadl_epi64((void const *)&col[5]));
    __m128i a3 = _mm_unpacklo_epi8(_mm_loadl_epi64((void const *)&col[6]),
                                   _mm_loadl_epi64((void const *)&col[7]));
    __m128i b0 = _mm_unpacklo_epi16(a0, a1);
    __m128i b1 = _mm_unpackhi_epi16(a0, a1);
    __m128i b2 = _mm_unpacklo_epi16(a2, a3);
    __m128i b3 = _mm_unpackhi_epi16(a2, a3);
    __m128i out[3];
    int r;

    out[0] = _mm_unpacklo_epi32(b0, b2);    /* rows 0 and 1 */
    out[1] = _mm_unpackhi_epi32(b0, b2);    /* rows 2 and 3 */
    out[2] = _mm_unpacklo_epi32(b1, b3);    /* rows 4 and 5 */
    for (r = 0; r < nrows; r++) {
        __m128i v = (r & 1) ? _mm_unpackhi_epi64(out[r / 2], out[r / 2])
                            : out[r / 2];
        _mm_storel_epi64((void *)rows[r], v);
    }
}

#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define HAVE_SIMD 1
typedef uint8x16_t v128;
#define v_load(p)     vld1q_u8((uint8_t const *)(p))
#define v_store(p, v) vst1q_u8((uint8_t *)(p), (v))
#define v_set1(c)     vdupq_n_u8((uint8_t)(c))
#define v_eq(a, b)    vceqq_u8((a), (b))
#define v_and(a, b)   vandq_u8((a), (b))
#define v_or(a, b)    vorrq_u8((a), (b))
#define v_zero()      vdupq_n_u8(0)

/* NEON has no movemask. For compare results, whose lanes are 0x00 or
 * 0xff, keep each lane's own bit weight and add up each half. */
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

static inline void
transpose_columns(uint64_t const *col, unsigned char **rows, int nrows)
{
    uint8x16_t x[8];
    uint8x16_t a[4];
    uint8x16_t b[4];
    uint8x16_t out[3];
    int i;
    int r;

    for (i = 0; i < 8; i++) {
        x[i] = vreinterpretq_u8_u64(vdupq_n_u64(col[i]));
    }
    for (i = 0; i < 4; i++) {
        a[i] = vzip1q_u8(x[2 * i], x[2 * i + 1]);
    }
    b[0] = vreinterpretq_u8_u16(vzip1q_u16(vreinterpretq_u16_u8(a[0]),
                                           vreinterpretq_u16_u8(a[1])));
    b[1] = vreinterpretq_u8_u16(vzip2q_u16(vreinterpretq_u16_u8(a[0]),
                                           vreinterpretq_u16_u8(a[1])));
    b[2] = vreinterpretq_u8_u16(vzip1q_u16(vreinterpretq_u16_u8(a[2]),
                                           vreinterpretq_u16_u8(a[3])));
    b[3] = vreinterpretq_u8_u16(vzip2q_u16(vreinterpretq_u16_u8(a[2]),
                                           vreinterpretq_u16_u8(a[3])));
    out[0] = vreinterpretq_u8_u32(vzip1q_u32(vreinterpretq_u32_u8(b[0]),
                                             vreinterpretq_u32_u8(b[2])));
    out[1] = vreinterpretq_u8_u32(vzip2q_u32(vreinterpretq_u32_u8(b[0]),
                                             vreinterpretq_u32_u8(b[2])));
    out[2] = vreinterpretq_u8_u32(vzip1q_u32(vreinterpretq_u32_u8(b[1]),
                                             vreinterpretq_u32_u8(b[3])));
    for (r = 0; r < nrows; r++) {
        vst1_u8(rows[r], (r & 1) ? vget_high_u8(out[r / 2])
                                 : vget_low_u8(out[r / 2]));
    }
}
#endif

/* ------------------------------------------------------------------ */
/* Encoder                                                             */

typedef struct node {
    struct node *next;
    int color;
    int sx;
    int mx;
    unsigned char *map;
} node_t;

typedef struct encoder {
    int width;
    size_t words;               /* 64-bit words per occupancy row */
    unsigned char *map;         /* ncolors rows of sixel bits */
    uint64_t *occupancy;        /* ncolors rows of column bits */
    uint64_t *columns;          /* one column word per x */
    unsigned char used[256];    /* colors present in the band */
    unsigned int seen[256];     /* block stamp of the last visit */
    unsigned int stamp;
    uint64_t *starts;           /* columns where pending nodes start */
    node_t *pool;
    node_t **heads;             /* pending nodes by start, longest first */
    int nnodes;
} encoder_t;

static void
encoder_free(encoder_t *e)
{
    free(e->map);
    free(e->occupancy);
    free(e->columns);
    free(e->starts);
    free(e->pool);
    free(e->heads);
}

static int
encoder_init(encoder_t *e, image_t const *img)
{
    int w = img->width;

    memset(e, 0, sizeof(*e));
    e->width = w;
    e->words = ((size_t)w + 63) / 64;
    e->map = calloc((size_t)img->ncolors * w, 1);
    e->occupancy = calloc((size_t)img->ncolors * e->words, sizeof(uint64_t));
    e->columns = malloc((size_t)w * sizeof(uint64_t));
    e->pool = malloc(sizeof(node_t) * (size_t)img->ncolors *
                     (w / (NODE_GAP + 1) + 1));
    e->heads = calloc((size_t)w, sizeof(node_t *));
    e->starts = calloc(e->words, sizeof(uint64_t));
    if (e->map == NULL || e->occupancy == NULL || e->columns == NULL ||
            e->pool == NULL || e->heads == NULL || e->starts == NULL) {
        encoder_free(e);
        return -1;
    }
    return 0;
}

/* Transpose the band's rows into column words. Accumulating a whole row
 * at a time keeps every loop a sequential pass that compilers vectorize. */
static void
load_columns(encoder_t *e, image_t const *img, int y, int rows, int from)
{
    unsigned char const *row;
    int x;
    int r;

    memset(e->columns + from, 0, (size_t)(e->width - from) * sizeof(uint64_t));
    for (r = 0; r < rows; r++) {
        row = img->pixels + (size_t)(y + r) * e->width;
        for (x = from; x < e->width; x++) {
            e->columns[x] |= (uint64_t)row[x] << (8 * r);
        }
    }
}

/* Visit every color of every column once. A byte is the first occurrence
 * of its color when no lower byte holds the same register; comparing the
 * word with itself shifted by one to five bytes finds all repeats at once.
 * Each first occurrence then names a color, and equal_bytes() finds all its
 * rows. The iterations are independent, so the CPU can overlap them. */
static void
classify(encoder_t *e, int rows, int from)
{
    uint64_t lanes = UINT64_C(0x8080808080808080) >> (8 * (8 - rows));
    uint64_t word;
    uint64_t repeat;
    uint64_t firsts;
    unsigned int c;
    unsigned int bits;
    int k;
    int x;

    for (x = from; x < e->width; x++) {
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
}

#ifdef HAVE_SIMD
/* Classify columns [0, end) in blocks of 16, end being a multiple of 16.
 * Scanning the block's pixels finds each distinct color once (the stamp
 * marks colors already done in this block); six vector compares then give
 * the sixel characters of that color for all 16 columns, stored with one
 * 16-byte write. Lanes without the color store zero, which the map already
 * holds there. */
static void
classify_blocks(encoder_t *e, image_t const *img, int y, int rows, int end)
{
    unsigned char const *src[6];
    v128 row[6];
    v128 weight[6];
    v128 bits;
    v128 color;
    unsigned int occupied;
    int lane;
    int x;
    int r;
    int k;
    int c;

    for (r = 0; r < rows; r++) {
        src[r] = img->pixels + (size_t)(y + r) * e->width;
        weight[r] = v_set1(1 << r);
    }
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
}
#endif

static void
add_node(encoder_t *e, int c, int sx, int mx)
{
    node_t *np = &e->pool[e->nnodes++];
    node_t **link;

    np->color = c;
    np->sx = sx;
    np->mx = mx;
    np->map = e->map + (size_t)c * e->width;
    for (link = &e->heads[sx]; *link && (*link)->mx >= mx;
         link = &(*link)->next) {
    }
    np->next = *link;
    *link = np;
    e->starts[sx / 64] |= UINT64_C(1) << (sx % 64);
}

/* Same nodes as step 2, found by walking set occupancy bits. The walk also
 * clears the occupancy bits and used flags for the next band. */
static void
compose(encoder_t *e)
{
    uint64_t *occ;
    uint64_t bits;
    size_t i;
    int c;
    int x;
    int sx;
    int mx;

    e->nnodes = 0;
    for (c = 0; c < 256; c++) {
        if (e->used[c]) {
            e->used[c] = 0;
            occ = e->occupancy + (size_t)c * e->words;
            sx = -1;
            mx = 0;
            for (i = 0; i < e->words; i++) {
                for (bits = occ[i]; bits != 0; bits &= bits - 1) {
                    x = (int)(i * 64) + ctz64(bits);
                    if (sx >= 0 && x - mx <= NODE_GAP) {
                        mx = x + 1;
                        continue;
                    }
                    if (sx >= 0) {
                        add_node(e, c, sx, mx);
                    }
                    sx = x;
                    mx = x + 1;
                }
                occ[i] = 0;
            }
            add_node(e, c, sx, mx);
        }
    }
}

/* The first pending start column at or after x, or -1. */
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

static void
put_span(buffer_t *out, unsigned char const *map, int from, int to)
{
    int x;
    int run;

    for (x = from; x < to; x += run) {
        for (run = 1; x + run < to && map[x + run] == map[x]; run++) {
        }
        buf_putrun(out, map[x] + '?', run);
    }
}

/* Step 2 walks the node list once per graphics line and skips the nodes
 * that start left of the cursor. Taking the leftmost pending start column
 * at or after the cursor, and the longest node there, picks the same nodes
 * in the same order without visiting the skipped ones. */
static void
put_band(encoder_t *e, buffer_t *out)
{
    node_t *np;
    int remaining = e->nnodes;
    int color = -1;         /* every band selects its first color */
    int x = 0;
    int sx;

    while (remaining > 0) {
        sx = next_start(e, x);
        if (sx < 0) {
            buf_putc(out, '$');
            x = 0;
            continue;
        }
        np = e->heads[sx];
        e->heads[sx] = np->next;
        if (np->next == NULL) {
            e->starts[sx / 64] &= ~(UINT64_C(1) << (sx % 64));
        }
        if (np->color != color) {
            buf_putc(out, '#');
            buf_putnum(out, np->color);
            color = np->color;
        }
        buf_putrun(out, '?', np->sx - x);
        put_span(out, np->map, np->sx, np->mx);
        memset(np->map + np->sx, 0, (size_t)(np->mx - np->sx));
        x = np->mx;
        remaining--;
    }
}

static void
encode_band(encoder_t *e, image_t const *img, int y, buffer_t *out)
{
    int rows = img->height - y < 6 ? img->height - y : 6;
    int from = 0;

    if (y > 0) {
        buf_putc(out, '-');
    }
#ifdef HAVE_SIMD
    from = e->width & ~15;
    classify_blocks(e, img, y, rows, from);
#endif
    load_columns(e, img, y, rows, from);
    classify(e, rows, from);
    compose(e);
    put_band(e, out);
}

#ifndef TUTORIAL_STEP5
int
tutorial_encode(image_t const *img, buffer_t *out)
{
    encoder_t e;
    int y;

    if (encoder_init(&e, img) != 0) {
        return -1;
    }
    put_header_and_palette(img, out);
    for (y = 0; y < img->height; y += 6) {
        encode_band(&e, img, y, out);
    }
    buf_printf(out, "\033\\");
    encoder_free(&e);
    return 0;
}

#endif /* TUTORIAL_STEP5 */

/* ------------------------------------------------------------------ */
/* Decoder                                                             */

static inline int
bottom_row(int bits)
{
    int r = 5;

    while ((bits >> r & 1) == 0) {
        r--;
    }
    return r;
}

typedef struct decoder {
    uint64_t *columns;      /* bands * width column words */
    int width;              /* allocated columns per band */
    int bands;              /* allocated bands */
    int used_width;         /* declared or painted extent */
    int used_height;
} decoder_t;

/* Grow like step 2, moving whole column words. */
static int
decoder_reserve(decoder_t *d, int need_w, int need_bands)
{
    uint64_t *columns;
    int w = d->width;
    int bands = d->bands;
    int b;

    if (need_w <= w && need_bands <= bands) {
        return 0;
    }
    if (need_w > TUTORIAL_MAX_SIDE || need_bands > TUTORIAL_MAX_SIDE / 6) {
        return -1;
    }
    while (w < need_w) {
        w = w < 64 ? 64 : w * 2;
    }
    while (bands < need_bands) {
        bands = bands < 16 ? 16 : bands * 2;
    }
    w = w > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : w;
    bands = bands > TUTORIAL_MAX_SIDE / 6 ? TUTORIAL_MAX_SIDE / 6 : bands;
    columns = calloc((size_t)w * bands, sizeof(uint64_t));
    if (columns == NULL) {
        return -1;
    }
    for (b = 0; b < d->bands; b++) {
        memcpy(columns + (size_t)b * w, d->columns + (size_t)b * d->width,
               (size_t)d->width * sizeof(uint64_t));
    }
    free(d->columns);
    d->columns = columns;
    d->width = w;
    d->bands = bands;
    return 0;
}

/* Copy row r of every column word into the image: the transpose back,
 * eight columns at a time when SIMD is available. */
static void
materialize(decoder_t const *d, image_t *img, int first_band, int end_band)
{
    unsigned char *rows[6];
    int nrows;
    int b;
    int r;
    int x;

    for (b = first_band; b < end_band; b++) {
        uint64_t const *col = d->columns + (size_t)b * d->width;

        nrows = img->height - b * 6 < 6 ? img->height - b * 6 : 6;
        for (r = 0; r < nrows; r++) {
            rows[r] = img->pixels + (size_t)(b * 6 + r) * img->width;
        }
        x = 0;
#ifdef HAVE_SIMD
        for (; x + 8 <= img->width; x += 8) {
            transpose_columns(col + x, rows, nrows);
            for (r = 0; r < nrows; r++) {
                rows[r] += 8;
            }
        }
#endif
        for (r = 0; r < nrows; r++) {
            int i;

            for (i = x; i < img->width; i++) {
                rows[r][i - x] = (unsigned char)(col[i] >> (8 * r));
            }
        }
    }
}

static int
decode_serial(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    decoder_t d = { NULL, 0, 0, 0, 0 };
    uint64_t *col;
    uint64_t color6 = 0;
    uint64_t mask;
    uint64_t paint;
    int a[5];
    int n;
    int x = 0;
    int y = 0;
    int band = 0;           /* y / 6 */
    int band_rows = 0;      /* rows painted in this band */
    int i;
    int color = 0;
    int repeat;
    int bits;
    int ch;

    img->ncolors = 0;
    while (p < end && *p++ != 'q') {
    }
    while (p < end && *p != 0x1b) {
        ch = *p++;
        repeat = 1;
        if (ch == '!') {
            parse_params(&p, end, &repeat, 1);
            repeat = repeat > 0 ? repeat : 1;
            if (p == end) {
                break;
            }
            ch = *p++;
        }
        if (ch >= '?' && ch <= '~') {
            bits = ch - '?';
            if (bits != 0) {
                if (x + repeat > d.width || band >= d.bands) {
                    if (x + repeat > TUTORIAL_MAX_SIDE ||
                            decoder_reserve(&d, x + repeat, band + 1) != 0) {
                        goto fail;
                    }
                }
                mask = row_mask((unsigned int)bits);
                paint = color6 & mask;
                col = d.columns + (size_t)band * d.width + x;
                for (i = 0; i < repeat; i++) {
                    col[i] = (col[i] & ~mask) | paint;
                }
                if (x + repeat > d.used_width) {
                    d.used_width = x + repeat;
                }
                band_rows |= bits;
            }
            x = x + repeat > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : x + repeat;
        } else if (ch == '$') {
            x = 0;
        } else if (ch == '-') {
            if (band_rows != 0 && y + bottom_row(band_rows) >= d.used_height) {
                d.used_height = y + bottom_row(band_rows) + 1;
            }
            band_rows = 0;
            x = 0;
            y = y < TUTORIAL_MAX_SIDE ? y + 6 : y;
            band = y / 6;
        } else if (ch == '#') {
            n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            color6 = (uint64_t)color * UINT64_C(0x010101010101);
            if (color >= img->ncolors) {
                img->ncolors = color + 1;
            }
            if (n == 5 && a[1] == 1) {
                set_hls(img, color, a[2], a[3], a[4]);
            } else if (n == 5 && a[1] == 2) {
                set_rgb_pct(img, color, a[2], a[3], a[4]);
            }
        } else if (ch == '"') {
            parse_params(&p, end, a, 4);
            if (a[2] > 0 && a[3] > 0) {
                if (decoder_reserve(&d, a[2], (a[3] + 5) / 6) != 0) {
                    goto fail;
                }
                d.used_width = a[2] > d.used_width ? a[2] : d.used_width;
                d.used_height = a[3] > d.used_height ? a[3] : d.used_height;
            }
        }
    }
    if (band_rows != 0 && y + bottom_row(band_rows) >= d.used_height) {
        d.used_height = y + bottom_row(band_rows) + 1;
    }
    if (d.used_width == 0 || d.used_height == 0 ||
            image_alloc(img, d.used_width, d.used_height) != 0) {
        goto fail;
    }
    materialize(&d, img, 0, (d.used_height + 5) / 6);
    free(d.columns);
    return 0;
fail:
    free(d.columns);
    return -1;
}

#ifndef TUTORIAL_STEP5
int
tutorial_decode(unsigned char const *p, size_t size, image_t *img)
{
    return decode_serial(p, size, img);
}
#endif /* TUTORIAL_STEP5 */
