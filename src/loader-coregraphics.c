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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_COREGRAPHICS

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "loader-coregraphics.h"
#include "loader.h"
#include "logger.h"
#include "compat_stub.h"

typedef struct sixel_loader_coregraphics_component {
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
} sixel_loader_coregraphics_component_t;


static SIXELSTATUS
coregraphics_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
coregraphics_resolve_animation_start_frame_no(int start_frame_no,
                                              int frame_count,
                                              int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}

static SIXELSTATUS
load_with_coregraphics(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
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
    int start_frame_no;
    int resolved_start_frame_no;
    int total_frames;
    int frame_index;
    int frames_in_loop;
    int loop_no;
    int stop_loop;
    int is_animation_container;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    total_frames = 0;
    frame_index = 0;
    frames_in_loop = 0;
    loop_no = 0;
    stop_loop = 0;
    is_animation_container = 0;
    frame_props = NULL;

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = coregraphics_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

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

    total_frames = (int)frame_count;
    if (start_frame_no != INT_MIN) {
        status = coregraphics_resolve_animation_start_frame_no(
            start_frame_no,
            total_frames,
            &resolved_start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /*
     * Keep total_frames as the actual image frame count even in static mode.
     * In static mode we still need to seek to resolved_start_frame_no first,
     * then emit exactly one frame and return from inside the decode loop.
     */

    props = CGImageSourceCopyProperties(source, NULL);
    if (props) {
        anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
            props, kCGImagePropertyGIFDictionary);
        if (anim_dict) {
            /*
             * Treat multi-frame decoding as animation only when the source
             * exposes GIF animation metadata. A multi-size ICO can contain
             * multiple static images and must not enter animation mode.
             */
            is_animation_container = 1;
            loop_num = (CFNumberRef)CFDictionaryGetValue(
                anim_dict, kCGImagePropertyGIFLoopCount);
            if (loop_num) {
                CFNumberGetValue(loop_num, kCFNumberIntType, &anim_loop_count);
            }
        }
    }

    frame->multiframe = (!fstatic && frame_count > 1
                        && is_animation_container);

    for (;;) {
        frame_index = 0;
        if (loop_no == 0 && resolved_start_frame_no != INT_MIN) {
            /*
             * Apply start-frame override only on the first loop. Later loops
             * always restart from frame 0 to preserve normal replay behavior.
             */
            frame_index = resolved_start_frame_no;
        }
        frames_in_loop = 0;

        while (frame_index < total_frames) {
            frame->frame_no = frames_in_loop;
            frame->loop_count = loop_no;

            image = CGImageSourceCreateImageAtIndex(
                source, (unsigned int)frame_index, NULL);
            if (! image) {
                status = SIXEL_FALSE;
                goto end;
            }

            frame_props = CGImageSourceCopyPropertiesAtIndex(
                source, (unsigned int)frame_index, NULL);
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
                        frame_anim_dict,
                        kCGImagePropertyGIFUnclampedDelayTime);
                    if (! delay_num) {
                        delay_num = (CFNumberRef)CFDictionaryGetValue(
                            frame_anim_dict, kCGImagePropertyGIFDelayTime);
                    }
                    if (delay_num) {
                        CFNumberGetValue(
                            delay_num,
                            kCFNumberDoubleType,
                            &delay_sec);
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
            /*
             * CoreGraphics renders into a premultiplied RGBA surface. Report
             * the four-component layout so downstream planners know an alpha
             * channel is available.
             */
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;

            if (frame->width > SIXEL_WIDTH_LIMIT) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: given width parameter is too"
                    " huge.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (frame->height > SIXEL_HEIGHT_LIMIT) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: given height parameter is too"
                    " huge.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (frame->width <= 0) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: an invalid width parameter"
                    " detected.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (frame->height <= 0) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: an invalid width parameter"
                    " detected.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (frame->height >= INT_MAX / 4 ||
                    frame->width >= INT_MAX / 4 ||
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
                    "load_with_coregraphics: sixel_allocator_malloc()"
                    " failed.");
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

            frame->multiframe = (!fstatic && frame_count > 1
                                && is_animation_container);
            status = fn_load(frame, context);
            CGImageRelease(image);
            image = NULL;
            if (status != SIXEL_OK) {
                goto end;
            }

            if (sixel_loader_callback_is_canceled(context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }

            ++frame_index;
            ++frames_in_loop;

            if (fstatic || !is_animation_container) {
                status = SIXEL_OK;
                goto end;
            }
        }

        ++loop_no;
        stop_loop = 0;

        if (total_frames <= 1 || loop_control == SIXEL_LOOP_DISABLE) {
            stop_loop = 1;
        } else if (loop_control == SIXEL_LOOP_AUTO) {
            if (anim_loop_count < 0) {
                stop_loop = 1;
            } else if (anim_loop_count > 0 && loop_no >= anim_loop_count) {
                stop_loop = 1;
            }
        }

        if (stop_loop) {
            break;
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
    if (props) {
        CFRelease(props);
    }
    if (data) {
        CFRelease(data);
    }
    if (frame) {
        sixel_frame_unref(frame);
    }
    return status;
}


static void
sixel_loader_coregraphics_ref(sixel_loader_component_t *component)
{
    sixel_loader_coregraphics_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_coregraphics_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_coregraphics_unref(sixel_loader_component_t *component)
{
    sixel_loader_coregraphics_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_coregraphics_component_t *)component;
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
sixel_loader_coregraphics_setopt(sixel_loader_component_t *component,
                                 int option,
                                 void const *value)
{
    sixel_loader_coregraphics_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_coregraphics_component_t *)component;
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
sixel_loader_coregraphics_load(sixel_loader_component_t *component,
                               sixel_chunk_t const *chunk,
                               sixel_load_image_function fn_load,
                               void *context)
{
    sixel_loader_coregraphics_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_coregraphics_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_coregraphics(chunk,
                                  self->fstatic,
                                  self->fuse_palette,
                                  self->reqcolors,
                                  bgcolor,
                                  self->loop_control,
                                  self->has_start_frame_no,
                                  self->start_frame_no,
                                  fn_load,
                                  context);
}

static char const *
sixel_loader_coregraphics_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "coregraphics";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_coregraphics_vtbl = {
    sixel_loader_coregraphics_ref,
    sixel_loader_coregraphics_unref,
    sixel_loader_coregraphics_setopt,
    sixel_loader_coregraphics_load,
    sixel_loader_coregraphics_name
};

SIXELSTATUS
sixel_loader_coregraphics_new(sixel_allocator_t *allocator,
                              sixel_loader_component_t **ppcomponent)
{
    sixel_loader_coregraphics_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_coregraphics_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_coregraphics_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

#endif  /* HAVE_COREGRAPHICS */

#if !HAVE_COREGRAPHICS
/*
 * Anchor a harmless symbol so the translation unit stays non-empty when
 * CoreGraphics is unavailable.
 */
typedef int loader_coregraphics_disabled;
#endif
