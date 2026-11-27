/* SPDX-License-Identifier: MIT */
/*
 * Paint sixteen characters at once.
 *
 * The decoder's inner loop paints one data character per iteration and
 * leaves when a character that is not data arrives. This file looks at
 * sixteen bytes with one vector compare, which tells how many of them are
 * data, and paints that many columns with vector stores: the characters
 * are spread over their column words by interleaving the vector with
 * itself, the reverse of the transpose. Runs shorter than four characters,
 * the last bytes of the input and the right edge keep the scalar loop.
 *
 * The encoder is the vector-lanes chapter's, through the transpose chapter.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

static inline int ctz32(uint32_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(v);
#else
    int n = 0;
    for (; (v & 1) == 0; v >>= 1) n++;
    return n;
#endif
}

/* SIXEL color coordinates are percentages (0-100). */
#define PCT(v) (((v) * 100 + 127) / 255)

static void set_rgb_pct(image_t *img, int color, int r, int g, int b)
{
    int pct[3] = { r, g, b };
    for (int i = 0; i < 3; i++)
        img->palette[color * 3 + i] =
            (unsigned char)(((pct[i] > 100 ? 100 : pct[i]) * 255 + 50) / 100);
}

static double hls_channel(double m1, double m2, double h)
{
    h -= 360.0 * (int)(h / 360.0);
    if (h < 60.0) return m1 + (m2 - m1) * h / 60.0;
    if (h < 180.0) return m2;
    if (h < 240.0) return m1 + (m2 - m1) * (240.0 - h) / 60.0;
    return m1;
}

/* SIXEL HLS puts blue at 0 degrees, red at 120 and green at 240, so the
 * usual red-at-zero hue is rotated by 240 degrees. */
static void set_hls(image_t *img, int color, int h, int l, int s)
{
    double lum = (l > 100 ? 100 : l) / 100.0, sat = (s > 100 ? 100 : s) / 100.0;
    double m2 = lum <= 0.5 ? lum * (1.0 + sat) : lum + sat - lum * sat, m1 = 2.0 * lum - m2;
    double hue = (double)((h + 240) % 360);
    double rgb[3] = { hls_channel(m1, m2, hue + 120.0), hls_channel(m1, m2, hue),
                      hls_channel(m1, m2, hue + 240.0) };
    for (int i = 0; i < 3; i++)
        img->palette[color * 3 + i] = (unsigned char)(rgb[i] * 255.0 + 0.5);
}

/* Read up to max numeric parameters separated by ';'. Missing values are
 * zero. Returns the number of parameters present, at most max. */
static int parse_params(unsigned char const **pp, unsigned char const *end, int *params, int max)
{
    unsigned char const *p = *pp;
    int n = 0;

    for (;;) {
        int value = 0;
        for (; p < end && *p >= '0' && *p <= '9'; p++)
            if (value < 1000000) value = value * 10 + (*p - '0');
        if (n < max) params[n] = value;
        n++;
        if (p == end || *p != ';') break;
        p++;
    }
    for (int i = n; i < max; i++) params[i] = 0;
    *pp = p;
    return n < max ? n : max;
}

/* A number of at most five digits that no ';' or further digit follows, the
 * form of nearly every "#n" and "!n" in a stream. Advances *pp and returns 1,
 * or leaves *pp alone and returns 0 so that parse_params() can take over.
 *
 * With four bytes left, they are read as one little-endian word, the first
 * byte lowest. Subtracting 0x30 from every byte leaves the digit values. A
 * byte that is not a digit gets its top bit set: below '0' the subtraction
 * borrows into it, and above '9' adding 0x46 carries into it. The lowest set
 * top bit is where the number ends. */
static inline int parse_plain_number(unsigned char const **pp, unsigned char const *end, int *value)
{
    unsigned char const *p = *pp;
    int v = 0, digits = 0;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
    defined(_M_X64) || defined(_M_ARM64)
    if (end - p >= 4) {
        uint32_t word, d, stop;
        memcpy(&word, p, 4);
        d = word - UINT32_C(0x30303030);
        stop = ((word + UINT32_C(0x46464646)) | d) & UINT32_C(0x80808080);
        if (stop != 0) {                    /* one to three digits, or none */
            digits = ctz32(stop) / 8;
            if (digits == 0 || p[digits] == ';') return 0;
            d <<= 32 - 8 * digits;          /* the last digit on top */
            *value = (int)(d >> 24) + (int)(d >> 16 & 0xff) * 10 +
                     (int)(d >> 8 & 0xff) * 100;
            *pp = p + digits;
            return 1;
        }
    }
#endif
    for (; p < end && *p >= '0' && *p <= '9' && digits < 5; digits++)
        v = v * 10 + (*p++ - '0');
    if (digits == 0 || p == end || *p == ';' || (*p >= '0' && *p <= '9')) return 0;
    *pp = p;
    *value = v;
    return 1;
}

/* The first body byte after the "ESC P ... q" introducer, or end. */
static unsigned char const *find_sixel_body(unsigned char const *p, unsigned char const *end)
{
    while (p < end && *p != 0x90 && !(*p == 0x1b && p + 1 < end && p[1] == 'P')) p++;
    while (p < end && *p++ != 'q') {}
    return p;
}

static void put_header_and_palette(image_t const *img, buffer_t *out)
{
    buf_printf(out, "\033Pq\"1;1;%d;%d", img->width, img->height);
    for (int c = 0; c < img->ncolors; c++)
        buf_printf(out, "#%d;2;%d;%d;%d", c, PCT(img->palette[c * 3]),
                   PCT(img->palette[c * 3 + 1]), PCT(img->palette[c * 3 + 2]));
}

#define NODE_GAP 9
#define BYTES_01 UINT64_C(0x0101010101010101)

static inline int ctz64(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(v);
#else
    int n = 0;
    for (; (v & 1) == 0; v >>= 1) n++;
    return n;
#endif
}

/* Inverse direction: expand six row bits to a mask of 0xff bytes. */
static inline uint64_t row_mask(unsigned int bits)
{
    return ((bits * UINT64_C(0x0002040810204081)) & BYTES_01) * 0xff;
}

/* ------------------------------------------------------------------ */
/* The few vector operations this step needs                           */

#if defined(TUTORIAL_NO_SIMD)
/* scalar build for comparison: the scalar code paths only */
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
/* Lanes holding a data character, '?' to '~'. Bytes from 0x80 are negative
 * as signed bytes, so they fail the first compare. */
#define v_data(v)     _mm_and_si128(_mm_cmpgt_epi8((v), _mm_set1_epi8(0x3e)), \
                                    _mm_cmplt_epi8((v), _mm_set1_epi8(0x7f)))
#define v_lt(a, b)    _mm_cmplt_epi8((a), (b))
#define v_sub(a, b)   _mm_sub_epi8((a), (b))
#define v_andnot(m, a) _mm_andnot_si128((m), (a))
#define v_set1_64(x)  _mm_set1_epi64x((long long)(x))
#define v_zip8(a)     _mm_unpacklo_epi8((a), (a))
#define v_zip8hi(a)   _mm_unpackhi_epi8((a), (a))
#define v_zip16(a)    _mm_unpacklo_epi16((a), (a))
#define v_zip16hi(a)  _mm_unpackhi_epi16((a), (a))
#define v_zip32(a)    _mm_unpacklo_epi32((a), (a))
#define v_zip32hi(a)  _mm_unpackhi_epi32((a), (a))
/* One bit per byte lane, taken from the lane's top bit. */
#define v_movemask(v) ((unsigned int)_mm_movemask_epi8(v))
#define LOAD8(i)      _mm_loadl_epi64((void const *)&col[i])

/* Rows r0..r5 of eight columns from eight column words: interleave bytes,
 * then 16-bit pairs, then 32-bit quads. Each round doubles the run of
 * same-row bytes until a row is eight bytes long. */
static inline void transpose_columns(uint64_t const *col, unsigned char **rows,
                                     int nrows)
{
    __m128i a0 = _mm_unpacklo_epi8(LOAD8(0), LOAD8(1));
    __m128i a1 = _mm_unpacklo_epi8(LOAD8(2), LOAD8(3));
    __m128i a2 = _mm_unpacklo_epi8(LOAD8(4), LOAD8(5));
    __m128i a3 = _mm_unpacklo_epi8(LOAD8(6), LOAD8(7));
    __m128i b0 = _mm_unpacklo_epi16(a0, a1), b1 = _mm_unpackhi_epi16(a0, a1);
    __m128i b2 = _mm_unpacklo_epi16(a2, a3), b3 = _mm_unpackhi_epi16(a2, a3);
    __m128i out[3] = {
        _mm_unpacklo_epi32(b0, b2),     /* rows 0 and 1 */
        _mm_unpackhi_epi32(b0, b2),     /* rows 2 and 3 */
        _mm_unpacklo_epi32(b1, b3),     /* rows 4 and 5 */
    };
    for (int r = 0; r < nrows; r++)
        _mm_storel_epi64((void *)rows[r], (r & 1) ?
                         _mm_unpackhi_epi64(out[r / 2], out[r / 2]) : out[r / 2]);
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
#define v_data(v)     vcleq_u8(vsubq_u8((v), vdupq_n_u8('?')), vdupq_n_u8(63))
#define v_lt(a, b)    vcltq_u8((a), (b))
#define v_sub(a, b)   vsubq_u8((a), (b))
#define v_andnot(m, a) vbicq_u8((a), (m))
#define v_set1_64(x)  vreinterpretq_u8_u64(vdupq_n_u64(x))
#define v_zip8(a)     vzip1q_u8((a), (a))
#define v_zip8hi(a)   vzip2q_u8((a), (a))
#define v_zip16(a)    ZIP16(vzip1q_u16, a, a)
#define v_zip16hi(a)  ZIP16(vzip2q_u16, a, a)
#define v_zip32(a)    ZIP32(vzip1q_u32, a, a)
#define v_zip32hi(a)  ZIP32(vzip2q_u32, a, a)
#define ZIP16(f, a, b) vreinterpretq_u8_u16(f(vreinterpretq_u16_u8(a), \
                                              vreinterpretq_u16_u8(b)))
#define ZIP32(f, a, b) vreinterpretq_u8_u32(f(vreinterpretq_u32_u8(a), \
                                              vreinterpretq_u32_u8(b)))

/* NEON has no movemask. For compare results, whose lanes are 0x00 or
 * 0xff, keep each lane's own bit weight and add up each half. */
static inline unsigned int v_movemask(uint8x16_t v)
{
    static uint8_t const weights[16] = {
        1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128
    };
    uint8x16_t m = vandq_u8(v, vld1q_u8(weights));
    return (unsigned int)vaddv_u8(vget_low_u8(m)) |
           (unsigned int)vaddv_u8(vget_high_u8(m)) << 8;
}

static inline void transpose_columns(uint64_t const *col, unsigned char **rows,
                                     int nrows)
{
    uint8x16_t a[4];
    for (int i = 0; i < 4; i++)
        a[i] = vzip1q_u8(vreinterpretq_u8_u64(vdupq_n_u64(col[2 * i])),
                         vreinterpretq_u8_u64(vdupq_n_u64(col[2 * i + 1])));
    uint8x16_t b0 = ZIP16(vzip1q_u16, a[0], a[1]), b1 = ZIP16(vzip2q_u16, a[0], a[1]);
    uint8x16_t b2 = ZIP16(vzip1q_u16, a[2], a[3]), b3 = ZIP16(vzip2q_u16, a[2], a[3]);
    uint8x16_t out[3] = {
        ZIP32(vzip1q_u32, b0, b2), ZIP32(vzip2q_u32, b0, b2), ZIP32(vzip1q_u32, b1, b3)
    };
    for (int r = 0; r < nrows; r++)
        vst1_u8(rows[r], (r & 1) ? vget_high_u8(out[r / 2]) : vget_low_u8(out[r / 2]));
}
#endif

/* ------------------------------------------------------------------ */
/* Encoder                                                             */

typedef struct node {
    struct node *next;
    int color, sx, mx;
    unsigned char *map;
} node_t;

typedef struct encoder {
    int width;
    size_t words;               /* 64-bit words per occupancy row */
    unsigned char *map;         /* ncolors rows of sixel bits */
    uint64_t *occupancy;        /* ncolors rows of column bits */
    unsigned char used[256];    /* colors present in the band */
    unsigned int seen[256];     /* block stamp of the last visit */
    unsigned int stamp;
    uint64_t *starts;           /* columns where pending nodes start */
    node_t *pool;
    node_t **heads;             /* pending nodes by start, longest first */
    int nnodes;
} encoder_t;

static void encoder_free(encoder_t *e)
{
    free(e->map), free(e->occupancy), free(e->starts), free(e->pool), free(e->heads);
}

static int encoder_init(encoder_t *e, image_t const *img)
{
    int w = img->width;

    memset(e, 0, sizeof(*e));
    e->width = w;
    e->words = ((size_t)w + 63) / 64;
    e->map = calloc((size_t)img->ncolors * w, 1);
    e->occupancy = calloc((size_t)img->ncolors * e->words, sizeof(uint64_t));
    e->pool = malloc(sizeof(node_t) * (size_t)img->ncolors * (w / (NODE_GAP + 1) + 1));
    e->heads = calloc((size_t)w, sizeof(node_t *));
    e->starts = calloc(e->words, sizeof(uint64_t));
    if (!e->map || !e->occupancy || !e->pool || !e->heads || !e->starts) {
        encoder_free(e);
        return -1;
    }
    return 0;
}

/* Classify columns [from, width) one pixel at a time: the pixel's row bit
 * goes into its color's drawer, and the column's bit into that color's
 * occupancy row. */
static void classify(encoder_t *e, image_t const *img, int y, int rows, int from)
{
    for (int r = 0; r < rows; r++) {
        unsigned char const *row = img->pixels + (size_t)(y + r) * e->width;
        for (int x = from; x < e->width; x++) {
            e->map[row[x] * e->width + x] |= (unsigned char)(1 << r);
            e->occupancy[row[x] * e->words + (size_t)x / 64] |= UINT64_C(1) << (x % 64);
            e->used[row[x]] = 1;
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
static void classify_blocks(encoder_t *e, image_t const *img, int y, int rows, int end)
{
    unsigned char const *src[6];
    v128 row[6], weight[6];

    for (int r = 0; r < rows; r++) {
        src[r] = img->pixels + (size_t)(y + r) * e->width;
        weight[r] = v_set1(1 << r);
    }
    for (int x = 0; x < end; x += 16) {
        e->stamp++;
        for (int r = 0; r < rows; r++)
            row[r] = v_load(src[r] + x);
        for (int r = 0; r < rows; r++) {
            for (int lane = 0; lane < 16; lane++) {
                int c = src[r][x + lane];
                if (e->seen[c] == e->stamp) continue;
                e->seen[c] = e->stamp;
                v128 color = v_set1(c), bits = v_zero();
                for (int k = r; k < rows; k++)      /* rows above r lack c */
                    bits = v_or(bits, v_and(v_eq(row[k], color), weight[k]));
                v_store(e->map + (size_t)c * e->width + x, bits);
                unsigned int occupied = ~v_movemask(v_eq(bits, v_zero())) & 0xffff;
                e->occupancy[(size_t)c * e->words + (size_t)x / 64] |=
                    (uint64_t)occupied << (x % 64);
                e->used[c] = 1;
            }
        }
    }
}
#endif

static void add_node(encoder_t *e, int c, int sx, int mx)
{
    node_t *np = &e->pool[e->nnodes++], **link = &e->heads[sx];

    *np = (node_t){ NULL, c, sx, mx, e->map + (size_t)c * e->width };
    while (*link && (*link)->mx >= mx) link = &(*link)->next;
    np->next = *link;
    *link = np;
    e->starts[sx / 64] |= UINT64_C(1) << (sx % 64);
}

/* The nodes of next-node, found by walking the set occupancy bits instead
 * of scanning every column of every color. The walk also clears the bits
 * and the used flags for the next band. */
static void compose(encoder_t *e)
{
    e->nnodes = 0;
    for (int c = 0; c < 256; c++) {
        if (!e->used[c]) continue;
        e->used[c] = 0;
        uint64_t *occ = e->occupancy + (size_t)c * e->words;
        int sx = -1, mx = 0;
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
    }
}

/* The first pending start column at or after x, or -1. */
static int next_start(encoder_t const *e, int x)
{
    if (x >= e->width) return -1;
    size_t i = (size_t)x / 64;
    uint64_t bits = e->starts[i] & (~UINT64_C(0) << (x % 64));
    while (bits == 0) {
        if (++i == e->words) return -1;
        bits = e->starts[i];
    }
    return (int)(i * 64) + ctz64(bits);
}

/* Write a number or a run at q. The caller has reserved the room. */
static inline unsigned char *put_num(unsigned char *q, int n)
{
    char digits[12];
    int i = 0;

    do digits[i++] = (char)('0' + n % 10); while ((n /= 10) > 0);
    while (i > 0) *q++ = (unsigned char)digits[--i];
    return q;
}

static inline unsigned char *put_run(unsigned char *q, int ch, int count)
{
    if (count > 3) {
        *q++ = '!';
        q = put_num(q, count);
        *q++ = (unsigned char)ch;
    } else {
        while (count-- > 0) *q++ = (unsigned char)ch;
    }
    return q;
}

/* As next-node, but each node reserves its worst case once and is then
 * written through a plain pointer: one byte per column at most, plus a color
 * selection and a run of blank columns. */
static void put_band(encoder_t *e, buffer_t *out)
{
    int color = -1;         /* every band selects its first color */

    for (int remaining = e->nnodes, x = 0; remaining > 0;) {
        int sx = next_start(e, x);
        if (sx < 0) {
            buf_putc(out, '$');
            x = 0;
            continue;
        }
        node_t *np = e->heads[sx];
        e->heads[sx] = np->next;
        if (np->next == NULL) e->starts[sx / 64] &= ~(UINT64_C(1) << (sx % 64));
        buf_reserve(out, (size_t)(np->mx - np->sx) + 32);
        unsigned char *q = out->data + out->size;
        if (np->color != color) {
            *q++ = '#';
            q = put_num(q, color = np->color);
        }
        q = put_run(q, '?', np->sx - x);
        for (int x1 = np->sx; x1 < np->mx;) {
            unsigned char ch = np->map[x1];
            int run = 1;
            while (x1 + run < np->mx && np->map[x1 + run] == ch) run++;
            if (run <= 3) {
                q[0] = q[1] = q[2] = (unsigned char)(ch + '?');    /* no branch */
                q += run;
            } else {
                q = put_run(q, ch + '?', run);
            }
            x1 += run;
        }
        out->size = (size_t)(q - out->data);
        memset(np->map + np->sx, 0, (size_t)(np->mx - np->sx));
        x = np->mx;
        remaining--;
    }
}

static void encode_band(encoder_t *e, image_t const *img, int y, buffer_t *out)
{
    int rows = img->height - y < 6 ? img->height - y : 6, from = 0;

    if (y > 0) buf_putc(out, '-');
#ifdef HAVE_SIMD
    from = e->width & ~15;
    classify_blocks(e, img, y, rows, from);
#endif
    classify(e, img, y, rows, from);
    compose(e);
    put_band(e, out);
}

#ifndef TUTORIAL_THREADED
int encode_sixel(image_t const *img, buffer_t *out)
{
    encoder_t e;

    if (encoder_init(&e, img) != 0) return -1;
    put_header_and_palette(img, out);
    for (int y = 0; y < img->height; y += 6)
        encode_band(&e, img, y, out);
    buf_printf(out, "\033\\");
    encoder_free(&e);
    return 0;
}
#endif /* TUTORIAL_THREADED */

/* ------------------------------------------------------------------ */
/* Decoder                                                             */

static inline int bottom_row(int bits)
{
    int r = 5;
    while ((bits >> r & 1) == 0) r--;
    return r;
}

typedef struct decoder {
    uint64_t *columns;      /* bands * width column words */
    int width;              /* allocated columns per band */
    int bands;              /* allocated bands */
    int used_width;         /* declared or painted extent */
    int used_height;
} decoder_t;

/* Grow like full-decoder, moving whole column words. */
static int decoder_reserve(decoder_t *d, int need_w, int need_bands)
{
    int w = d->width, bands = d->bands;

    if (need_w <= w && need_bands <= bands) return 0;
    if (need_w > TUTORIAL_MAX_SIDE || need_bands > TUTORIAL_MAX_BANDS) return -1;
    while (w < need_w) w = w < 64 ? 64 : w * 2;
    while (bands < need_bands) bands = bands < 16 ? 16 : bands * 2;
    w = w > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : w;
    bands = bands > TUTORIAL_MAX_BANDS ? TUTORIAL_MAX_BANDS : bands;
    uint64_t *columns = calloc((size_t)w * bands, sizeof(uint64_t));
    if (columns == NULL) return -1;
    for (int b = 0; b < d->bands; b++)
        memcpy(columns + (size_t)b * w, d->columns + (size_t)b * d->width,
               (size_t)d->width * sizeof(uint64_t));
    free(d->columns);
    *d = (decoder_t){ columns, w, bands, d->used_width, d->used_height };
    return 0;
}

/* Copy row r of every column word into the image: the transpose back,
 * eight columns at a time when SIMD is available. */
static void materialize(decoder_t const *d, image_t *img, int first_band, int end_band)
{
    for (int b = first_band; b < end_band; b++) {
        uint64_t const *col = d->columns + (size_t)b * d->width;
        unsigned char *rows[6];
        int nrows = img->height - b * 6 < 6 ? img->height - b * 6 : 6, x = 0;

        for (int r = 0; r < nrows; r++)
            rows[r] = img->pixels + (size_t)(b * 6 + r) * img->width;
#ifdef HAVE_SIMD
        for (; x + 8 <= img->width; x += 8) {
            transpose_columns(col + x, rows, nrows);
            for (int r = 0; r < nrows; r++) rows[r] += 8;
        }
#endif
        for (int r = 0; r < nrows; r++)
            for (int i = x; i < img->width; i++)
                rows[r][i - x] = (unsigned char)(col[i] >> (8 * r));
    }
}

static inline int select_color(image_t *img, int n)
{
    int color = n > 255 ? 255 : n;
    if (color >= img->ncolors) img->ncolors = color + 1;
    return color;
}

#define COLOR6(c) ((uint64_t)(c) * UINT64_C(0x010101010101))

#ifdef HAVE_SIMD
static inline int clz32(uint32_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clz(v);
#else
    int n = 0;
    for (; (v & 0x80000000u) == 0; v <<= 1) n++;
    return n;
#endif
}

/* Paint the data characters at the start of the sixteen bytes at p into the
 * sixteen column words at col and return how many there were, or 0 when the
 * run is too short to be worth it. Lanes past the run are cleared, so they
 * paint nothing. Interleaving the characters with themselves three times
 * fills the eight bytes of a column word with its character, two words per
 * vector; a byte is painted when its row bit is set. Row bits 6 and 7 never
 * occur, so the two spare bytes of each word are never painted. *rows
 * collects the painted rows and *last the last painted lane. */
static inline int paint_block(unsigned char const *p, uint64_t *col, uint64_t color6,
                              int *rows, int *last)
{
    static unsigned char const lane_index[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    v128 c = v_load(p);
    int n = ctz32(~v_movemask(v_data(c)) | 0x10000);

    if (n < 4) return 0;
    c = v_and(v_sub(c, v_set1('?')), v_lt(v_load(lane_index), v_set1(n)));
    unsigned int painted = ~v_movemask(v_eq(c, v_zero())) & 0xffff;
    if (painted != 0) *last = 31 - clz32(painted);
    uint64_t halves[2];
    v_store(halves, c);
    halves[0] |= halves[1];
    halves[0] |= halves[0] >> 32, halves[0] |= halves[0] >> 16, halves[0] |= halves[0] >> 8;
    *rows |= (int)(halves[0] & 63);

    v128 bit = v_set1_64(UINT64_C(0x8040201008040201)), color = v_set1_64(color6);
    v128 lo = v_zip8(c), hi = v_zip8hi(c);
    v128 quads[4] = { v_zip16(lo), v_zip16hi(lo), v_zip16(hi), v_zip16hi(hi) };
    for (int i = 0; i < (n > 8 ? 4 : 2); i++) {
        v128 pair[2] = { v_zip32(quads[i]), v_zip32hi(quads[i]) };
        for (int j = 0; j < 2; j++) {
            uint64_t *w = col + 4 * i + 2 * j;
            v128 m = v_eq(v_and(pair[j], bit), bit);
            v_store(w, v_or(v_andnot(m, v_load(w)), v_and(m, color)));
        }
    }
    return n;
}
#endif

static int decode_serial(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    decoder_t d = { NULL, 0, 0, 0, 0 };
    uint64_t color6 = 0;
    uint64_t *col, mask, paint;
    int a[5], n, x = 0, y = 0, color = 0, last, repeat, bits, ch;
    int band = 0;           /* y / 6 */
    int band_rows = 0;      /* rows painted in this band */

    img->ncolors = 0;
    p = find_sixel_body(p, end);
    while (p < end && *p != 0x1b) {
        /* The masks inner loop, with one word store per character, after
         * sixteen characters at a time while the run is long. */
        if (band < d.bands) {
            col = d.columns + (size_t)band * d.width;
            last = -1;
#ifdef HAVE_SIMD
            while (end - p >= 16 && d.width - x >= 16) {
                int lane = -1, n = paint_block(p, col + x, color6, &band_rows, &lane);
                if (lane >= 0) last = x + lane;
                p += n;
                x += n;
                if (n < 16) break;
            }
#endif
            while (p < end && *p >= '?' && *p <= '~' && x < d.width) {
                bits = *p++ - '?';
                mask = row_mask((unsigned int)bits);
                col[x] = (col[x] & ~mask) | (color6 & mask);
                band_rows |= bits;
                if (bits != 0) last = x;
                x++;
            }
            if (last >= d.used_width) d.used_width = last + 1;
            if (p == end || *p == 0x1b) break;
        }
        ch = *p++;
        if (ch == '#' && parse_plain_number(&p, end, &n)) {
            color6 = COLOR6(color = select_color(img, n));
            continue;
        }
        repeat = 1;
        if (ch == '!') {
            if (!parse_plain_number(&p, end, &repeat)) parse_params(&p, end, &repeat, 1);
            repeat = repeat > 0 ? repeat : 1;
            if (p == end) break;
            ch = *p++;
        }
        if (ch >= '?' && ch <= '~') {
            bits = ch - '?';
            if (bits != 0) {
                if ((x + repeat > d.width || band >= d.bands) &&
                        (x + repeat > TUTORIAL_MAX_SIDE ||
                         decoder_reserve(&d, x + repeat, band + 1) != 0))
                    goto fail;
                mask = row_mask((unsigned int)bits);
                paint = color6 & mask;
                col = d.columns + (size_t)band * d.width + x;
                for (int i = 0; i < repeat; i++)
                    col[i] = (col[i] & ~mask) | paint;
                if (x + repeat > d.used_width) d.used_width = x + repeat;
                band_rows |= bits;
            }
            x = x + repeat > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : x + repeat;
        } else if (ch == '$') {
            x = 0;
        } else if (ch == '-') {
            if (band_rows != 0 && y + bottom_row(band_rows) >= d.used_height)
                d.used_height = y + bottom_row(band_rows) + 1;
            band_rows = 0;
            x = 0;
            y = y < TUTORIAL_MAX_SIDE ? y + 6 : y;
            band = y / 6;
        } else if (ch == '#') {
            n = parse_params(&p, end, a, 5);
            color6 = COLOR6(color = select_color(img, a[0]));
            if (n == 5 && a[1] == 1) set_hls(img, color, a[2], a[3], a[4]);
            else if (n == 5 && a[1] == 2) set_rgb_pct(img, color, a[2], a[3], a[4]);
        } else if (ch == '"') {
            parse_params(&p, end, a, 4);
            if (a[2] > 0 && a[3] > 0) {
                if (decoder_reserve(&d, a[2], (a[3] + 5) / 6) != 0) goto fail;
                d.used_width = a[2] > d.used_width ? a[2] : d.used_width;
                d.used_height = a[3] > d.used_height ? a[3] : d.used_height;
            }
        }
    }
    if (band_rows != 0 && y + bottom_row(band_rows) >= d.used_height)
        d.used_height = y + bottom_row(band_rows) + 1;
    if (d.used_width == 0 || d.used_height == 0 ||
            image_alloc(img, d.used_width, d.used_height) != 0)
        goto fail;
    materialize(&d, img, 0, (d.used_height + 5) / 6);
    free(d.columns);
    return 0;
fail:
    free(d.columns);
    return -1;
}

#ifndef TUTORIAL_THREADED
int decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    return decode_serial(p, size, img);
}
#endif /* TUTORIAL_THREADED */
