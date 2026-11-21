/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

int
test_png_height_limit(int argc, char **argv)
{
    unsigned char pixel[] = {24, 83, 151};
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    /* Keep a missing height check from opening a file or reading pixels. */
    status = sixel_helper_write_image_file(pixel, 1, SIXEL_HEIGHT_LIMIT + 1,
                                         NULL, SIXEL_PIXELFORMAT_RGB888,
                                         NULL, SIXEL_FORMAT_PNG, NULL);
    return status == SIXEL_BAD_INPUT ? EXIT_SUCCESS : EXIT_FAILURE;
}
