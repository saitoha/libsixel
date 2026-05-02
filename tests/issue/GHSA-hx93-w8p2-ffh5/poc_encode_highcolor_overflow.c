/*
 * Regression test for GHSA-hx93-w8p2-ffh5.
 *
 * The vulnerable high-color encoder calculated work-buffer sizes with
 * signed int arithmetic.  65536 * 65536 wraps to zero on typical
 * two's-complement int implementations, so the first high-color
 * allocation could become smaller than the number of input pixels.
 *
 * This test uses a custom allocator to observe the requested allocation
 * size and stops before the encoder can enter the pixel loop.  It does
 * not need a multi-gigabyte input buffer; seeing an allocation request
 * smaller than width * height is enough to reproduce the root cause.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

static size_t minimum_allocation;
static int guard_enabled;
static int undersized_request;


static void *
watched_malloc(size_t size)
{
    if (guard_enabled && size < minimum_allocation) {
        undersized_request = 1;
        return NULL;
    }

    return malloc(size);
}


static int
discard_write(char *data, int size, void *priv)
{
    (void) data;
    (void) priv;

    return size;
}


int
main(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    sixel_allocator_t *allocator = NULL;
    sixel_dither_t *dither = NULL;
    sixel_output_t *output = NULL;
    unsigned char pixels[3];
    int width;
    int height;

    width = 65536;
    height = 65536;

    if ((size_t)-1 <= 4294967295UL) {
        nret = EXIT_SUCCESS;
        goto end;
    }

    pixels[0] = 0;
    pixels[1] = 0;
    pixels[2] = 0;

    minimum_allocation = (size_t)width * (size_t)height;

    status = sixel_allocator_new(&allocator,
                                 watched_malloc,
                                 NULL,
                                 NULL,
                                 free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_dither_new(&dither, -1, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_output_new(&output, discard_write, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    guard_enabled = 1;
    (void) sixel_encode(pixels, width, height, 3, dither, output);
    guard_enabled = 0;

    if (undersized_request) {
        fprintf(stderr, "high-color allocation size wrapped\n");
        goto end;
    }

    nret = EXIT_SUCCESS;

end:
    guard_enabled = 0;
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return nret;
}
