/*
 * Reproducer for GHSA-wpx3-h5g8-qr3w.
 *
 * sixel_decode() has the same palette allocation check as
 * sixel_decode_raw().  The raster attributes keep the deprecated wrapper's
 * initial 2048x2048 buffer from being resized before the palette allocation,
 * making the third malloc call the palette allocation.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

static int malloc_count;

static void *
fail_palette_malloc(size_t size)
{
    ++malloc_count;
    if (malloc_count == 3) {
        return NULL;
    }

    return malloc(size);
}

int
main(void)
{
    static unsigned char sixel[] = "\033Pq\"1;1;2048;2048q\033\\";
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;

    pixels = NULL;
    palette = NULL;
    width = 0;
    height = 0;
    ncolors = 0;

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    status = sixel_decode(sixel,
                          (int)strlen((char *)sixel),
                          &pixels,
                          &width,
                          &height,
                          &palette,
                          &ncolors,
                          fail_palette_malloc);
#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic pop
#endif

    free(pixels);
    free(palette);

    return status == SIXEL_BAD_ALLOCATION ? 0: 1;
}
