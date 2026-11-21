/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

static int png_fail_packed_allocations;

static void *
png_fail_packed_malloc(size_t size)
{
    return png_fail_packed_allocations ? NULL : malloc(size);
}

int
test_png_packed_allocation_failure(int argc, char **argv)
{
    unsigned char pixel[] = {0x80};
    unsigned char palette[] = {0, 0, 0, 255, 255, 255};
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    png_fail_packed_allocations = 0;
    status = sixel_allocator_new(&allocator, png_fail_packed_malloc,
                                  NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }

    /* Fail the allocation of the expanded index plane before any write. */
    png_fail_packed_allocations = 1;
    status = sixel_helper_write_image_file(pixel, 1, 1, palette,
                                         SIXEL_PIXELFORMAT_PAL1, "-",
                                         SIXEL_FORMAT_PNG, allocator);
    png_fail_packed_allocations = 0;
    sixel_allocator_unref(allocator);
    return status == SIXEL_BAD_ALLOCATION ? EXIT_SUCCESS : EXIT_FAILURE;
}
