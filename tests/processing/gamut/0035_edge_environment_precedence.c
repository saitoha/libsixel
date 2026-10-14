/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_test_common.h"

int
test_gamut_0035_edge_environment_precedence(int argc, char **argv)
{
    sixel_encoder_t *encoder = NULL;
    int ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)) &&
        encoder->output_gamut == SIXEL_GAMUT_EDGE_SEEKER &&
        encoder->gamut_mapper != NULL && encoder->prefer_float32 &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U', "gamma:Gclip")) &&
        encoder->output_gamut == SIXEL_GAMUT_CLIP &&
        encoder->gamut_mapper == NULL && !encoder->prefer_float32;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
