/* SPDX-License-Identifier: MIT */
/* A reused decoder must not import another capture's colors or canvas. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "../../../src/decoder.h"

int test_sequence_0083(int argc, char **argv)
{
    static unsigned char const first[] =
        "\033P0;1q\"1;1;1;1#0;2;100;0;0@\033\\";
    static unsigned char const second[] =
        "\033P0;1q\"1;1;1;1#0@\033\\";
    sixel_decoder_t *decoder;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    memset(&result, 0, sizeof(result));
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    decoder->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    decoder->gpu_policy = SIXEL_GPU_POLICY_OFF;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6delta-progressive");
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_decoder_decode_pixels(decoder, first,
            sizeof(first) - 1U, NULL, &result);
    }
    ok = SIXEL_SUCCEEDED(status) && result.pixels != NULL;
    sixel_allocator_free(decoder->allocator, result.pixels);
    result.pixels = NULL;
    status = sixel_decoder_decode_pixels(decoder, second,
        sizeof(second) - 1U, NULL, &result);
    ok = ok && SIXEL_FAILED(status) && result.pixels == NULL &&
        decoder->input_passes == 0U;
    sixel_allocator_free(decoder->allocator, result.pixels);
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
