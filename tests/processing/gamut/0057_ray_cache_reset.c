/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_test_common.h"

int
test_gamut_0057_ray_cache_reset(int argc, char **argv)
{
    sixel_encoder_t *encoder = NULL;
    unsigned char retained[3] = {0, 0, 0};
    int ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U',
            "gamma:gamut=edge-seeker")) &&
        SIXEL_SUCCEEDED(sixel_encoder_set_accumulation_buffer(encoder,
            retained, 1, 1, SIXEL_PIXELFORMAT_RGB888)) &&
        encoder->accumulation_valid &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U',
            "gamma:gamut=ray-trace")) && !encoder->accumulation_valid;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
