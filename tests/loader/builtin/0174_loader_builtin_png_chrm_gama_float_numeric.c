/* Fix cHRM plus gAMA matrix conversion at float precision. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "loader_builtin_png_color_test_common.h"

int
test_loader_0174_loader_builtin_png_chrm_gama_float_numeric(int argc,
                                                             char **argv)
{
    unsigned char rgba_png[128];
    unsigned char metadata_png[256];
    unsigned char combined_png[256];
    edge_writer_t writer;
    edge_loader_options_t options;
    size_t rgba_size;
    size_t metadata_size;
    size_t chrm_offset;
    size_t chrm_size;
    size_t gama_offset;
    size_t gama_size;
    size_t const sample_pixels[3] = { 0u, 1u, 1u };
    float const expected[9] = {
        0.00000236504457f, 0.0000378407130f, 0.000191568586f,
        0.00147815282f, 0.00306509738f, 0.00567847257f,
        0.00147815282f, 0.00306509738f, 0.00567847257f
    };

    (void)argc;
    (void)argv;
    rgba_size = 0u;
    metadata_size = 0u;
    chrm_offset = 0u;
    chrm_size = 0u;
    gama_offset = 0u;
    gama_size = 0u;
    writer.buffer = combined_png;
    writer.capacity = sizeof(combined_png);
    writer.length = 0u;
    writer.failed = 0;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    if (edge_read_fixture("/tests/data/inputs/formats/rgba.png",
                          rgba_png,
                          sizeof(rgba_png),
                          &rgba_size) != 0 ||
        edge_read_fixture(
            "/tests/data/colormgmt/input/png/rgb/"
            "img_rgb_icc0_srgb0_chrm1_gama1.png",
            metadata_png,
            sizeof(metadata_png),
            &metadata_size) != 0 ||
        !png_color_find_chunk(metadata_png,
                              metadata_size,
                              "cHRM",
                              &chrm_offset,
                              &chrm_size) ||
        !png_color_find_chunk(metadata_png,
                              metadata_size,
                              "gAMA",
                              &gama_offset,
                              &gama_size)) {
        return 1;
    }
    edge_put_bytes(&writer, rgba_png, 33u);
    edge_put_bytes(&writer, metadata_png + chrm_offset, chrm_size);
    edge_put_bytes(&writer, metadata_png + gama_offset, gama_size);
    edge_put_bytes(&writer,
                   rgba_png + 33u,
                   rgba_size - 33u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_buffer_float_samples(
        "PNG cHRM/gAMA float conversion",
        combined_png,
        writer.length,
        &options,
        2,
        1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        sample_pixels,
        expected,
        0.000001f);
}
