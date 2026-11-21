/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>

static int png_null_path_live_allocations;

static void *
png_null_path_malloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);
    if (ptr != NULL) {
        ++png_null_path_live_allocations;
    }
    return ptr;
}

static void
png_null_path_free(void *ptr)
{
    if (ptr != NULL) {
        --png_null_path_live_allocations;
    }
    free(ptr);
}

int
test_png_null_filename_cleanup(int argc, char **argv)
{
    unsigned char pixel[] = {255, 24, 83, 151};
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    png_null_path_live_allocations = 0;
    status = sixel_allocator_new(&allocator, png_null_path_malloc, NULL,
                                  NULL, png_null_path_free);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }

    /* ARGB needs a temporary RGBA buffer before output is opened. */
    status = sixel_helper_write_image_file(pixel, 1, 1, NULL,
                                         SIXEL_PIXELFORMAT_ARGB8888, NULL,
                                         SIXEL_FORMAT_PNG, allocator);
    sixel_allocator_unref(allocator);
    if (status != SIXEL_BAD_ARGUMENT || png_null_path_live_allocations != 0) {
        fprintf(stderr, "PNG null filename: status=0x%x, live=%d\n",
                (unsigned int)status, png_null_path_live_allocations);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
