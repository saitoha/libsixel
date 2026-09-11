/* Fix singular cHRM fallback to the usable gAMA interpretation. */

#include <string.h>

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "loader_builtin_png_color_test_common.h"

int
test_loader_0175_loader_builtin_png_singular_chrm_numeric(
    int argc,
    char **argv)
{
    unsigned char png[EDGE_BUFFER_CAPACITY];
    edge_loader_options_t options;
    edge_frame_probe_t actual;
    edge_frame_probe_t expected;
    SIXELSTATUS actual_status;
    SIXELSTATUS expected_status;
    size_t png_size;
    size_t chrm_offset;
    size_t chrm_size;

    (void)argc;
    (void)argv;
    memset(&actual, 0, sizeof(actual));
    memset(&expected, 0, sizeof(expected));
    actual_status = SIXEL_FALSE;
    expected_status = SIXEL_FALSE;
    png_size = 0u;
    chrm_offset = 0u;
    chrm_size = 0u;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    if (edge_read_fixture(
            "/tests/data/colormgmt/input/png/rgb/"
            "img_rgb_icc0_srgb0_chrm1_gama1.png",
            png,
            sizeof(png),
            &png_size) != 0 ||
        !png_color_find_chunk(png,
                              png_size,
                              "cHRM",
                              &chrm_offset,
                              &chrm_size) ||
        chrm_size != 44u) {
        return 1;
    }
    memcpy(png + chrm_offset + 8u + 16u,
           png + chrm_offset + 8u + 8u,
           8u);
    memcpy(png + chrm_offset + 8u + 24u,
           png + chrm_offset + 8u + 8u,
           8u);
    if (edge_load_buffer_options("PNG singular cHRM",
                                 png,
                                 png_size,
                                 &options,
                                 &actual,
                                 &actual_status) != 0 ||
        edge_load_fixture_options(
            "PNG gAMA reference",
            "/tests/data/colormgmt/input/png/rgb/"
            "img_rgb_icc0_srgb0_chrm0_gama1.png",
            &options,
            &expected,
            &expected_status) != 0 ||
        actual_status != SIXEL_OK || expected_status != SIXEL_OK ||
        actual.callback_count != 1 || expected.callback_count != 1 ||
        actual.pixelformat[0] != expected.pixelformat[0] ||
        actual.colorspace[0] != expected.colorspace[0] ||
        actual.rgb_size[0] != expected.rgb_size[0] ||
        memcmp(actual.rgb[0],
               expected.rgb[0],
               actual.rgb_size[0]) != 0) {
        return 1;
    }
    return 0;
}
