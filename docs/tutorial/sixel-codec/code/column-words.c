/* SPDX-License-Identifier: MIT */
/*
 * Keep a column in one word.
 *
 * A sixel character describes one column of a six-row band. This encoder
 * copies the six pixels of each column into one 64-bit word first, one byte
 * per row:
 *
 *   byte:   7   6    5    4    3    2    1    0
 *   word: [ 0 | 0 | r5 | r4 | r3 | r2 | r1 | r0 ]   rN = register of row N
 *
 * and then classifies the column from that word. The decoder is the masks
 * chapter's, and the stream is the packing chapter's.
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

static inline void buf_putnum(buffer_t *b, int n)
{
    char digits[12];
    int i = 0;

    do digits[i++] = (char)('0' + n % 10); while ((n /= 10) > 0);
    buf_reserve(b, (size_t)i);
    while (i > 0) b->data[b->size++] = (unsigned char)digits[--i];
}

/* Emit `count` copies of a sixel character, using DECGRI ('!') when the
 * repeat form is shorter. */
static inline void buf_putrun(buffer_t *b, int ch, int count)
{
    if (count > 3) {
        buf_putc(b, '!');
        buf_putnum(b, count);
        buf_putc(b, ch);
    } else {
        while (count-- > 0) buf_putc(b, ch);
    }
}

static void put_header_and_palette(image_t const *img, buffer_t *out)
{
    buf_printf(out, "\033Pq\"1;1;%d;%d", img->width, img->height);
    for (int c = 0; c < img->ncolors; c++)
        buf_printf(out, "#%d;2;%d;%d;%d", c, PCT(img->palette[c * 3]),
                   PCT(img->palette[c * 3 + 1]), PCT(img->palette[c * 3 + 2]));
}
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

#define NODE_GAP 9

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
    uint64_t *columns;          /* one column word per x */
    unsigned char used[256];    /* colors present in the band */
    uint64_t *starts;           /* columns where pending nodes start */
    node_t *pool;
    node_t **heads;             /* pending nodes by start, longest first */
    int nnodes;
} encoder_t;

static void encoder_free(encoder_t *e)
{
    free(e->map), free(e->columns), free(e->starts), free(e->pool), free(e->heads);
}

static int encoder_init(encoder_t *e, image_t const *img)
{
    int w = img->width;

    memset(e, 0, sizeof(*e));
    e->width = w;
    e->words = ((size_t)w + 63) / 64;
    e->map = calloc((size_t)img->ncolors * w, 1);
    e->columns = malloc((size_t)w * sizeof(uint64_t));
    e->pool = malloc(sizeof(node_t) * (size_t)img->ncolors * (w / (NODE_GAP + 1) + 1));
    e->heads = calloc((size_t)w, sizeof(node_t *));
    e->starts = calloc(e->words, sizeof(uint64_t));
    if (!e->map || !e->columns || !e->pool || !e->heads || !e->starts) {
        encoder_free(e);
        return -1;
    }
    return 0;
}

/* Transpose the band's rows into column words. Accumulating a whole row
 * at a time keeps every loop a sequential pass that compilers vectorize. */
static void load_columns(encoder_t *e, image_t const *img, int y, int rows)
{
    memset(e->columns, 0, (size_t)e->width * sizeof(uint64_t));
    for (int r = 0; r < rows; r++) {
        unsigned char const *row = img->pixels + (size_t)(y + r) * e->width;
        for (int x = 0; x < e->width; x++)
            e->columns[x] |= (uint64_t)row[x] << (8 * r);
    }
}

/* Add each pixel's row value to its color's slot, as before, but take the
 * six pixels of a column from one word instead of from six rows. */
static void classify(encoder_t *e, int rows)
{
    for (int x = 0; x < e->width; x++) {
        uint64_t word = e->columns[x];
        for (int r = 0; r < rows; r++) {
            unsigned int c = (unsigned int)(word >> (8 * r)) & 0xff;
            e->map[(size_t)c * e->width + x] |= (unsigned char)(1u << r);
            e->used[c] = 1;
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

/* Cut each used color's row of the map into nodes, as the packing
 * chapter does, and bucket them by start column. */
static void compose(encoder_t *e)
{
    e->nnodes = 0;
    for (int c = 0; c < 256; c++) {
        if (!e->used[c]) continue;
        e->used[c] = 0;
        unsigned char const *row = e->map + (size_t)c * e->width;
        for (int sx = 0, mx; sx < e->width; sx = mx) {
            while (sx < e->width && row[sx] == 0) sx++;
            if (sx == e->width) break;
            mx = sx + 1;
            for (int x = mx; x < e->width && x - mx <= NODE_GAP; x++)
                if (row[x] != 0) mx = x + 1;
            add_node(e, c, sx, mx);
        }
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

static void put_span(buffer_t *out, unsigned char const *map, int from, int to)
{
    for (int x = from, run; x < to; x += run) {
        for (run = 1; x + run < to && map[x + run] == map[x]; run++) {}
        buf_putrun(out, map[x] + '?', run);
    }
}

/* Packing walks the node list once per graphics line and skips the nodes
 * that start left of the cursor. Taking the leftmost pending start column
 * at or after the cursor, and the longest node there, picks the same nodes
 * in the same order without visiting the skipped ones. */
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
        if (np->color != color) {
            buf_putc(out, '#');
            buf_putnum(out, color = np->color);
        }
        buf_putrun(out, '?', np->sx - x);
        put_span(out, np->map, np->sx, np->mx);
        memset(np->map + np->sx, 0, (size_t)(np->mx - np->sx));
        x = np->mx;
        remaining--;
    }
}

static void encode_band(encoder_t *e, image_t const *img, int y, buffer_t *out)
{
    int rows = img->height - y < 6 ? img->height - y : 6;

    if (y > 0) buf_putc(out, '-');
    load_columns(e, img, y, rows);
    classify(e, rows);
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

typedef struct canvas {
    unsigned char *pixels;
    int width;              /* allocated size */
    int height;
    int used_width;         /* painted or declared extent */
    int used_height;
} canvas_t;

/* Make sure (need_w x need_h) is allocated, at least doubling each time so
 * that a stream without raster attributes still decodes in linear time. */
static int canvas_reserve(canvas_t *cv, int need_w, int need_h)
{
    int w = cv->width, h = cv->height;

    if (need_w <= w && need_h <= h) return 0;
    if (need_w > TUTORIAL_MAX_SIDE || need_h > TUTORIAL_MAX_BANDS * 6) return -1;
    while (w < need_w) w = w < 64 ? 64 : w * 2;
    while (h < need_h) h = h < 64 ? 64 : h * 2;
    w = w > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : w;
    h = h > TUTORIAL_MAX_BANDS * 6 ? TUTORIAL_MAX_BANDS * 6 : h;
    unsigned char *pixels = calloc((size_t)w * h, 1);
    if (pixels == NULL) return -1;
    for (int y = 0; y < cv->height; y++)
        memcpy(pixels + (size_t)y * w, cv->pixels + (size_t)y * cv->width, (size_t)cv->width);
    free(cv->pixels);
    cv->pixels = pixels;
    cv->width = w;
    cv->height = h;
    return 0;
}

/* Extend the painted height by the lowest row painted in a band. */
static void used_rows(canvas_t *cv, int y, int band_rows)
{
    if (band_rows == 0) return;
    int r = 5;
    while ((band_rows >> r & 1) == 0) r--;
    if (y + r + 1 > cv->used_height) cv->used_height = y + r + 1;
}

int decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    canvas_t cv = { NULL, 0, 0, 0, 0 };
    unsigned char *row, *dst;
    unsigned int mask;
    int a[5], n, x = 0, y = 0, color = 0, last, repeat, bits, ch;
    int band_rows = 0;      /* rows painted in this band */

    img->ncolors = 0;
    p = find_sixel_body(p, end);
    while (p < end && *p != 0x1b) {
        /* Most bytes are data characters that follow one another. Stay here
         * while that holds and the whole band fits on the canvas. */
        if (y + 6 <= cv.height) {
            row = cv.pixels + (size_t)y * cv.width;
            last = -1;
            while (p < end && *p >= '?' && *p <= '~' && x < cv.width) {
                bits = *p++ - '?';
                dst = row + x;
                for (int r = 0; r < 6; r++, dst += cv.width) {
                    mask = 0u - ((unsigned int)bits >> r & 1u);
                    *dst = (unsigned char)((*dst & ~mask) | ((unsigned int)color & mask));
                }
                band_rows |= bits;
                if (bits != 0) last = x;
                x++;
            }
            if (last >= cv.used_width) cv.used_width = last + 1;
            if (p == end || *p == 0x1b) break;
        }
        ch = *p++;
        if (ch == '#' && parse_plain_number(&p, end, &n)) {
            color = n > 255 ? 255 : n;
            if (color >= img->ncolors) img->ncolors = color + 1;
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
                if (x + repeat > cv.width || y + 6 > cv.height)
                    if (x + repeat > TUTORIAL_MAX_SIDE ||
                            canvas_reserve(&cv, x + repeat, y + 6) != 0)
                        goto fail;
                for (int r = 0; r < 6; r++)
                    if (bits >> r & 1)
                        memset(cv.pixels + (size_t)(y + r) * cv.width + x, color, (size_t)repeat);
                band_rows |= bits;
                if (x + repeat > cv.used_width) cv.used_width = x + repeat;
            }
            x = x + repeat > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : x + repeat;
        } else if (ch == '$') {
            x = 0;
        } else if (ch == '-') {
            used_rows(&cv, y, band_rows);
            band_rows = 0;
            x = 0;
            y = y < TUTORIAL_MAX_SIDE ? y + 6 : y;
        } else if (ch == '#') {
            n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            if (color >= img->ncolors) img->ncolors = color + 1;
            if (n == 5 && a[1] == 1) set_hls(img, color, a[2], a[3], a[4]);
            else if (n == 5 && a[1] == 2) set_rgb_pct(img, color, a[2], a[3], a[4]);
        } else if (ch == '"') {
            parse_params(&p, end, a, 4);
            if (a[2] > 0 && a[3] > 0) {
                if (canvas_reserve(&cv, a[2], a[3]) != 0) goto fail;
                cv.used_width = a[2] > cv.used_width ? a[2] : cv.used_width;
                cv.used_height = a[3] > cv.used_height ? a[3] : cv.used_height;
            }
        }
    }
    used_rows(&cv, y, band_rows);
    if (cv.used_width == 0 || cv.used_height == 0 ||
            image_alloc(img, cv.used_width, cv.used_height) != 0)
        goto fail;
    for (y = 0; y < cv.used_height; y++)
        memcpy(img->pixels + (size_t)y * cv.used_width,
               cv.pixels + (size_t)y * cv.width, (size_t)cv.used_width);
    free(cv.pixels);
    return 0;
fail:
    free(cv.pixels);
    return -1;
}
