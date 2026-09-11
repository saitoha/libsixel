/* Verify that every builtin format propagates an interrupted frame callback. */

#include <stdio.h>

#include "loader_builtin_memory_test_common.h"

typedef struct callback_fixture {
    char const *label;
    char const *path;
} callback_fixture_t;

static SIXELSTATUS
loader0156_interrupt(sixel_frame_t *frame, void *data)
{
    unsigned int *callback_count;

    callback_count = (unsigned int *)data;
    if (frame == NULL || callback_count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    ++*callback_count;
    return SIXEL_INTERRUPTED;
}

int
test_loader_0156_loader_builtin_callback_status_matrix(int argc, char **argv)
{
    static callback_fixture_t const fixtures[] = {
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
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    size_t fixture_index;
    unsigned int callback_count;
    int result;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    status = SIXEL_FALSE;
    allocator = NULL;
    fixture_index = 0u;
    callback_count = 0u;
    result = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (fixture_index = 0u;
         fixture_index < sizeof(fixtures) / sizeof(fixtures[0]);
         ++fixture_index) {
        callback_count = 0u;
        status = SIXEL_FALSE;
        result = edge_load_fixture_custom(fixtures[fixture_index].label,
                                          fixtures[fixture_index].path,
                                          &options,
                                          allocator,
                                          loader0156_interrupt,
                                          &callback_count,
                                          &status);
        if (result != 0 || status != SIXEL_INTERRUPTED ||
            callback_count != 1u) {
            result = 1;
            fprintf(stderr,
                    "%s callback: result=%d status=%d callbacks=%u\n",
                    fixtures[fixture_index].label,
                    result,
                    (int)status,
                    callback_count);
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    sixel_allocator_unref(allocator);
    return result;
}
