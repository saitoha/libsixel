/* SPDX-License-Identifier: MIT */
/* Verify the explicit native entry and atomic rejection of invalid input. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"
#include "tests/data/color-numeric/png-samples.h"

int
test_loader_0188_colorsync_linear(int argc, char **argv)
{
    unsigned char bytes[4096];
    float pixels[24];
    float original[24];
    FILE *file;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t selected;
    size_t length;
    double expected;
    int named;
    int invalid;
    int width;
    int converted;
    int result;
    int p;
    int c;

#if !HAVE_COREGRAPHICS
    return 77;
#endif
    --argc;
    ++argv;
    if (argc != 4 || !sixel_cms_engine_from_string(argv[2], &engine)) {
        return 1;
    }
    named = strcmp(argv[0], "named") == 0;
    invalid = atoi(argv[1]);
    length = 0u;
    if (!named) {
        file = sixel_compat_fopen(argv[3], "rb");
        if (file == NULL) {
            return 1;
        }
        length = fread(bytes, 1u, sizeof(bytes), file);
        result = ferror(file) || length == 0u || length == sizeof(bytes);
        result |= fclose(file) != 0;
        if (result) {
            return 1;
        }
    }
    for (p = 0; p < 8; ++p) {
        for (c = 0; c < 3; ++c) {
            pixels[p * 3 + c] = (float)(color_png_source[1][p][c] / 65535.0);
        }
    }
    width = invalid == 3 ? 0 : 4;
    if (invalid == 1) {
        pixels[23] = NAN;
    } else if (invalid == 2) {
        pixels[23] = INFINITY;
    } else if (invalid == 4) {
        length = 3u;
    }
    memcpy(original, pixels, sizeof(pixels));
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    selected = sixel_cms_get_engine();
    converted = sixel_cms_convert_colorsync_linear(pixels, width, 2, named,
        named ? NULL : bytes, length);
    result = sixel_cms_get_engine() != selected;
    sixel_cms_set_engine(previous);
    if (invalid) {
        /* Include the complete prefix and the exact nonfinite source bits.
         * A false return does not excuse an already modified input buffer. */
        return result || converted ||
            memcmp(original, pixels, sizeof(pixels)) != 0;
    }
    if (result || !converted) {
        return 1;
    }
    for (p = 0; p < 8; ++p) {
        for (c = 0; c < 3; ++c) {
            expected = named ? color_png_srgb_linear[1][p][c] :
                color_png_icc_linear[1][p][c];
            if (!isfinite(pixels[p * 3 + c]) ||
                fabs(pixels[p * 3 + c] - expected) > 0.0001) {
                return 1;
            }
        }
    }
    return 0;
}
