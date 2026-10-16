/* SPDX-License-Identifier: MIT */
/* Step 1: a complete SIXEL encoder and decoder with run-length coding. */
#include "tutorial.h"

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

    while (p < end && *p++ != 'q') {}                    /* skip ESC P ... q */
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
