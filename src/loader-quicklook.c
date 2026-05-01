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

#if defined(HAVE_CONFIG_H)
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

#include "chunk-view.h"
#include "frame-private.h"
#include "frame-factory.h"
#include "loader-common.h"
#include "loader-quicklook.h"

typedef struct sixel_loader_quicklook_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_quicklook_component_t;

static unsigned char
quicklook_unpremultiply_channel(unsigned int value, unsigned int alpha)
{
    unsigned int unpremultiplied;

    if (alpha == 0u) {
        return 0u;
    }
    if (alpha >= 255u) {
        return (unsigned char)value;
    }

    unpremultiplied = (value * 255u + alpha / 2u) / alpha;
    if (unpremultiplied > 255u) {
        unpremultiplied = 255u;
    }
    return (unsigned char)unpremultiplied;
}

static void
quicklook_finalize_frame_pixels(sixel_frame_t *frame,
                                unsigned char const *bgcolor)
{
    unsigned char *pixels;
    size_t pixel_total;
    size_t pixel_index;
    unsigned int alpha;
    int inspect_alpha;
    int preserve_alpha;

    pixels = NULL;
    pixel_total = 0u;
    pixel_index = 0u;
    alpha = 0u;
    inspect_alpha = 0;
    preserve_alpha = 0;
    if (frame == NULL) {
        return;
    }

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL || frame->width <= 0 || frame->height <= 0) {
        return;
    }

    pixel_total = (size_t)frame->width * (size_t)frame->height;
    inspect_alpha = bgcolor == NULL ? 1 : 0;
    if (inspect_alpha) {
        for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
            alpha = pixels[pixel_index * 4u + 3u];
            if (alpha != 255u) {
                preserve_alpha = 1;
            }
            pixels[pixel_index * 4u + 0u] =
                quicklook_unpremultiply_channel(
                    pixels[pixel_index * 4u + 0u], alpha);
            pixels[pixel_index * 4u + 1u] =
                quicklook_unpremultiply_channel(
                    pixels[pixel_index * 4u + 1u], alpha);
            pixels[pixel_index * 4u + 2u] =
                quicklook_unpremultiply_channel(
                    pixels[pixel_index * 4u + 2u], alpha);
        }
    }

    if (!preserve_alpha) {
        for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
            pixels[pixel_index * 3u + 0u] = pixels[pixel_index * 4u + 0u];
            pixels[pixel_index * 3u + 1u] = pixels[pixel_index * 4u + 1u];
            pixels[pixel_index * 3u + 2u] = pixels[pixel_index * 4u + 2u];
        }
    }

    frame->pixelformat = preserve_alpha
        ? SIXEL_PIXELFORMAT_RGBA8888
        : SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = preserve_alpha ? 1 : 0;
}

static void
quicklook_set_error_message(char const *context)
{
    if (context == NULL) {
        return;
    }
    sixel_helper_set_additional_message(context);
}

#if HAVE_QUICKLOOK_THUMBNAILING
CGImageRef
sixel_quicklook_thumbnail_create(CFURLRef url, CGSize max_size);
#endif

static int
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

    if (pchunk != NULL && sixel_chunk_get_source_path(pchunk) != NULL) {
        path = sixel_chunk_get_source_path(pchunk);
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

static int
loader_quicklook_can_decode_chunk(sixel_chunk_t const *pchunk)
{
    /*
     * Registry predicates receive only the chunk. This wrapper forwards to the
     * full probe while omitting any filename hint so the registry table keeps
     * type-safe pointers.
     */
    return loader_quicklook_can_decode(pchunk, NULL);
}

static SIXELSTATUS
load_with_quicklook(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
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
    (void)start_frame_no_set;
    (void)start_frame_no;

    if (pchunk == NULL || sixel_chunk_get_source_path(pchunk) == NULL) {
        quicklook_set_error_message(
            "load_with_quicklook: source path is unavailable.");
        goto end;
    }

    loader_thumbnailer_initialize_size_hint();

    status = sixel_frame_create_from_factory(&frame, sixel_chunk_get_allocator(pchunk));
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    path = CFStringCreateWithCString(kCFAllocatorDefault,
                                     sixel_chunk_get_source_path(pchunk),
                                     kCFStringEncodingUTF8);
    if (path == NULL) {
        quicklook_set_error_message(
            "load_with_quicklook: CFStringCreateWithCString failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                        path,
                                        kCFURLPOSIXPathStyle,
                                        false);
    if (url == NULL) {
        quicklook_set_error_message(
            "load_with_quicklook: CFURLCreateWithFileSystemPath failed.");
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
        quicklook_set_error_message(
            "load_with_quicklook: QuickLook thumbnail creation failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    bounds = CGRectMake(0.0, 0.0,
                        (CGFloat)CGImageGetWidth(image),
                        (CGFloat)CGImageGetHeight(image));
    frame->width = (int)bounds.size.width;
    frame->height = (int)bounds.size.height;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    /*
     * QuickLook renders into a premultiplied RGBA buffer. Keep a four-byte
     * stride so the pixel format matches the bitmap context layout.
     */
    stride = (size_t)frame->width * 4;

    pixels = (unsigned char *)sixel_allocator_malloc(
        sixel_chunk_get_allocator(pchunk),
        (size_t)(frame->height * stride));
    if (pixels == NULL) {
        quicklook_set_error_message(
            "load_with_quicklook: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            NULL,
            frame->width,
            frame->height,
            frame->pixelformat,
            frame->colorspace,
            -1,
            SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(sixel_chunk_get_allocator(pchunk), pixels);
        goto end;
    }

    color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (color_space == NULL) {
        quicklook_set_error_message(
            "load_with_quicklook: CGColorSpaceCreateWithName failed.");
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
        quicklook_set_error_message(
            "load_with_quicklook: CGBitmapContextCreate failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    if (bgcolor != NULL) {
        fill_r = (CGFloat)bgcolor[0] / 255.0;
        fill_g = (CGFloat)bgcolor[1] / 255.0;
        fill_b = (CGFloat)bgcolor[2] / 255.0;
        CGContextSetRGBFillColor(ctx, fill_r, fill_g, fill_b, 1.0);
        CGContextFillRect(ctx, bounds);
    } else {
        CGContextSetBlendMode(ctx, kCGBlendModeCopy);
        CGContextSetRGBFillColor(ctx, 0.0, 0.0, 0.0, 0.0);
        CGContextFillRect(ctx, bounds);
        CGContextSetBlendMode(ctx, kCGBlendModeNormal);
    }
    CGContextDrawImage(ctx, bounds, image);
    quicklook_finalize_frame_pixels(frame, bgcolor);
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


static void
sixel_loader_quicklook_ref(sixel_loader_component_t *component)
{
    sixel_loader_quicklook_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_quicklook_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_quicklook_unref(sixel_loader_component_t *component)
{
    sixel_loader_quicklook_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_quicklook_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_quicklook_setopt(sixel_loader_component_t *component,
                              int option,
                              void const *value)
{
    sixel_loader_quicklook_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_quicklook_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_quicklook_load(sixel_loader_component_t *component,
                            sixel_chunk_t const *chunk,
                            sixel_load_image_function fn_load,
                            void *context)
{
    sixel_loader_quicklook_component_t *self;
    unsigned char *bgcolor;
    SIXELSTATUS status;
    int header_job_id;
    int decode_job_id;
    sixel_loader_timeline_callback_state_t timeline_state;

    self = NULL;
    bgcolor = NULL;
    status = SIXEL_FALSE;
    header_job_id = -1;
    decode_job_id = -1;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_quicklook_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    header_job_id = loader_timeline_phase_start("header/read");
    decode_job_id = loader_timeline_phase_start("decode/pixels");
    loader_timeline_callback_state_init(&timeline_state,
                                        fn_load,
                                        context,
                                        header_job_id,
                                        decode_job_id);

    status = load_with_quicklook(chunk,
                                 self->fstatic,
                                 self->fuse_palette,
                                 self->reqcolors,
                                 bgcolor,
                                 self->loop_control,
                                 self->has_start_frame_no,
                                 self->start_frame_no,
                                 loader_timeline_emit_frame_callback,
                                 &timeline_state);

    loader_timeline_callback_close_header(&timeline_state, status);
    loader_timeline_callback_close_decode(&timeline_state, status);
    loader_timeline_optional_skip_if_unmarked("post/colorspace");
    loader_timeline_optional_skip_if_unmarked("post/background");
    loader_timeline_optional_skip_if_unmarked("post/icc");

    return status;
}

static char const *
sixel_loader_quicklook_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "quicklook";
}

static int
sixel_loader_quicklook_predicate(sixel_loader_component_t *component,
                                 sixel_chunk_t const *chunk)
{
    (void)component;
    return loader_quicklook_can_decode_chunk(chunk);
}

static sixel_loader_component_vtbl_t const g_sixel_loader_quicklook_vtbl = {
    sixel_loader_quicklook_ref,
    sixel_loader_quicklook_unref,
    sixel_loader_quicklook_setopt,
    sixel_loader_quicklook_load,
    sixel_loader_quicklook_name,
    sixel_loader_quicklook_predicate
};

SIXELSTATUS
sixel_loader_quicklook_new(sixel_allocator_t *allocator,
                           void **ppcomponent)
{
    sixel_loader_quicklook_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_quicklook_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_quicklook_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
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
