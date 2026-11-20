/* SPDX-License-Identifier: MIT */
/*
 * Decode into column words.
 *
 * A sixel character describes one column of a six-row band, so this
 * decoder keeps the canvas as one 64-bit word per column, one byte per row:
 *
 *   byte:   7   6    5    4    3    2    1    0
 *   word: [ 0 | 0 | r5 | r4 | r3 | r2 | r1 | r0 ]   rN = register of row N
 *
 * It paints a character with one masked word store instead of six byte
 * stores, and converts columns back to rows once at the end.
 *
 * The encoder is the direct-writes chapter's.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

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
 * or leaves *pp alone and returns 0 so that parse_params() can take over. */
static inline int parse_plain_number(unsigned char const **pp, unsigned char const *end, int *value)
{
    unsigned char const *p = *pp;
    int v = 0, digits = 0;

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

/* One pass over the band's pixels: the pixel's row bit goes into its
 * color's drawer, and the column's bit into that color's occupancy row. */
static void classify(encoder_t *e, image_t const *img, int y, int rows)
{
    for (int r = 0; r < rows; r++) {
        unsigned char const *row = img->pixels + (size_t)(y + r) * e->width;
        for (int x = 0; x < e->width; x++) {
            e->map[row[x] * e->width + x] |= (unsigned char)(1 << r);
            e->occupancy[row[x] * e->words + (size_t)x / 64] |= UINT64_C(1) << (x % 64);
            e->used[row[x]] = 1;
        }
    }
}

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
    int rows = img->height - y < 6 ? img->height - y : 6;

    if (y > 0) buf_putc(out, '-');
    classify(e, img, y, rows);
    compose(e);
    put_band(e, out);
}

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

/* Copy row r of every column word into the image: the transpose back. */
static void materialize(decoder_t const *d, image_t *img, int first_band, int end_band)
{
    for (int b = first_band; b < end_band; b++) {
        uint64_t const *col = d->columns + (size_t)b * d->width;
        for (int r = 0, y; r < 6 && (y = b * 6 + r) < img->height; r++) {
            unsigned char *row = img->pixels + (size_t)y * img->width;
            for (int x = 0; x < img->width; x++)
                row[x] = (unsigned char)(col[x] >> (8 * r));
        }
    }
}

static inline int select_color(image_t *img, int n)
{
    int color = n > 255 ? 255 : n;
    if (color >= img->ncolors) img->ncolors = color + 1;
    return color;
}

#define COLOR6(c) ((uint64_t)(c) * UINT64_C(0x010101010101))

int decode_sixel(unsigned char const *p, size_t size, image_t *img)
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
        /* The masks inner loop, with one word store per character. */
        if (band < d.bands) {
            col = d.columns + (size_t)band * d.width;
            last = -1;
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
