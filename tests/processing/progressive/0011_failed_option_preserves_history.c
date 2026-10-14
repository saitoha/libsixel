/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

int
test_progressive_0011(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    unsigned char pixel[3];
    unsigned char *history;
    uint64_t revision;
    uint64_t configuration;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    memset(pixel, 0, sizeof(pixel));
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    ok = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_ALPHA_POLICY,
                              "clear") == SIXEL_OK &&
        sixel_encoder_set_accumulation_buffer(encoder, pixel, 1, 1,
                                              SIXEL_PIXELFORMAT_RGB888)
            == SIXEL_OK;
    history = encoder->accumulation_pixels;
    revision = encoder->history_revision;
    configuration = encoder->configuration_revision;
    ok = ok && sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                                    "delta:threshold=9") == SIXEL_BAD_ARGUMENT;
    ok = ok && !encoder->sixdelta_enabled &&
        encoder->sixdelta_threshold == 0 &&
        encoder->accumulation_valid &&
        encoder->accumulation_pixels == history &&
        encoder->history_revision == revision &&
        encoder->configuration_revision == configuration;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
