/* Verify baseline allocation-failure cleanup across builtin formats. */

#include "loader_builtin_memory_test_common.h"

typedef struct allocator_fixture {
    char const *label;
    char const *path;
} allocator_fixture_t;

int
test_loader_0155_loader_builtin_allocator_failure_matrix(int argc,
                                                          char **argv)
{
    static allocator_fixture_t const fixtures[] = {
        { "PNG", "/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png" },
        { "JPEG", "/tests/data/inputs/formats/"
                  "snake-jpeg-8bit-rgb-seq444.jpg" },
        { "GIF", "/tests/data/inputs/snake_64.gif" },
        { "WebP", "/tests/data/inputs/snake_16.webp" },
        { "HDR", "/tests/data/inputs/formats/stbi_minimal.hdr" },
        { "PSD", "/tests/data/inputs/formats/stbi_minimal.psd" },
        { "BMP", "/tests/data/inputs/formats/"
                 "bmp-info40-topdown-24bpp-2x2.bmp" },
        { "TGA", "/tests/data/inputs/formats/tga-rgba-2x2-top-left.tga" },
        { "PIC", "/tests/data/inputs/formats/stbi_minimal.pic" },
        { "PNM", "/tests/data/inputs/snake_64.ppm" },
        { "SIXEL", "/tests/data/inputs/snake_64.six" }
    };
    edge_loader_options_t options;
    size_t fixture_index;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    for (fixture_index = 0u;
         fixture_index < sizeof(fixtures) / sizeof(fixtures[0]);
         ++fixture_index) {
        if (edge_expect_fixture_allocation_failures(
                fixtures[fixture_index].label,
                fixtures[fixture_index].path,
                &options,
                0) != 0) {
            return 1;
        }
    }
    return 0;
}
