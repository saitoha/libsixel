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
 * QuickLook thumbnail loader carved out of loader.c to keep macOS-specific
 * headers contained. The helper mirrors the previous flow: probe QuickLook,
 * request a thumbnail, and blit it into a sixel frame.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK

#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-quicklook.h"

#if HAVE_QUICKLOOK_THUMBNAILING
CGImageRef
sixel_quicklook_thumbnail_create(CFURLRef url, CGSize max_size);
#endif

int
loader_quicklook_can_decode(sixel_chunk_t const *pchunk,
                            char const *filename)
{
    char const *path;
    CFStringRef path_ref;
    CFURLRef url;
    CGFloat max_dimension;
    CGSize max_size;
    CGImageRef image;
    int result;
    int hint;

    path = NULL;
    path_ref = NULL;
    url = NULL;
    image = NULL;
    result = 0;

    loader_thumbnailer_initialize_size_hint();

    if (pchunk != NULL && pchunk->source_path != NULL) {
        path = pchunk->source_path;
    } else if (filename != NULL) {
        path = filename;
    }

    if (path == NULL || strcmp(path, "-") == 0 ||
            strstr(path, "://") != NULL) {
        return 0;
    }

    path_ref = CFStringCreateWithCString(kCFAllocatorDefault,
                                         path,
                                         kCFStringEncodingUTF8);
    if (path_ref == NULL) {
        return 0;
    }

    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                        path_ref,
                                        kCFURLPOSIXPathStyle,
                                        false);
    CFRelease(path_ref);
    path_ref = NULL;
    if (url == NULL) {
        return 0;
    }

    hint = loader_thumbnailer_get_size_hint();
    if (hint > 0) {
        max_dimension = (CGFloat)hint;
    } else {
        max_dimension = (CGFloat)loader_thumbnailer_get_default_size_hint();
    }
    max_size.width = max_dimension;
    max_size.height = max_dimension;

#if HAVE_QUICKLOOK_THUMBNAILING
    image = sixel_quicklook_thumbnail_create(url, max_size);
    if (image == NULL) {
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
        image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                       url,
                                       max_size,
                                       NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
    }
#else
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
    image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                   url,
                                   max_size,
                                   NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
#endif

    if (image != NULL) {
        result = 1;
        CGImageRelease(image);
        image = NULL;
    }

    CFRelease(url);
    url = NULL;

    return result;
}

int
loader_quicklook_can_decode_chunk(sixel_chunk_t const *pchunk)
{
    /*
     * Registry predicates receive only the chunk. This wrapper forwards to the
     * full probe while omitting any filename hint so the registry table keeps
     * type-safe pointers.
     */
    return loader_quicklook_can_decode(pchunk, NULL);
}

SIXELSTATUS
load_with_quicklook(
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
    CFStringRef path = NULL;
    CFURLRef url = NULL;
    CGImageRef image = NULL;
    CGColorSpaceRef color_space = NULL;
    CGContextRef ctx = NULL;
    CGRect bounds;
    size_t stride;
    unsigned char fill_color[3];
    unsigned char default_bgcolor[3];
    unsigned char const *fill_source;
    CGFloat fill_r;
    CGFloat fill_g;
    CGFloat fill_b;
    CGFloat max_dimension;
    CGSize max_size;
    unsigned char *pixels;
    int hint;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;

    if (pchunk == NULL || pchunk->source_path == NULL) {
        goto end;
    }

    loader_thumbnailer_initialize_size_hint();

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    path = CFStringCreateWithCString(kCFAllocatorDefault,
                                     pchunk->source_path,
                                     kCFStringEncodingUTF8);
    if (path == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                        path,
                                        kCFURLPOSIXPathStyle,
                                        false);
    if (url == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    hint = loader_thumbnailer_get_size_hint();
    if (hint > 0) {
        max_dimension = (CGFloat)hint;
    } else {
        max_dimension = (CGFloat)loader_thumbnailer_get_default_size_hint();
    }
    max_size.width = max_dimension;
    max_size.height = max_dimension;

#if HAVE_QUICKLOOK_THUMBNAILING
    image = sixel_quicklook_thumbnail_create(url, max_size);
    if (image == NULL) {
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
        image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                       url,
                                       max_size,
                                       NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
    }
#else
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
    image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                   url,
                                   max_size,
                                   NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
#endif

    if (image == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    bounds = CGRectMake(0.0, 0.0,
                        (CGFloat)CGImageGetWidth(image),
                        (CGFloat)CGImageGetHeight(image));
    frame->width = (int)bounds.size.width;
    frame->height = (int)bounds.size.height;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    /*
     * QuickLook renders into a premultiplied RGBA buffer. Keep a four-byte
     * stride so the pixel format matches the bitmap context layout.
     */
    stride = (size_t)frame->width * 4;

    /*
     * Background colors are optional for most loaders. QuickLook renders
     * into a bitmap that needs an explicit clear color, so choose a safe
     * default when callers did not supply one.
     */
    fill_source = bgcolor;
    if (fill_source == NULL) {
        default_bgcolor[0] = 0;
        default_bgcolor[1] = 0;
        default_bgcolor[2] = 0;
        fill_source = default_bgcolor;
    }
    fill_color[0] = fill_source[0];
    fill_color[1] = fill_source[1];
    fill_color[2] = fill_source[2];
    fill_r = (CGFloat)fill_color[0] / 255.0;
    fill_g = (CGFloat)fill_color[1] / 255.0;
    fill_b = (CGFloat)fill_color[2] / 255.0;
    /* QuickLook renders into RGBA so no palette mapping is required. */
    sixel_frame_set_pixels(frame,
                           sixel_allocator_malloc(
                               pchunk->allocator,
                               (size_t)(frame->height * stride)));
    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (color_space == NULL) {
        status = SIXEL_RUNTIME_ERROR;
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
    if (ctx == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    CGContextSetRGBFillColor(ctx, fill_r, fill_g, fill_b, 1.0);
    CGContextFillRect(ctx, bounds);
    CGContextDrawImage(ctx, bounds, image);
    frame->multiframe = 0;
    frame->frame_no = 0;
    frame->delay = 0;
    status = fn_load(frame, context);
    if (status != SIXEL_OK) {
        goto end;
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
    if (url) {
        CFRelease(url);
    }
    if (path) {
        CFRelease(path);
    }
    if (frame) {
        sixel_frame_unref(frame);
    }
    return status;
}

#endif  /* HAVE_COREGRAPHICS && HAVE_QUICKLOOK */

#if !(HAVE_COREGRAPHICS && HAVE_QUICKLOOK)
/*
 * Preserve a non-empty unit when QuickLook support is disabled to avoid
 * pedantic compiler warnings.
 */
typedef int loader_quicklook_disabled;
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
