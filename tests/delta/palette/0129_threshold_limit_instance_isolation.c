/* SPDX-License-Identifier: MIT */
/* Verify threshold limit instance isolation. */
#include <sixel.h>
#include "src/encoder.h"

int
test_delta_palette_0129(int argc, char **argv)
{
    sixel_encoder_t *encoder, *other;
    int failed;

    (void)argc;
    (void)argv;
    encoder = other = NULL;
    failed = 1;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_new(&other, NULL) != SIXEL_OK) goto end;
    failed = sixel_encoder_set_6delta_threshold_limit(encoder, 0) != SIXEL_OK
        || other->sixdelta_threshold_limit != 255;
end:
    sixel_encoder_unref(encoder);
    sixel_encoder_unref(other);
    return failed;
}
