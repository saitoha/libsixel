/* SPDX-License-Identifier: MIT */
/* Indexed serialization reconciles byte edits when application is bypassed */
#include "retention_test_common.h"

int
test_palette_retention_indexed_write(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float assigned[6] = {64.f/255, 64.f/255, 64.f/255,
        131.f/255, 131.f/255, 131.f/255};
    unsigned char indices[2] = {0, 1};
    unsigned char *bytes;
    sixel_dither_t *dither;
    sixel_output_t *output;
    retention_capture_t capture;
    int dark;
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
    bytes = retention_bytes(dither);
    dark = bytes[0] < bytes[3] ? 0 : 1;
    bytes[dark * 3] = bytes[dark * 3 + 1] = bytes[dark * 3 + 2] = 64;
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB);
    ok = SIXEL_SUCCEEDED(sixel_encode(indices, 2, 1, 0, dither, output)) &&
        strstr(capture.bytes, ";2;25;25;25#") != NULL &&
        strstr(capture.bytes, ";2;51;51;51#") != NULL &&
        retention_pair(dither, assigned);
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
