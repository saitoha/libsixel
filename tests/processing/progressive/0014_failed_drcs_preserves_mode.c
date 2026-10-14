/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

int
test_progressive_0014(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    uint64_t revision;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    revision = encoder->configuration_revision;
    ok = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DRCS, "99:1")
        == SIXEL_BAD_ARGUMENT;
    ok = ok && !encoder->fdrcs && encoder->tile_outfd == -1 &&
        encoder->configuration_revision == revision;
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
