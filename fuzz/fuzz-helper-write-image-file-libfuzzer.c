/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#define FUZZ_MAX_INPUT_BYTES (4u * 1024u * 1024u)
#define FUZZ_HEADER_BYTES 4u
#define FUZZ_MAX_WIDTH 64
#define FUZZ_MAX_HEIGHT 64
#define FUZZ_PALETTE_BYTES (256u * 3u)

static int
fuzz_pick_pixelformat(unsigned int selector)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_PAL1,
        SIXEL_PIXELFORMAT_PAL2,
        SIXEL_PIXELFORMAT_PAL4,
        SIXEL_PIXELFORMAT_PAL8,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_PIXELFORMAT_G8,
        SIXEL_PIXELFORMAT_RGB565,
        SIXEL_PIXELFORMAT_RGB555,
        SIXEL_PIXELFORMAT_BGR565,
        SIXEL_PIXELFORMAT_BGR555,
        SIXEL_PIXELFORMAT_GA88,
        SIXEL_PIXELFORMAT_AG88,
        SIXEL_PIXELFORMAT_BGR888,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32,
        SIXEL_PIXELFORMAT_RGBA8888,
        SIXEL_PIXELFORMAT_ARGB8888,
        SIXEL_PIXELFORMAT_BGRA8888,
        SIXEL_PIXELFORMAT_ABGR8888
    };
    size_t format_count;

    format_count = sizeof(formats) / sizeof(formats[0]);
    return formats[selector % format_count];
}

static int
fuzz_pick_imageformat(unsigned int selector)
{
    static int const formats[] = {
        SIXEL_FORMAT_PNG,
        SIXEL_FORMAT_GIF,
        SIXEL_FORMAT_BMP,
        SIXEL_FORMAT_JPG,
        SIXEL_FORMAT_TGA,
        SIXEL_FORMAT_WBMP,
        SIXEL_FORMAT_TIFF,
        SIXEL_FORMAT_SIXEL,
        SIXEL_FORMAT_PNM,
        SIXEL_FORMAT_GD2,
        SIXEL_FORMAT_PSD,
        SIXEL_FORMAT_HDR
    };
    size_t format_count;

    format_count = sizeof(formats) / sizeof(formats[0]);
    return formats[selector % format_count];
}

static int
fuzz_requires_palette(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        return 1;
    default:
        return 0;
    }
}

static void
fuzz_fill_buffer(unsigned char *dst,
                 size_t dst_size,
                 uint8_t const *src,
                 size_t src_size)
{
    size_t i;

    if (dst == NULL || dst_size == 0u) {
        return;
    }
    if (src == NULL || src_size == 0u) {
        memset(dst, 0, dst_size);
        return;
    }

    for (i = 0u; i < dst_size; ++i) {
        dst[i] = (unsigned char)src[i % src_size];
    }
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    int width;
    int height;
    int pixelformat;
    int imageformat;
    int depth;
    size_t pixel_total;
    size_t pixel_bytes;
    unsigned char *pixels;
    unsigned char palette[FUZZ_PALETTE_BYTES];
    unsigned char *palette_ptr;
    uint8_t const *payload;
    size_t payload_size;

    if (data == NULL || size > FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    width = 1 + (int)(data[0] % FUZZ_MAX_WIDTH);
    height = 1 + (int)(data[1] % FUZZ_MAX_HEIGHT);
    pixelformat = fuzz_pick_pixelformat(data[2]);
    imageformat = fuzz_pick_imageformat(data[3]);
    payload = NULL;
    payload_size = 0u;
    palette_ptr = NULL;

    if (size > FUZZ_HEADER_BYTES) {
        payload = data + FUZZ_HEADER_BYTES;
        payload_size = size - FUZZ_HEADER_BYTES;
    }

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        return 0;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total / (size_t)width != (size_t)height) {
        return 0;
    }
    if (pixel_total > SIZE_MAX / (size_t)depth) {
        return 0;
    }
    pixel_bytes = pixel_total * (size_t)depth;

    pixels = (unsigned char *)malloc(pixel_bytes);
    if (pixels == NULL) {
        return 0;
    }
    fuzz_fill_buffer(pixels, pixel_bytes, payload, payload_size);

    if (fuzz_requires_palette(pixelformat)) {
        fuzz_fill_buffer(palette, FUZZ_PALETTE_BYTES, payload, payload_size);
        palette_ptr = palette;
    }

    (void)sixel_helper_write_image_file(pixels,
                                        width,
                                        height,
                                        palette_ptr,
                                        pixelformat,
                                        "/dev/null",
                                        imageformat,
                                        NULL);
    free(pixels);
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
