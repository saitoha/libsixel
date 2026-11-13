/* SPDX-License-Identifier: MIT */
/* Verify refine initially unneeded. */
#include <sixel.h>
#include "src/encoder.h"

int
test_delta_palette_0126(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    int failed;

    (void)argc;
    (void)argv;
    encoder = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) return 1;
    failed = sixel_encoder_needs_6delta_refinement(encoder) != 0;
    sixel_encoder_unref(encoder);
    return failed;
}
