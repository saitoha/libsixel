/* SPDX-License-Identifier: MIT */
/* Uncached serialization rounds the retained float coordinates */
#include "retention_test_common.h"

int
test_palette_retention_wire(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    sixel_dither_t *dither;
    sixel_output_t *output;
    retention_capture_t capture;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    output = NULL;
    memset(&capture, 0, sizeof(capture));
    ok = 0;
    if (!retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_QUANTIZE_MODEL_KMEANS) ||
            SIXEL_FAILED(sixel_output_new(&output, retention_write,
                &capture, NULL))) {
        goto end;
    }
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB);
    /* 1.49% rounds to 1; byte 4/255 would incorrectly round to 2.
     * Both decisions have ample margin over float representation error. */
    ok = SIXEL_SUCCEEDED(sixel_encode((unsigned char *)pixels, 2, 1,
        0, dither, output)) && strstr(capture.bytes, ";2;1;1;1#") != NULL &&
        strstr(capture.bytes, ";2;51;51;51#") != NULL &&
        retention_pair(dither, pixels);
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
