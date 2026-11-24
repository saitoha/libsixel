/* SPDX-License-Identifier: MIT */
/* Count palette exports while retaining composition from skipped frames. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/frame.h"
#include "src/fromgif.h"
#include "tests/_support/loader/builtin/loader_builtin_gif_test_common.h"

static int selected_gif_exports;

static void *
selected_gif_malloc(size_t size)
{
    /* A 17x17 PAL8 export has a distinct allocation size. Canvas buffers,
     * palette/hash storage and aligned object allocations have other sizes. */
    if (size == 17u * 17u) {
        ++selected_gif_exports;
    }
    return malloc(size);
}

static SIXELSTATUS
selected_gif_frame(sixel_frame_t *frame, void *context)
{
    int *count;
    int i;
    unsigned char const *pixels;
    unsigned char const *palette;

    count = context;
    if (sixel_frame_get_width(frame) != 17 ||
        sixel_frame_get_height(frame) != 17 ||
        sixel_frame_format(frame) != SIXEL_PIXELFORMAT_PAL8 ||
        sixel_frame_get_ncolors(frame) != 4 ||
        sixel_frame_get_frame_no(frame) != 0) {
        return SIXEL_BAD_INPUT;
    }
    pixels = sixel_frame_get_pixels(frame);
    palette = sixel_frame_get_palette(frame);
    for (i = 0; i < 17 * 17; ++i) {
        if (pixels[i] >= 4 || memcmp(palette + pixels[i] * 3,
                edge_palette_rgb + (i < 3 ? i + 1 : 0) * 3, 3u) != 0) {
            return SIXEL_BAD_INPUT;
        }
    }
    ++*count;
    return SIXEL_OK;
}

int
test_gif_selected_frame_export(int argc, char **argv)
{
    unsigned char data[256];
    unsigned char pixel;
    edge_writer_t writer;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    union {
        sixel_load_image_function fn;
        void *p;
    } callback;
    int i;
    int count;

    (void)argc;
    (void)argv;
    writer.buffer = data;
    writer.capacity = sizeof(data);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 17, 17);
    for (i = 0; i < 3; ++i) {
        pixel = (unsigned char)(i + 1);
        edge_gif_graphic_control(&writer, 1);
        edge_gif_image(&writer, (unsigned int)i, 0, 1, 1, 0,
                       &pixel, 1u, 0);
    }
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed) {
        return 1;
    }
    allocator = NULL;
    status = sixel_allocator_new(&allocator, selected_gif_malloc,
                                  NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    selected_gif_exports = 0;
    count = 0;
    callback.fn = selected_gif_frame;
    status = load_gif(data, (int)writer.length, NULL, 0, 16, 1, 1,
                      SIXEL_LOOP_DISABLE, 2, callback.p, &count, NULL,
                      allocator);
    sixel_allocator_unref(allocator);
    if (status != SIXEL_OK || count != 1 || selected_gif_exports != 1) {
        fprintf(stderr, "GIF selection: status=%x frames=%d exports=%d\n",
                status, count, selected_gif_exports);
        return 1;
    }
    return 0;
}
