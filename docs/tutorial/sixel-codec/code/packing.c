/* SPDX-License-Identifier: MIT */
/*
 * Pack nodes onto shared lines.
 *
 * Nodes of different colors that do not overlap can share a graphics line.
 * This encoder sorts the nodes of a band by start column, longest first, and
 * fills each line from left to right with every node that still fits, like
 * books on shelves. The band needs fewer '$', '#' and blank columns.
 *
 * The decoder is the first codec's.
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
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

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

/* The first codec's decoder: it needs raster attributes. */
int decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    int a[5], x = 0, y = 0, color = 0, repeat = 1;

    p = find_sixel_body(p, end);                         /* skip ESC P ... q */
    while (p < end && *p != 0x1b) {                      /* until ESC \ */
        int ch = *p++;
        if (ch == '"') {                                 /* raster attributes */
            parse_params(&p, end, a, 4);
            if (image_alloc(img, a[2], a[3]) != 0) return -1;
        } else if (ch == '#') {                          /* color register */
            int n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            if (color >= img->ncolors) img->ncolors = color + 1;
            if (n == 5 && a[1] == 2) set_rgb_pct(img, color, a[2], a[3], a[4]);
        } else if (ch == '!') {                          /* repeat count */
            parse_params(&p, end, &repeat, 1);
        } else if (ch == '$') {                          /* carriage return */
            x = 0;
        } else if (ch == '-') {                          /* next band */
            x = 0, y += 6;
        } else if (ch >= '?' && ch <= '~') {             /* six pixels */
            for (int i = 0; i < (repeat > 0 ? repeat : 1); i++, x++)
                for (int r = 0; r < 6; r++)
                    if ((ch - '?') >> r & 1 && x < img->width && y + r < img->height)
                        img->pixels[(y + r) * img->width + x] = (unsigned char)color;
            repeat = 1;
        }
    }
    return img->pixels != NULL ? 0 : -1;
}
