/* SPDX-License-Identifier: MIT
 * Accepted limits, rejected values and reset semantics use the public API.
 */
#include <stdlib.h>
#include <sixel.h>
#include "src/encoder.h"

int
test_0014_range_strength_option(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    int result;

    (void)argc;
    (void)argv;
    encoder = NULL;
    result = EXIT_FAILURE;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '5',
            "auto:strength=0")) || encoder->palette_range_strength != 0u ||
            !encoder->palette_range_strength_override) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '5', "auto:S100")) ||
            encoder->palette_range_strength != 100u) {
        goto end;
    }
    if (SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '5',
            "auto:strength=101")) ||
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '5',
            "auto:strength=-1")) ||
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '5',
            "hard:strength=50"))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '5', "auto")) ||
            encoder->palette_range_strength_override) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_encoder_unref(encoder);
    return result;
}
