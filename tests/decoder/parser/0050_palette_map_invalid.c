/* SPDX-License-Identifier: MIT */
/* A nonnull transform without a callback is invalid on every public route. */
#include <sixel.h>
#include <stdlib.h>
#include <string.h>

int test_pmap_0048(int argc, char **argv)
{
    sixel_palette_transform_t transform;
    sixel_decode_result_t result;
    sixel_decoder_t *decoder;
    unsigned char const stream[] = "\033Pq#2~\033\\";
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    memset(&transform, 0, sizeof(transform));
    memset(&result, 0, sizeof(result));
    decoder = NULL;
    status = sixel_decode_pixels_mapped(stream, sizeof(stream) - 1U, NULL,
                                        &transform, &result, NULL);
    ok = status == SIXEL_BAD_ARGUMENT && result.pixels == NULL;
    status = sixel_decode_pixels_body_mapped(
        stream, sizeof(stream) - 1U, NULL, 0U, NULL, &transform, &result, NULL);
    ok &= status == SIXEL_BAD_ARGUMENT && result.pixels == NULL;
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_decoder_decode_mapped(decoder, stream, sizeof(stream) - 1U,
                                         NULL, &transform, &result);
    ok &= status == SIXEL_BAD_ARGUMENT && result.pixels == NULL;
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
