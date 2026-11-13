/* SPDX-License-Identifier: MIT */
/* Verify painted pixels null. */
#include <sixel.h>
#include "src/encoder.h"

int
test_delta_palette_0127(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_encoder_get_6delta_painted_pixels(NULL) != 0u;
}
