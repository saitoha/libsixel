/*
 * Reproducer for GHSA-wpx3-h5g8-qr3w.
 *
 * sixel_decode_raw() used to test the address of the output parameter
 * after allocating the palette.  This allocator fails exactly at the
 * palette allocation so the vulnerable code continues and writes through
 * the NULL value stored in palette.
 */

#include "config.h"

#include <stdlib.h>

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
    static unsigned char const sixel[] = "";
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;

    allocator = NULL;
    pixels = NULL;
    palette = NULL;
    width = 0;
    height = 0;
    ncolors = 0;

    status = sixel_allocator_new(&allocator,
                                 fail_palette_malloc,
                                 NULL,
                                 NULL,
                                 NULL);
    if (SIXEL_FAILED(status)) {
        return 2;
    }

    status = sixel_decode_raw((unsigned char *)sixel,
                              0,
                              &pixels,
                              &width,
                              &height,
                              &palette,
                              &ncolors,
                              allocator);

    sixel_allocator_unref(allocator);
    free(pixels);
    free(palette);

    return status == SIXEL_BAD_ALLOCATION ? 0: 1;
}
