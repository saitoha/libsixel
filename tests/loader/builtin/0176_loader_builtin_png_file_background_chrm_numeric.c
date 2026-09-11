/* Fix bKGD conversion through the same cHRM/gAMA source profile. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "loader_builtin_png_color_test_common.h"

int
test_loader_0176_loader_builtin_png_bkgd_chrm_numeric(
    int argc,
    char **argv)
{
    static unsigned char const bkgd[] = {
        0x00u, 0x00u, 0x00u, 0x01u, 0x62u, 0x4bu, 0x47u, 0x44u,
        0x01u, 0xffu, 0x02u, 0x2du, 0xdeu
    };
    unsigned char palette_png[256];
    unsigned char metadata_png[256];
    unsigned char combined_png[384];
    edge_writer_t writer;
    edge_loader_options_t options;
    size_t palette_size;
    size_t metadata_size;
    size_t chrm_offset;
    size_t chrm_size;
    size_t gama_offset;
    size_t gama_size;
    size_t plte_offset;
    size_t plte_size;
    size_t const sample_pixels[3] = { 1u, 1u, 1u };
    float const expected[9] = {
        0.0634861887f, 0.0f, 0.0f,
        0.0634861887f, 0.0f, 0.0f,
        0.0634861887f, 0.0f, 0.0f
    };

    (void)argc;
    (void)argv;
    palette_size = 0u;
    metadata_size = 0u;
    chrm_offset = 0u;
    chrm_size = 0u;
    gama_offset = 0u;
    gama_size = 0u;
    plte_offset = 0u;
    plte_size = 0u;
    writer.buffer = combined_png;
    writer.capacity = sizeof(combined_png);
    writer.length = 0u;
    writer.failed = 0;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    if (edge_read_fixture(
            "/tests/data/inputs/formats/pal8-trns-key0-gama-only.png",
            palette_png,
            sizeof(palette_png),
            &palette_size) != 0 ||
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
                              &gama_size) ||
        !png_color_find_chunk(palette_png,
                              palette_size,
                              "PLTE",
                              &plte_offset,
                              &plte_size)) {
        return 1;
    }
    edge_put_bytes(&writer, palette_png, 33u);
    edge_put_bytes(&writer, metadata_png + chrm_offset, chrm_size);
    edge_put_bytes(&writer, metadata_png + gama_offset, gama_size);
    edge_put_bytes(&writer,
                   palette_png + plte_offset,
                   plte_size);
    edge_put_bytes(&writer, bkgd, sizeof(bkgd));
    edge_put_bytes(&writer,
                   palette_png + plte_offset + plte_size,
                   palette_size - plte_offset - plte_size);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_buffer_float_samples(
        "PNG cHRM/gAMA file background",
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
