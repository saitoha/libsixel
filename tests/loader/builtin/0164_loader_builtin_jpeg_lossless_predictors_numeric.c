/* Fix lossless JPEG predictors 1 through 7 with point transform two. */

#include <stdio.h>

#include "loader_builtin_memory_test_common.h"
#include "jpeg_lossless_test_common.h"

int
test_loader_0164_loader_builtin_jpeg_lossless_predictors_numeric(
    int argc,
    char **argv)
{
    static size_t const pixels[3] = { 4u, 7u, 8u };
    static float const expected[9] = {
        0.564705908f, 0.564705908f, 0.564705908f,
        0.941176474f, 0.941176474f, 0.941176474f,
        0.376470596f, 0.376470596f, 0.376470596f
    };
    unsigned char jpeg[512];
    edge_loader_options_t options;
    size_t jpeg_size;
    int predictor;
    int result;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    jpeg_size = 0u;
    predictor = 0;
    result = 0;
    for (predictor = 1; predictor <= 7; ++predictor) {
        jpeg_size = jpeg_lossless_build_gray_predictor(
            jpeg,
            sizeof(jpeg),
            predictor,
            2);
        if (jpeg_size == 0u ||
            edge_expect_buffer_float_samples(
                "JPEG lossless predictor",
                jpeg,
                jpeg_size,
                &options,
                3,
                3,
                SIXEL_PIXELFORMAT_RGBFLOAT32,
                SIXEL_COLORSPACE_GAMMA,
                pixels,
                expected,
                0.0000001f) != 0) {
            fprintf(stderr, "JPEG lossless predictor %d failed\n", predictor);
            result = 1;
        }
    }
    return result;
}
