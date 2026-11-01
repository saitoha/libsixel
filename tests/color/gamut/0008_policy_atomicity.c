/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0008_policy_atomicity(int argc, char **argv)
{
    sixel_encoder_t *encoder = NULL;
    sixel_gamut_t *saved;
    int ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U',
            "display-p3:gamut=local-minde"));
    saved = ok ? encoder->gamut_mapper : NULL;
    ok = ok && saved != NULL &&
        sixel_encoder_setopt(encoder, 'U', "gamma:gamut=oklch") ==
            SIXEL_BAD_ARGUMENT && encoder->gamut_mapper == saved &&
        encoder->output_color_definition == SIXEL_NAMED_COLOR_DISPLAY_P3;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
