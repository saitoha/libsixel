/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * CoreGraphics-backed loader extracted from loader.c. Isolating macOS headers
 * keeps other backends lightweight while preserving the existing decoding
 * sequence and diagnostics.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_COREGRAPHICS

#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "loader-coregraphics.h"
#include "logger.h"

SIXELSTATUS
load_with_coregraphics(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    CFDataRef data = NULL;
    CGImageSourceRef source = NULL;
    CGImageRef image = NULL;
    CGColorSpaceRef color_space = NULL;
    CGContextRef ctx = NULL;
    size_t stride;
    size_t frame_count;
    int anim_loop_count = (-1);
    CFDictionaryRef props = NULL;
    CFDictionaryRef anim_dict;
    CFNumberRef loop_num;
    CFDictionaryRef frame_props;
    CFDictionaryRef frame_anim_dict;
    CFNumberRef delay_num;
    double delay_sec;
    unsigned char *pixels;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    data = CFDataCreate(kCFAllocatorDefault,
                        pchunk->buffer,
                        (CFIndex)pchunk->size);
    if (! data) {
        status = SIXEL_FALSE;
        goto end;
    }

    source = CGImageSourceCreateWithData(data, NULL);
    if (! source) {
        status = SIXEL_FALSE;
        goto end;
    }

    frame_count = CGImageSourceGetCount(source);
    if (! frame_count) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (fstatic) {
        frame_count = 1;
    }

    props = CGImageSourceCopyProperties(source, NULL);
    if (props) {
        anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
            props, kCGImagePropertyGIFDictionary);
        if (anim_dict) {
            loop_num = (CFNumberRef)CFDictionaryGetValue(
                anim_dict, kCGImagePropertyGIFLoopCount);
            if (loop_num) {
                CFNumberGetValue(loop_num, kCFNumberIntType, &anim_loop_count);
            }
        }
    }

    frame->frame_no = 0;
    frame->loop_count = 0;
    frame->multiframe = (frame_count > 1);
    for (;;) {
        image = CGImageSourceCreateImageAtIndex(
            source, (unsigned int)frame->frame_no, NULL);
        if (! image) {
            status = SIXEL_FALSE;
            goto end;
        }

        frame_props = CGImageSourceCopyPropertiesAtIndex(
            source, (unsigned int)frame->frame_no, NULL);
        if (frame_props) {
            frame_anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
                frame_props, kCGImagePropertyGIFDictionary);
            if (frame_anim_dict) {
                loop_num = (CFNumberRef)CFDictionaryGetValue(
                    frame_anim_dict, kCGImagePropertyGIFLoopCount);
                if (loop_num) {
                    CFNumberGetValue(loop_num,
                                     kCFNumberIntType,
                                     &anim_loop_count);
                }
                delay_num = (CFNumberRef)CFDictionaryGetValue(
                    frame_anim_dict, kCGImagePropertyGIFUnclampedDelayTime);
                if (! delay_num) {
                    delay_num = (CFNumberRef)CFDictionaryGetValue(
                        frame_anim_dict, kCGImagePropertyGIFDelayTime);
                }
                if (delay_num) {
                    CFNumberGetValue(delay_num, kCFNumberDoubleType, &delay_sec);
                    if (delay_sec < 0) {
                        delay_sec = 0.0;
                    }
                    frame->delay = (int)(delay_sec * 100);
                }
            }
            CFRelease(frame_props);
            frame_props = NULL;
        }

        frame->width = (int)CGImageGetWidth(image);
        frame->height = (int)CGImageGetHeight(image);
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;

        if (frame->width > SIXEL_WIDTH_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given width parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->height > SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given height parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->width <= 0) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: an invalid width parameter detected.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: an invalid width parameter detected.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->height >= INT_MAX / 4 || frame->width >= INT_MAX / 4 ||
                frame->height * frame->width * 4 >= INT_MAX) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: too large image.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        stride = (size_t)frame->width * 4;
        sixel_frame_set_pixels(frame,
                               sixel_allocator_malloc(
                                   pchunk->allocator,
                                   (size_t)(frame->height * stride)));
        pixels = sixel_frame_get_pixels(frame);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        if (! color_space) {
            CGImageRelease(image);
            goto end;
        }

        ctx = CGBitmapContextCreate(pixels,
                                    (size_t)frame->width,
                                    (size_t)frame->height,
                                    8,
                                    stride,
                                    color_space,
                                    kCGImageAlphaPremultipliedLast |
                                            kCGBitmapByteOrder32Big);
        if (!ctx) {
            CGImageRelease(image);
            goto end;
        }

        CGContextDrawImage(ctx,
                           CGRectMake(0, 0, frame->width, frame->height),
                           image);
        CGContextRelease(ctx);
        ctx = NULL;

        frame->multiframe = (frame_count > 1);
        status = fn_load(frame, context);
        CGImageRelease(image);
        image = NULL;
        if (status != SIXEL_OK) {
            goto end;
        }
        ++frame->frame_no;

        ++frame->loop_count;

        if (frame_count <= 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_DISABLE) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO) {
            if (anim_loop_count < 0) {
                break;
            }
            if (anim_loop_count > 0 && frame->loop_count >= anim_loop_count) {
                break;
            }
            continue;
        }
    }

    status = SIXEL_OK;

end:
    if (ctx) {
        CGContextRelease(ctx);
    }
    if (color_space) {
        CGColorSpaceRelease(color_space);
    }
    if (image) {
        CGImageRelease(image);
    }
    if (source) {
        CFRelease(source);
    }
    if (data) {
        CFRelease(data);
    }
    if (frame) {
        sixel_frame_unref(frame);
    }
    return status;
}

#endif  /* HAVE_COREGRAPHICS */

#if !HAVE_COREGRAPHICS
/*
 * Anchor a harmless symbol so the translation unit stays non-empty when
 * CoreGraphics is unavailable.
 */
typedef int loader_coregraphics_disabled;
#endif

