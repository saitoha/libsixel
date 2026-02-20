/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * libwebp-backed loader helpers extracted from loader.c to keep WebP decoding
 * isolated from unrelated translation units.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_WEBP

#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <webp/decode.h>
#include <webp/demux.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-libwebp.h"
#include "logger.h"

/*
 * Decode a WebP buffer into an RGB(A) pixel buffer managed by libsixel.
 *
 * The steps are:
 *   1) Probe the WebP bitstream for dimensions and alpha flags.
 *   2) Allocate the output buffer from the sixel allocator.
 *   3) Decode into RGB or RGBA depending on the alpha information.
 */
static SIXELSTATUS
load_webp(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    WebPBitstreamFeatures features;
    int bytes_per_pixel;
    size_t stride;
    size_t size;

    status = SIXEL_BAD_INPUT;

    if (WebPGetFeatures(data, datasize, &features) != VP8_STATUS_OK) {
        sixel_helper_set_additional_message(
            "load_webp: WebPGetFeatures failed.");
        return status;
    }

    if (features.width <= 0 || features.height <= 0) {
        sixel_helper_set_additional_message(
            "load_webp: invalid image dimensions.");
        return status;
    }

    if (features.width > INT_MAX || features.height > INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    *pwidth = features.width;
    *pheight = features.height;

    bytes_per_pixel = features.has_alpha ? 4 : 3;
    *ppixelformat = features.has_alpha ?
        SIXEL_PIXELFORMAT_RGBA8888 : SIXEL_PIXELFORMAT_RGB888;

    if ((size_t)*pwidth > SIZE_MAX / (size_t)bytes_per_pixel) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    stride = (size_t)*pwidth * (size_t)bytes_per_pixel;
    if ((size_t)*pheight > 0 && stride > SIZE_MAX / (size_t)*pheight) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    size = stride * (size_t)*pheight;
    *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (features.has_alpha) {
        if (WebPDecodeRGBAInto(data, datasize, *result, size,
                               (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBAInto failed.");
            return SIXEL_BAD_INPUT;
        }
    } else {
        if (WebPDecodeRGBInto(data, datasize, *result, size,
                              (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBInto failed.");
            return SIXEL_BAD_INPUT;
        }
    }

    status = SIXEL_OK;

    return status;
}


/*
 * Parse a VP8L payload header and detect whether the color indexing
 * transform is present.
 */
static int
vp8l_payload_uses_color_indexing(unsigned char const *data, size_t size)
{
    unsigned int bitbuf;
    int bits;
    size_t pos;
    unsigned int value;
    int transform_type;

    bitbuf = 0U;
    bits = 0;
    pos = 0U;
    value = 0U;
    transform_type = 0;

    if (data == NULL || size < 5U) {
        return 0;
    }

    if (size >= 13U &&
        data[0] == 'V' &&
        data[1] == 'P' &&
        data[2] == '8' &&
        data[3] == 'L') {
        data += 8;
        size -= 8U;
    }

    if (data[0] != 0x2fU) {
        return 0;
    }

    pos = 5U;

    for (;;) {
        while (bits < 1) {
            if (pos >= size) {
                return 0;
            }
            bitbuf |= (unsigned int)data[pos] << bits;
            bits += 8;
            pos++;
        }
        value = bitbuf & 0x1U;
        bitbuf >>= 1;
        bits -= 1;
        if (value == 0U) {
            break;
        }

        while (bits < 2) {
            if (pos >= size) {
                return 0;
            }
            bitbuf |= (unsigned int)data[pos] << bits;
            bits += 8;
            pos++;
        }
        transform_type = (int)(bitbuf & 0x3U);
        bitbuf >>= 2;
        bits -= 2;

        if (transform_type == 3) {
            return 1;
        }

        if (transform_type == 0 || transform_type == 1) {
            while (bits < 3) {
                if (pos >= size) {
                    return 0;
                }
                bitbuf |= (unsigned int)data[pos] << bits;
                bits += 8;
                pos++;
            }
            bitbuf >>= 3;
            bits -= 3;
        } else if (transform_type == 3) {
            while (bits < 8) {
                if (pos >= size) {
                    return 0;
                }
                bitbuf |= (unsigned int)data[pos] << bits;
                bits += 8;
                pos++;
            }
            bitbuf >>= 8;
            bits -= 8;
        }
    }

    return 0;
}


/*
 * Return 1 when every frame in the WebP stream uses VP8L color indexing.
 *
 * Palette promotion must only run for bitstreams that are explicitly indexed
 * in the source format. RGB/RGBA sources must keep their original semantics.
 */
static int
webp_stream_may_contain_vp8l(sixel_chunk_t const *pchunk)
{
    unsigned char const *bytes;
    size_t i;

    bytes = NULL;
    i = 0U;

    if (pchunk == NULL || pchunk->buffer == NULL || pchunk->size < 4U) {
        return 0;
    }

    bytes = pchunk->buffer;
    for (i = 0U; i + 3U < pchunk->size; ++i) {
        if (bytes[i + 0U] == 'V' &&
            bytes[i + 1U] == 'P' &&
            bytes[i + 2U] == '8' &&
            bytes[i + 3U] == 'L') {
            return 1;
        }
    }

    return 0;
}


static int
webp_input_is_indexed(sixel_chunk_t const *pchunk)
{
    WebPData data;
    WebPDemuxer *demux;
    WebPIterator iter;
    int frame_count;
    int frame_index;
    int indexed;

    data = (WebPData){ 0 };
    demux = NULL;
    iter = (WebPIterator){ 0 };
    frame_count = 0;
    frame_index = 0;
    indexed = 0;

    if (pchunk == NULL || pchunk->buffer == NULL || pchunk->size == 0U) {
        return 0;
    }
    if (!webp_stream_may_contain_vp8l(pchunk)) {
        return 0;
    }

    data.bytes = pchunk->buffer;
    data.size = pchunk->size;

    demux = WebPDemux(&data);
    if (demux == NULL) {
        return 0;
    }

    frame_count = (int)WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    if (frame_count <= 0) {
        WebPDemuxDelete(demux);
        return 0;
    }

    indexed = 1;
    for (frame_index = 1; frame_index <= frame_count; ++frame_index) {
        if (!WebPDemuxGetFrame(demux, frame_index, &iter)) {
            indexed = 0;
            break;
        }

        if (!vp8l_payload_uses_color_indexing(iter.fragment.bytes,
                                              iter.fragment.size)) {
            indexed = 0;
            WebPDemuxReleaseIterator(&iter);
            break;
        }

        WebPDemuxReleaseIterator(&iter);
    }

    WebPDemuxDelete(demux);

    return indexed;
}


/*
 * Try to convert an RGB frame into PAL8 when palette mode is requested.
 *
 * This helper performs an exact-color scan only. It does not approximate
 * colors. If the frame contains more unique colors than the requested budget,
 * the original RGB pixels are preserved.
 */
static SIXELSTATUS
loader_try_promote_pal8(
    sixel_frame_t  /* in/out */ *frame,
    int            /* in */     reqcolors,
    sixel_allocator_t /* in */  *allocator)
{
    SIXELSTATUS status;
    unsigned char *src;
    unsigned char *indices;
    unsigned char *palette;
    int pixel_total;
    int maxcolors;
    int i;
    int j;
    int index;
    int ncolors;
    int offset;
    int found;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned int key;
    unsigned int mixed;
    unsigned int step;
    unsigned int slot;
    unsigned int probe;
    unsigned int table_size;
    unsigned int table_mask;
    unsigned int *keys;
    unsigned char *values;
    int lookup_index;

#define PAL8_HASH_EMPTY_KEY 0xffffffffU

    status = SIXEL_OK;
    src = NULL;
    indices = NULL;
    palette = NULL;
    pixel_total = 0;
    maxcolors = 0;
    i = 0;
    j = 0;
    index = 0;
    ncolors = 0;
    offset = 0;
    found = 0;
    r = 0;
    g = 0;
    b = 0;
    key = 0U;
    mixed = 0U;
    step = 0U;
    slot = 0U;
    probe = 0U;
    table_size = 0U;
    table_mask = 0U;
    keys = NULL;
    values = NULL;
    lookup_index = 0;

    if (frame == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        return SIXEL_OK;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = frame->width * frame->height;
    if (pixel_total / frame->width != frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    maxcolors = reqcolors;
    if (maxcolors <= 0 || maxcolors > SIXEL_PALETTE_MAX) {
        maxcolors = SIXEL_PALETTE_MAX;
    }

    src = frame->pixels.u8ptr;
    if (src == NULL) {
        return SIXEL_BAD_INPUT;
    }

    indices = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      (size_t)pixel_total);
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      (size_t)maxcolors * 3U);
    if (palette == NULL) {
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }

    table_size = 2048U;
    while (table_size < (unsigned int)maxcolors * 4U) {
        table_size <<= 1U;
    }
    table_mask = table_size - 1U;

    keys = (unsigned int *)sixel_allocator_malloc(allocator,
                                                  sizeof(unsigned int) *
                                                  table_size);
    if (keys == NULL) {
        sixel_allocator_free(allocator, palette);
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }
    values = (unsigned char *)sixel_allocator_malloc(allocator,
                                                     table_size);
    if (values == NULL) {
        sixel_allocator_free(allocator, keys);
        sixel_allocator_free(allocator, palette);
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0; i < (int)table_size; ++i) {
        keys[i] = PAL8_HASH_EMPTY_KEY;
        values[i] = 0U;
    }

    for (i = 0; i < pixel_total; ++i) {
        offset = i * 3;
        r = src[offset + 0];
        g = src[offset + 1];
        b = src[offset + 2];
        key = ((unsigned int)r << 16)
            | ((unsigned int)g << 8)
            | (unsigned int)b;
        mixed = key;
        mixed ^= mixed >> 13;
        mixed *= 0x9e3779b1U;
        mixed ^= mixed >> 16;
        step = 0U;
        found = 0;
        index = 0;

        for (;;) {
            slot = (mixed + step) & table_mask;
            probe = keys[slot];
            if (probe == PAL8_HASH_EMPTY_KEY) {
                break;
            }
            if (probe == key) {
                lookup_index = (int)values[slot];
                if (lookup_index < ncolors &&
                    palette[lookup_index * 3 + 0] == r &&
                    palette[lookup_index * 3 + 1] == g &&
                    palette[lookup_index * 3 + 2] == b) {
                    found = 1;
                    index = lookup_index;
                    break;
                }
            }
            step++;
            if (step > table_mask) {
                break;
            }
        }

        if (!found && step > table_mask) {
            for (j = 0; j < ncolors; ++j) {
                if (palette[j * 3 + 0] == r &&
                    palette[j * 3 + 1] == g &&
                    palette[j * 3 + 2] == b) {
                    found = 1;
                    index = j;
                    break;
                }
            }
        }

        if (!found) {
            if (ncolors >= maxcolors) {
                sixel_allocator_free(allocator, values);
                sixel_allocator_free(allocator, keys);
                sixel_allocator_free(allocator, palette);
                sixel_allocator_free(allocator, indices);
                return SIXEL_OK;
            }
            index = ncolors;
            palette[index * 3 + 0] = r;
            palette[index * 3 + 1] = g;
            palette[index * 3 + 2] = b;
            ncolors++;

            if (step <= table_mask) {
                keys[slot] = key;
                values[slot] = (unsigned char)index;
            }
        }

        indices[i] = (unsigned char)index;
    }

    sixel_allocator_free(allocator, values);
    sixel_allocator_free(allocator, keys);

    sixel_allocator_free(allocator, frame->pixels.u8ptr);
    frame->pixels.u8ptr = NULL;

    sixel_frame_set_pixels(frame, indices);
    sixel_frame_set_palette(frame, palette);
    frame->ncolors = ncolors;
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

    return status;

#undef PAL8_HASH_EMPTY_KEY
}

/*
 * Dedicated libwebp loader wiring minimal pipeline.
 *
 *    +------------+     +-------------------+     +--------------------+
 *    | WebP chunk | --> | libwebp decode    | --> | sixel frame emit   |
 *    +------------+     +-------------------+     +--------------------+
 */
SIXELSTATUS
load_with_libwebp(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    WebPData webp_data;
    WebPAnimDecoderOptions decoder_options;
    WebPAnimDecoder *decoder;
    WebPAnimInfo anim_info;
    uint8_t *decoded_frame;
    int timestamp;
    int previous_timestamp;
    size_t frame_bytes;
    int next_delay;
    int frame_no;
    int loop_count;
    int allow_palette_promotion;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    webp_data = (WebPData){ 0 };
    decoder = NULL;
    anim_info = (WebPAnimInfo){ 0 };
    decoded_frame = NULL;
    timestamp = 0;
    previous_timestamp = 0;
    frame_bytes = 0;
    next_delay = 0;
    frame_no = 0;
    loop_count = 0;
    allow_palette_promotion = 0;

    if (fuse_palette) {
        allow_palette_promotion = webp_input_is_indexed(pchunk);
    }

    webp_data.bytes = pchunk->buffer;
    webp_data.size = pchunk->size;

    if (!WebPAnimDecoderOptionsInit(&decoder_options)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderOptionsInit failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }
    decoder_options.color_mode = MODE_RGBA;
    decoder = WebPAnimDecoderNew(&webp_data, &decoder_options);
    if (decoder == NULL) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderNew failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    if (!WebPAnimDecoderGetInfo(decoder, &anim_info)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderGetInfo failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    if (anim_info.frame_count <= 1) {
        WebPAnimDecoderDelete(decoder);
        decoder = NULL;

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = load_webp(&pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat,
                           pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        sixel_frame_set_pixels(frame, pixels);

        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (allow_palette_promotion) {
            status = loader_try_promote_pal8(frame,
                                             reqcolors,
                                             pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = SIXEL_OK;
        goto end;
    }

    if (fstatic) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (!WebPAnimDecoderHasMoreFrames(decoder)) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: no frames in animated WebP stream.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (!WebPAnimDecoderGetNext(decoder, &decoded_frame, &timestamp)) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: WebPAnimDecoderGetNext failed.");
            status = SIXEL_WEBP_ERROR;
            goto end;
        }

        frame->width = (int)anim_info.canvas_width;
        frame->height = (int)anim_info.canvas_height;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->multiframe = 0;
        frame->loop_count = 0;
        frame->frame_no = 0;
        frame->delay = timestamp / 10;

        if (frame->width <= 0 || frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: invalid canvas dimensions.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if ((size_t)frame->width > SIZE_MAX / 4 ||
            (size_t)frame->height > SIZE_MAX / ((size_t)frame->width * 4)) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        frame_bytes = (size_t)frame->width * (size_t)frame->height * 4;

        pixels = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                         frame_bytes);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(pixels, decoded_frame, frame_bytes);
        sixel_frame_set_pixels(frame, pixels);

        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (allow_palette_promotion) {
            status = loader_try_promote_pal8(frame,
                                             reqcolors,
                                             pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = SIXEL_OK;
        goto end;
    }

    /*
     * Decode WebP animation as fully composited RGBA canvases.
     *
     *   outer loop : logical animation loop
     *   inner loop : frame traversal inside a single loop
     *
     * Create a fresh sixel_frame_t for each callback invocation. This keeps
     * frame state isolated and avoids leaking in-place updates from one frame
     * into the next frame.
     */
    if ((int)anim_info.canvas_width <= 0 ||
        (int)anim_info.canvas_height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: invalid canvas dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    frame_bytes = (size_t)anim_info.canvas_width *
                  (size_t)anim_info.canvas_height;
    if ((size_t)anim_info.canvas_width != 0 &&
        frame_bytes / (size_t)anim_info.canvas_width !=
        (size_t)anim_info.canvas_height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (frame_bytes > SIZE_MAX / 4) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    frame_bytes *= 4;

    for (;;) {
        frame_no = 0;
        previous_timestamp = 0;

        while (WebPAnimDecoderHasMoreFrames(decoder)) {
            status = sixel_frame_new(&frame, pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            if (!WebPAnimDecoderGetNext(decoder,
                                        &decoded_frame,
                                        &timestamp)) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: WebPAnimDecoderGetNext failed.");
                status = SIXEL_WEBP_ERROR;
                goto end;
            }

            pixels = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                frame_bytes);
            if (pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            memcpy(pixels, decoded_frame, frame_bytes);
            sixel_frame_set_pixels(frame, pixels);
            pixels = NULL;

            frame->width = (int)anim_info.canvas_width;
            frame->height = (int)anim_info.canvas_height;
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            frame->multiframe = 1;
            frame->loop_count = loop_count;
            frame->frame_no = frame_no;

            next_delay = timestamp - previous_timestamp;
            if (next_delay < 0) {
                next_delay = 0;
            }
            frame->delay = next_delay / 10;

            status = sixel_frame_strip_alpha(frame, bgcolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            if (allow_palette_promotion) {
                status = loader_try_promote_pal8(frame,
                                                 reqcolors,
                                                 pchunk->allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }

            status = fn_load(frame, context);
            if (status == SIXEL_INTERRUPTED) {
                goto end;
            }
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            sixel_frame_unref(frame);
            frame = NULL;

            previous_timestamp = timestamp;
            frame_no++;
        }

        loop_count++;

        if (loop_control == SIXEL_LOOP_DISABLE || frame_no == 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO &&
            anim_info.loop_count > 0 &&
            (unsigned int)loop_count >= anim_info.loop_count) {
            break;
        }

        WebPAnimDecoderReset(decoder);
    }

    status = SIXEL_OK;

end:
    if (decoder != NULL) {
        WebPAnimDecoderDelete(decoder);
    }
    sixel_frame_unref(frame);

    return status;
}

int
loader_can_try_libwebp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_webp(chunk);
}

#else  /* !HAVE_WEBP */

/*
 * Keep a harmless placeholder around so pedantic builds skip the empty unit
 * warning when libwebp is not part of the build.
 */
enum { sixel_loader_libwebp_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libwebp_placeholder_function(void)
    SIXEL_LIBWEBP_PLACEHOLDER_UNUSED;

static void
sixel_loader_libwebp_placeholder_function(void)
{
    /*
     * The placeholder ties the enum to a symbol so MSVC does not warn about
     * an empty translation unit when libwebp support is disabled.
     */
    (void)sixel_loader_libwebp_placeholder;
}

#undef SIXEL_LIBWEBP_PLACEHOLDER_UNUSED

#endif  /* HAVE_WEBP */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
