/* SPDX-License-Identifier: MIT */
/* Verify threshold limit reject 256. */
#include <sixel.h>
#include "src/encoder.h"

int
test_delta_palette_0128(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    int failed;

    (void)argc;
    (void)argv;
    encoder = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) return 1;
    failed = sixel_encoder_set_6delta_threshold_limit(encoder, 0) != SIXEL_OK;
    /* A rejected value must retain the previously installed limit. */
    failed = failed ||
        sixel_encoder_set_6delta_threshold_limit(encoder, 256) !=
            SIXEL_BAD_ARGUMENT || encoder->sixdelta_threshold_limit != 0;
    sixel_encoder_unref(encoder);
    return failed;
}
