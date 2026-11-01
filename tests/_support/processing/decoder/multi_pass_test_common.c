/* SPDX-License-Identifier: MIT */
/* Literal capture pixels are an oracle independent of the encoder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "../../../../src/decoder.h"
#include "multi_pass_test_common.h"

int test_multi_pass_capture(char const *stream, size_t size,
    unsigned char const *expected, int width, int height, size_t passes,
    char const *mode)
{
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    int ok;

    decoder = NULL;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    decoder->gpu_policy = SIXEL_GPU_POLICY_OFF;
    decoder->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    if (strcmp(mode, "single") != 0) {
        status = sixel_decoder_setopt(decoder,
            SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6delta-progressive");
    }
    if (strcmp(mode, "force") == 0) {
        decoder->gpu_policy = SIXEL_GPU_POLICY_FORCE;
    }
    if (strcmp(mode, "dequant") == 0) {
        decoder->dequantize_method = SIXEL_DEQUANTIZE_K_UNDITHER;
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_decoder_decode_pixels(decoder,
            (unsigned char const *)stream, size, &options, &result);
    }
    if (expected == NULL) {
        ok = SIXEL_FAILED(status) && result.pixels == NULL &&
            result.width == 0 && result.height == 0 &&
            decoder->input_passes == 0U;
    } else {
        ok = SIXEL_SUCCEEDED(status) && result.pixels != NULL &&
            result.width == width && result.height == height &&
            decoder->input_passes == passes &&
            result.pixelformat == SIXEL_PIXELFORMAT_RGBA8888 &&
            result.stride == width * 4 &&
            memcmp(result.pixels, expected, (size_t)width * height * 4) == 0;
    }
    if (!ok) {
        fprintf(stderr, "capture status=%x geometry=%dx%d passes=%zu: %s\n",
            status, result.width, result.height, decoder->input_passes,
            sixel_helper_get_additional_message());
    }
    sixel_allocator_free(decoder->allocator, result.pixels);
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
