/* SPDX-License-Identifier: MIT */
/* A first complete SIXEL encoder and decoder with run-length coding. */
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

static void flush(buffer_t *out, int ch, int count)
{
    if (count > 3) buf_printf(out, "!%d%c", count, ch);  /* DECGRI repeat */
    else while (count-- > 0) buf_putc(out, ch);
}

int encode_sixel(image_t const *img, buffer_t *out)
{
    int w = img->width, h = img->height;

    buf_printf(out, "\033Pq\"1;1;%d;%d", w, h);          /* DCS and raster */
    for (int c = 0; c < img->ncolors; c++)               /* palette in % */
        buf_printf(out, "#%d;2;%d;%d;%d", c, PCT(img->palette[c * 3]),
                   PCT(img->palette[c * 3 + 1]), PCT(img->palette[c * 3 + 2]));
    for (int y = 0; y < h; y += 6) {                     /* one band */
        if (y > 0) buf_putc(out, '-');                   /* next band */
        for (int c = 0; c < img->ncolors; c++) {         /* one color */
            int last = -1, run = 0;
            buf_printf(out, c > 0 ? "$#%d" : "#%d", c);  /* CR, select */
            for (int x = 0; x < w; x++) {
                int bits = 0;
                for (int r = 0; r < 6 && y + r < h; r++)
                    if (img->pixels[(y + r) * w + x] == c) bits |= 1 << r;
                if (bits != last) flush(out, last + '?', run), run = 0;
                last = bits, run++;
            }
            if (last != 0) flush(out, last + '?', run);  /* drop blank tail */
        }
    }
    buf_printf(out, "\033\\");                           /* ST */
    return 0;
}

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
