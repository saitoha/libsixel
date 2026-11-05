/* SPDX-License-Identifier: MIT */
/*
 * Step 3: fewer decisions per byte.
 *
 * The data layout is still step 2's. What changes is how often the program
 * has to stop and decide something it could have known already.
 *
 * Decoder: consecutive data characters stay in one inner loop instead of
 * returning to the per-byte dispatch; a plain "#n" or "!n" is read on the
 * spot; a single character stores with a mask instead of testing six bits.
 *
 * Encoder: nodes stay in their start-column buckets, and a bitmap of the
 * non-empty buckets finds the next node for a line with one bit scan instead
 * of walking past the nodes that do not fit. The stream does not change.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

/* Columns of zeros that may stay inside a node. A longer gap is cheaper to
 * skip by starting a new node later on the same or another line. */
#define NODE_GAP 9

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

typedef struct node {
    struct node *next;
    int color;
    int sx;                 /* first occupied column */
    int mx;                 /* one past the last occupied column */
    unsigned char *map;     /* sixel bits of this color, indexed by x */
} node_t;

/* Emit map[from, to) as run-length coded sixel characters. */
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

/* The first pending start column at or after x, or -1. */
static int
next_start(uint64_t const *starts, size_t words, int width, int x)
{
    size_t i = (size_t)x / 64;
    uint64_t bits;

    if (x >= width) {
        return -1;
    }
    bits = starts[i] & (~UINT64_C(0) << (x % 64));
    while (bits == 0) {
        if (++i == words) {
            return -1;
        }
        bits = starts[i];
    }
    return (int)(i * 64) + ctz64(bits);
}

/* Emit the nodes of one band. Taking the leftmost pending start column at or
 * after the cursor, and the longest node there, picks the same nodes in the
 * same order as step 2's list walk without visiting the skipped ones. */
static void
put_band(buffer_t *out, node_t **heads, uint64_t *starts, size_t words,
         int width, int nnodes)
{
    node_t *np;
    int color = -1;         /* every band selects its first color */
    int x = 0;
    int sx;

    while (nnodes > 0) {
        sx = next_start(starts, words, width, x);
        if (sx < 0) {
            buf_putc(out, '$');             /* back to column 0 */
            x = 0;
            continue;
        }
        np = heads[sx];
        heads[sx] = np->next;
        if (np->next == NULL) {
            starts[sx / 64] &= ~(UINT64_C(1) << (sx % 64));
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
        nnodes--;
    }
}

int
encode_sixel(image_t const *img, buffer_t *out)
{
    int w = img->width;
    int h = img->height;
    int ncolors = img->ncolors;
    unsigned char *map;
    unsigned char *used;
    node_t *pool;
    node_t **heads;
    node_t **link;
    uint64_t *starts;       /* one bit per column with a non-empty bucket */
    size_t words = ((size_t)w + 63) / 64;
    node_t *np;
    int nnodes;
    int y;
    int r;
    int x;
    int c;
    int sx;
    int mx;

    map = calloc((size_t)ncolors * w, 1);
    used = calloc((size_t)ncolors, 1);
    /* Nodes of one color are separated by more than NODE_GAP columns. */
    pool = malloc(sizeof(node_t) * (size_t)ncolors * (w / (NODE_GAP + 1) + 1));
    heads = calloc((size_t)w, sizeof(node_t *));
    starts = calloc(words, sizeof(uint64_t));
    if (map == NULL || used == NULL || pool == NULL || heads == NULL ||
            starts == NULL) {
        free(map), free(used), free(pool), free(heads), free(starts);
        return -1;
    }

    put_header_and_palette(img, out);
    for (y = 0; y < h; y += 6) {
        if (y > 0) {
            buf_putc(out, '-');
        }

        /* 1. classify: one pass over the band's pixels */
        for (r = 0; r < 6 && y + r < h; r++) {
            unsigned char const *row = img->pixels + (size_t)(y + r) * w;
            for (x = 0; x < w; x++) {
                map[row[x] * w + x] |= (unsigned char)(1 << r);
                used[row[x]] = 1;
            }
        }

        /* 2. compose: cut each used color's row into nodes, and bucket them
         * by start column, longest first. Ties keep color order. */
        nnodes = 0;
        for (c = 0; c < ncolors; c++) {
            unsigned char *row = map + (size_t)c * w;
            if (!used[c]) {
                continue;
            }
            used[c] = 0;
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
                np = &pool[nnodes++];
                np->color = c;
                np->sx = sx;
                np->mx = mx;
                np->map = row;
                for (link = &heads[sx]; *link && (*link)->mx >= mx;
                     link = &(*link)->next) {
                }
                np->next = *link;
                *link = np;
                starts[sx / 64] |= UINT64_C(1) << (sx % 64);
            }
        }

        /* 3. emit, which also clears the map behind each node */
        put_band(out, heads, starts, words, w, nnodes);
    }
    buf_printf(out, "\033\\");

    free(map);
    free(used);
    free(pool);
    free(heads);
    free(starts);
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
static int
canvas_reserve(canvas_t *cv, int need_w, int need_h)
{
    unsigned char *pixels;
    int w = cv->width;
    int h = cv->height;
    int y;

    if (need_w <= w && need_h <= h) {
        return 0;
    }
    if (need_w > TUTORIAL_MAX_SIDE || need_h > TUTORIAL_MAX_BANDS * 6) {
        return -1;
    }
    while (w < need_w) {
        w = w < 64 ? 64 : w * 2;
    }
    while (h < need_h) {
        h = h < 64 ? 64 : h * 2;
    }
    w = w > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : w;
    h = h > TUTORIAL_MAX_BANDS * 6 ? TUTORIAL_MAX_BANDS * 6 : h;
    pixels = calloc((size_t)w * h, 1);
    if (pixels == NULL) {
        return -1;
    }
    for (y = 0; y < cv->height; y++) {
        memcpy(pixels + (size_t)y * w, cv->pixels + (size_t)y * cv->width,
               (size_t)cv->width);
    }
    free(cv->pixels);
    cv->pixels = pixels;
    cv->width = w;
    cv->height = h;
    return 0;
}

/* Extend the painted height by the lowest row painted in a band. */
static void
used_rows(canvas_t *cv, int y, int band_rows)
{
    int r = 5;

    if (band_rows == 0) {
        return;
    }
    while ((band_rows >> r & 1) == 0) {
        r--;
    }
    if (y + r + 1 > cv->used_height) {
        cv->used_height = y + r + 1;
    }
}

int
decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    canvas_t cv = { NULL, 0, 0, 0, 0 };
    unsigned char *row;
    unsigned char *dst;
    unsigned int mask;
    int last;
    int a[5];
    int n;
    int x = 0;
    int y = 0;
    int color = 0;
    int band_rows = 0;      /* rows painted in this band */
    int repeat;
    int bits;
    int r;
    int ch;

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
                for (r = 0; r < 6; r++) {
                    mask = 0u - ((unsigned int)bits >> r & 1u);
                    *dst = (unsigned char)((*dst & ~mask) |
                                           ((unsigned int)color & mask));
                    dst += cv.width;
                }
                band_rows |= bits;
                if (bits != 0) {
                    last = x;
                }
                x++;
            }
            if (last >= cv.used_width) {
                cv.used_width = last + 1;
            }
            if (p == end || *p == 0x1b) {
                break;
            }
        }
        ch = *p++;
        if (ch == '#' && parse_plain_number(&p, end, &n)) {
            color = n > 255 ? 255 : n;
            if (color >= img->ncolors) {
                img->ncolors = color + 1;
            }
            continue;
        }
        repeat = 1;
        if (ch == '!') {
            if (!parse_plain_number(&p, end, &repeat)) {
                parse_params(&p, end, &repeat, 1);
            }
            repeat = repeat > 0 ? repeat : 1;
            if (p == end) {
                break;
            }
            ch = *p++;
        }
        if (ch >= '?' && ch <= '~') {
            bits = ch - '?';
            if (bits != 0) {
                if (x + repeat > cv.width || y + 6 > cv.height) {
                    if (x + repeat > TUTORIAL_MAX_SIDE ||
                            canvas_reserve(&cv, x + repeat, y + 6) != 0) {
                        goto fail;
                    }
                }
                for (r = 0; r < 6; r++) {
                    if ((bits >> r & 1) == 0) {
                        continue;
                    }
                    dst = cv.pixels + (size_t)(y + r) * cv.width + x;
                    memset(dst, color, (size_t)repeat);
                }
                band_rows |= bits;
                if (x + repeat > cv.used_width) {
                    cv.used_width = x + repeat;
                }
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
                if (canvas_reserve(&cv, a[2], a[3]) != 0) {
                    goto fail;
                }
                cv.used_width = a[2] > cv.used_width ? a[2] : cv.used_width;
                cv.used_height = a[3] > cv.used_height ? a[3] : cv.used_height;
            }
        }
    }
    used_rows(&cv, y, band_rows);
    if (cv.used_width == 0 || cv.used_height == 0 ||
            image_alloc(img, cv.used_width, cv.used_height) != 0) {
        goto fail;
    }
    for (y = 0; y < cv.used_height; y++) {
        memcpy(img->pixels + (size_t)y * cv.used_width,
               cv.pixels + (size_t)y * cv.width, (size_t)cv.used_width);
    }
    free(cv.pixels);
    return 0;
fail:
    free(cv.pixels);
    return -1;
}
