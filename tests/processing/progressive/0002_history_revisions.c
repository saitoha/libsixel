/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/frame.h"

int
test_progressive_0002(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    unsigned char pixel[3];
    uint64_t history;
    uint64_t configuration;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    memset(pixel, 0, sizeof(pixel));
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    history = encoder->history_revision;
    ok = sixel_encoder_set_accumulation_buffer(encoder, pixel, 1, 1,
            SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK;
    ok = ok && encoder->history_revision == history + 1;
    history = encoder->history_revision;
    configuration = encoder->configuration_revision;
    ok = ok && sixel_encoder_set_6delta_plane_origin(encoder, 0, 0)
        == SIXEL_OK;
    ok = ok && sixel_encoder_set_6delta_threshold_limit(encoder, 0)
        == SIXEL_OK;
    ok = ok && encoder->history_revision == history &&
        encoder->configuration_revision == configuration &&
        encoder->accumulation_valid;
    ok = ok && sixel_encoder_invalidate_6delta_plane(encoder) == SIXEL_OK &&
        encoder->history_revision == history + 1 &&
        !encoder->accumulation_valid;
    /* Exhaustion is sticky and cannot revive an ancient binding. */
    encoder->history_revision = UINT64_MAX;
    ok = ok && sixel_encoder_invalidate_6delta_plane(encoder) == SIXEL_OK &&
        encoder->history_revision == UINT64_MAX && encoder->revision_exhausted;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
