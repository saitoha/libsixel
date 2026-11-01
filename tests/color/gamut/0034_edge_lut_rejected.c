/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0034_edge_lut_rejected(int argc, char **argv)
{
    sixel_encoder_t *encoder = NULL;
    sixel_gamut_t *saved;
    int ok;

    ok = argc == 2 && SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U',
            "display-p3:gamut=edge-seeker"));
    saved = ok ? encoder->gamut_mapper : NULL;
    ok = ok && sixel_encoder_setopt(encoder, 'U', argv[1]) ==
        SIXEL_BAD_ARGUMENT &&
        encoder->output_gamut == SIXEL_GAMUT_EDGE_SEEKER &&
        encoder->output_color_definition == SIXEL_NAMED_COLOR_DISPLAY_P3 &&
        encoder->gamut_mapper == saved &&
        /* A missing/unreadable fixture must not satisfy this rejection. */
        strstr(sixel_helper_get_additional_message(),
            "edge-seeker requires a supported matrix/TRC RGB gamut") != NULL;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
