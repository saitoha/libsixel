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

#include <sixel.h>

#define FUZZ_MAX_INPUT_BYTES (4u * 1024u * 1024u)
#define FUZZ_HEADER_BYTES 6u

static int
fuzz_choose_pixelformat(unsigned int selector)
{
    switch (selector & 0x03u) {
    case 0u:
        return SIXEL_PIXELFORMAT_RGB888;
    case 1u:
        return SIXEL_PIXELFORMAT_RGBA8888;
    case 2u:
        return SIXEL_PIXELFORMAT_PAL8;
    default:
        return SIXEL_PIXELFORMAT_G8;
    }
}

static size_t
fuzz_compute_pixel_bytes(int width,
                         int height,
                         int pixelformat)
{
    size_t pixel_total;
    size_t depth;
    int depth_i;

    if (width <= 0 || height <= 0) {
        return 0u;
    }
    depth_i = sixel_helper_compute_depth(pixelformat);
    if (depth_i <= 0) {
        return 0u;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total / (size_t)width != (size_t)height) {
        return 0u;
    }

    depth = (size_t)depth_i;
    if (pixel_total > SIZE_MAX / depth) {
        return 0u;
    }
    return pixel_total * depth;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    int width;
    int height;
    int pixelformat;
    int ncolors;
    size_t pixel_bytes;
    size_t payload_size;
    size_t palette_bytes;
    size_t max_palette_bytes;
    uint8_t const *payload;
    unsigned char *pixels;
    unsigned char *palette;

    if (data == NULL || size < FUZZ_HEADER_BYTES) {
        return 0;
    }
    if (size > FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    width = 1 + (int)(data[1] & 0x3fu);
    height = 1 + (int)(data[2] & 0x3fu);
    pixelformat = fuzz_choose_pixelformat(data[0]);

    payload = data + FUZZ_HEADER_BYTES;
    payload_size = size - FUZZ_HEADER_BYTES;
    pixel_bytes = fuzz_compute_pixel_bytes(width, height, pixelformat);
    if (pixel_bytes == 0u || pixel_bytes > payload_size) {
        return 0;
    }

    /*
     * Forward libFuzzer-managed storage directly to encode_bytes().
     * Ownership bugs are easier to detect when the API receives memory that
     * was not allocated through libsixel's allocator.
     */
    pixels = (unsigned char *)(uintptr_t)payload;
    palette = NULL;
    ncolors = 0;

    if (pixelformat == SIXEL_PIXELFORMAT_PAL8) {
        max_palette_bytes = payload_size - pixel_bytes;
        if (max_palette_bytes < 3u) {
            return 0;
        }

        ncolors = 1 + (int)data[3];
        palette_bytes = (size_t)ncolors * 3u;
        if (palette_bytes / 3u != (size_t)ncolors
                || palette_bytes > max_palette_bytes) {
            ncolors = (int)(max_palette_bytes / 3u);
            if (ncolors <= 0) {
                return 0;
            }
            palette_bytes = (size_t)ncolors * 3u;
        }
        palette = (unsigned char *)(uintptr_t)(payload + pixel_bytes);
    }

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        return 0;
    }

    (void)sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE, "/dev/null");
    (void)sixel_encoder_encode_bytes(encoder,
                                     pixels,
                                     width,
                                     height,
                                     pixelformat,
                                     palette,
                                     ncolors);
    sixel_encoder_unref(encoder);
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
