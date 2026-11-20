/* SPDX-License-Identifier: MIT */
/*
 * A decoder for the full body syntax.
 *
 * The decoder accepts HLS and RGB definitions, grows the canvas when paint
 * goes beyond the raster attributes or there are none, and fills repeats a
 * row at a time.
 *
 * The encoder is the packing chapter's.
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

/* Columns of zeros that may stay inside a node. A longer gap is cheaper to
 * skip by starting a new node later on the same or another line. */
#define NODE_GAP 9

typedef struct node {
    struct node *next;
    int color;
    int sx;                 /* first occupied column */
    int mx;                 /* one past the last occupied column */
    unsigned char *map;     /* sixel bits of this color, indexed by x */
} node_t;

/* Emit map[from, to) as run-length coded sixel characters. */
static void put_span(buffer_t *out, unsigned char const *map, int from, int to)
{
    for (int x = from, run; x < to; x += run) {
        for (run = 1; x + run < to && map[x + run] == map[x]; run++) {}
        buf_putrun(out, map[x] + '?', run);
    }
}

/* Emit the nodes of one band. Each pass over the list writes one graphics
 * line: it takes every node that starts at or after the current column. */
static void put_band(buffer_t *out, node_t *list)
{
    int color = -1;         /* every band selects its first color */

    while (list != NULL) {
        if (color >= 0) buf_putc(out, '$');     /* back to column 0 */
        int x = 0;
        for (node_t **link = &list, *np; (np = *link) != NULL;) {
            if (np->sx < x) {
                link = &np->next;               /* overlaps; next line */
                continue;
            }
            if (np->color != color) {
                buf_putc(out, '#');
                buf_putnum(out, color = np->color);
            }
            buf_putrun(out, '?', np->sx - x);   /* blank columns */
            put_span(out, np->map, np->sx, np->mx);
            memset(np->map + np->sx, 0, (size_t)(np->mx - np->sx));
            x = np->mx;
            *link = np->next;                   /* unlink */
        }
    }
}

int encode_sixel(image_t const *img, buffer_t *out)
{
    int w = img->width, h = img->height, ncolors = img->ncolors;
    unsigned char *map = calloc((size_t)ncolors * w, 1), *used = calloc((size_t)ncolors, 1);
    /* Nodes of one color are separated by more than NODE_GAP columns. */
    node_t *pool = malloc(sizeof(node_t) * (size_t)ncolors * (w / (NODE_GAP + 1) + 1));
    node_t **heads = calloc((size_t)w, sizeof(node_t *));

    if (!map || !used || !pool || !heads) {
        free(map), free(used), free(pool), free(heads);
        return -1;
    }
    put_header_and_palette(img, out);
    for (int y = 0; y < h; y += 6) {
        if (y > 0) buf_putc(out, '-');

        /* 1. classify: one pass over the band's pixels */
        for (int r = 0; r < 6 && y + r < h; r++) {
            unsigned char const *row = img->pixels + (size_t)(y + r) * w;
            for (int x = 0; x < w; x++) {
                map[row[x] * w + x] |= (unsigned char)(1 << r);
                used[row[x]] = 1;
            }
        }

        /* 2. compose: cut each used color's row into nodes, and bucket them
         * by start column, longest first. Ties keep color order. */
        int nnodes = 0;
        for (int c = 0; c < ncolors; c++) {
            unsigned char *row = map + (size_t)c * w;
            if (!used[c]) continue;
            used[c] = 0;
            for (int sx = 0, mx; sx < w; sx = mx) {
                while (sx < w && row[sx] == 0) sx++;
                if (sx == w) break;
                mx = sx + 1;
                for (int x = mx; x < w && x - mx <= NODE_GAP; x++)
                    if (row[x] != 0) mx = x + 1;
                node_t *np = &pool[nnodes++], **link = &heads[sx];
                *np = (node_t){ NULL, c, sx, mx, row };
                while (*link && (*link)->mx >= mx) link = &(*link)->next;
                np->next = *link;
                *link = np;
            }
        }

        /* 3. order: concatenate the buckets left to right */
        node_t *list = NULL, **tail = &list;
        for (int x = 0; x < w; x++) {
            for (node_t *np = heads[x]; np != NULL; np = np->next) {
                *tail = np;
                tail = &np->next;
            }
            heads[x] = NULL;
        }
        *tail = NULL;

        /* 4. emit, which also clears the map behind each node */
        put_band(out, list);
    }
    buf_printf(out, "\033\\");
    free(map), free(used), free(pool), free(heads);
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
    unsigned char *dst;
    int a[5], n, x = 0, y = 0, color = 0, repeat, bits, ch;
    int band_rows = 0;      /* rows painted in this band */

    img->ncolors = 0;
    p = find_sixel_body(p, end);
    while (p < end && *p != 0x1b) {
        ch = *p++;
        repeat = 1;
        if (ch == '!') {
            parse_params(&p, end, &repeat, 1);
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
                for (int r = 0; r < 6; r++) {
                    if ((bits >> r & 1) == 0) continue;
                    dst = cv.pixels + (size_t)(y + r) * cv.width + x;
                    if (repeat == 1) *dst = (unsigned char)color;   /* the common case */
                    else memset(dst, color, (size_t)repeat);
                }
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
