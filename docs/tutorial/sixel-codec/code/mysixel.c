/* SPDX-License-Identifier: MIT */
/*
 * Your own SIXEL encoder and decoder. Fill in the two functions below,
 * then build and run them:
 *
 *   make mysixel
 *   ./mysixel encode heart.pgm                  draw it in a SIXEL terminal
 *   ./mysixel encode heart.pgm | ./step2 decode > heart.ppm
 *   ./mysixel encode heart.pgm | ./mysixel decode > heart.ppm
 *   ./mysixel selftest                          round-trip 50 test images
 *
 * common.c does everything else: reading PNM files, writing PPM files and
 * the command line. step1.c is one possible answer; try before you peek.
 */
#include "tutorial.h"

int
encode_sixel(image_t const *img, buffer_t *out)
{
    /* 1. Start the picture and give its size: ESC P q "1;1;W;H */
    buf_printf(out, "\033Pq\"1;1;%d;%d", img->width, img->height);

    /* 2. Define every color register c < img->ncolors as #c;2;r;g;b.
     *    The palette holds 8-bit values; PCT() turns them into percent:
     *    PCT(img->palette[c * 3 + 0]) is the red of register c. */

    /* 3. Paint the picture band by band (six rows at a time). For each
     *    band and each color: select the color with #c, then write one
     *    character per column, '?' + the six-bit pattern of that color in
     *    that column. Write '$' before every color but the first of a
     *    band, and '-' between bands. The pixel at (x, y) is
     *    img->pixels[y * img->width + x]. */

    /* 4. End the picture: ESC \ */
    buf_printf(out, "\033\\");
    return 0;
}

int
decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    /* Keep four notes: the cursor x, the top row y of the band, the
     * selected color and the repeat count. Read one byte at a time:
     *
     *   "     raster: parse_params(&p, end, a, 4), then
     *               image_alloc(img, a[2], a[3])
     *   #     color: parse_params(&p, end, a, 5); a[0] is the register.
     *               With 5 numbers and a[1] == 2, also call
     *               set_rgb_pct(img, a[0], a[2], a[3], a[4]).
     *   !     repeat: parse_params(&p, end, &repeat, 1)
     *   $     x = 0
     *   -     x = 0 and y += 6
     *   ? ~   paint the set bits of (byte - '?') at column x, then x++
     *
     * Remember to set img->ncolors to one more than the highest register
     * you saw, and return 0 when the image has pixels. */
    (void)p;
    (void)size;
    (void)img;
    return -1;
}
