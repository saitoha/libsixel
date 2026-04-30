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
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>

#include <sixel.h>

#include "chunk.h"
#include "frame-private.h"
#include "loader-common.h"
#include "loader-coregraphics.h"
#include "loader.h"
#include "logger.h"
#include "compat_stub.h"

/*
 * Keep animation metadata keys available even on SDKs that do not expose the
 * newer typed constants yet. ImageIO dictionaries use these stable key names.
 */
#ifndef kCGImagePropertyAPNGLoopCount
#define kCGImagePropertyAPNGLoopCount CFSTR("LoopCount")
#endif
#ifndef kCGImagePropertyAPNGDelayTime
#define kCGImagePropertyAPNGDelayTime CFSTR("DelayTime")
#endif
#ifndef kCGImagePropertyAPNGUnclampedDelayTime
#define kCGImagePropertyAPNGUnclampedDelayTime CFSTR("UnclampedDelayTime")
#endif
#ifndef kCGImagePropertyWebPDictionary
#define kCGImagePropertyWebPDictionary CFSTR("{WebP}")
#endif
#ifndef kCGImagePropertyWebPLoopCount
#define kCGImagePropertyWebPLoopCount CFSTR("LoopCount")
#endif
#ifndef kCGImagePropertyWebPDelayTime
#define kCGImagePropertyWebPDelayTime CFSTR("DelayTime")
#endif
#ifndef kCGImagePropertyWebPUnclampedDelayTime
#define kCGImagePropertyWebPUnclampedDelayTime CFSTR("UnclampedDelayTime")
#endif
#ifndef kCGImagePropertyHEICSDictionary
#define kCGImagePropertyHEICSDictionary CFSTR("{HEICS}")
#endif
#ifndef kCGImagePropertyHEICSLoopCount
#define kCGImagePropertyHEICSLoopCount CFSTR("LoopCount")
#endif
#ifndef kCGImagePropertyHEICSDelayTime
#define kCGImagePropertyHEICSDelayTime CFSTR("DelayTime")
#endif
#ifndef kCGImagePropertyHEICSUnclampedDelayTime
#define kCGImagePropertyHEICSUnclampedDelayTime CFSTR("UnclampedDelayTime")
#endif
#ifndef kCGImagePropertyOrientation
#define kCGImagePropertyOrientation CFSTR("Orientation")
#endif

#define COREGRAPHICS_SRGB_ENCODE_LUT_SIZE 4096u
#define COREGRAPHICS_FRAME_CACHE_MAX_BYTES_DEFAULT \
    (64u * 1024u * 1024u)

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
    int enable_orientation;
} sixel_loader_coregraphics_component_t;

typedef struct coregraphics_srgb_lut_cache {
    int prepared;
    double decode_lut[256];
    unsigned char encode_lut[COREGRAPHICS_SRGB_ENCODE_LUT_SIZE + 1u];
} coregraphics_srgb_lut_cache_t;

typedef struct coregraphics_png_trns_chunk_cache {
    int initialized;
    int available;
    int alpha_count;
    unsigned char alpha_entries[SIXEL_PALETTE_MAX];
} coregraphics_png_trns_chunk_cache_t;

typedef struct coregraphics_frame_meta_slot {
    int delay;
    int orientation;
    int has_alpha;
    int promote_float32;
    int is_indexed;
    unsigned char props_ready;
    unsigned char decode_hint_ready;
} coregraphics_frame_meta_slot_t;

typedef struct coregraphics_cache_slot {
    unsigned char decided;
    sixel_frame_t *frame;
} coregraphics_cache_slot_t;

typedef struct coregraphics_loader_state {
    SIXELSTATUS status;
    sixel_chunk_t const *chunk;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int enable_orientation;
    unsigned char *bgcolor;
    int loop_control;
    int start_frame_no_set;
    int start_frame_no_override;
    sixel_load_image_function fn_load;
    void *context;
    sixel_frame_t *frame;
    sixel_frame_t *emit_frame;
    sixel_frame_t *decode_frame;
    sixel_frame_t *cached_frame_tmp;
    CFDataRef data;
    CGImageSourceRef source;
    CGImageRef image;
    CFDictionaryRef props;
    CFDictionaryRef frame_props;
    size_t frame_count;
    int total_frames;
    int anim_loop_count;
    int is_animation_container;
    int source_orientation;
    int start_frame_no;
    int resolved_start_frame_no;
    int frame_index;
    int loop_no;
    int frames_in_loop;
    int stop_loop;
    size_t metadata_slots;
    coregraphics_frame_meta_slot_t single_meta_slot;
    coregraphics_frame_meta_slot_t *frame_meta_slots;
    coregraphics_frame_meta_slot_t *active_meta_slots;
    coregraphics_cache_slot_t *frame_cache_slots;
    int frame_cache_enabled;
    size_t frame_cache_max_bytes;
    size_t frame_cache_used_bytes;
    size_t frame_cache_frame_bytes;
    size_t image_width;
    size_t image_height;
    int frame_cache_keep;
    int frame_cache_decision_pending;
    int release_emit_frame;
    int cache_hit;
    int indexed_handled;
    int force_alpha_from_indexed;
    int has_alpha_like;
    int promote_float32;
    int frame_orientation;
    size_t frame_meta_slot;
    coregraphics_png_trns_chunk_cache_t png_trns_chunk_cache;
    coregraphics_srgb_lut_cache_t rgba8_lut_cache;
    CFIndex cf_data_length;
    size_t prefetch_index;
} coregraphics_loader_state_t;

static unsigned char
coregraphics_unpremultiply_channel(unsigned int value, unsigned int alpha)
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

static double
coregraphics_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
coregraphics_decode_srgb_unit(double value)
{
    value = coregraphics_clamp_unit(value);
    if (value <= 0.04045) {
        return value / 12.92;
    }
#if HAVE_MATH_H
    return pow((value + 0.055) / 1.055, 2.4);
#else
    return value;
#endif
}

static double
coregraphics_encode_srgb_unit(double value)
{
    value = coregraphics_clamp_unit(value);
    if (value <= 0.0031308) {
        return value * 12.92;
    }
#if HAVE_MATH_H
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
#else
    return value;
#endif
}

static void
coregraphics_build_srgb_decode_u8_lut(double lut[256])
{
    int index;
    double unit;

    index = 0;
    unit = 0.0;
    if (lut == NULL) {
        return;
    }

    for (index = 0; index < 256; ++index) {
        unit = (double)index / 255.0;
        lut[index] = coregraphics_decode_srgb_unit(unit);
    }
}

static void
coregraphics_build_srgb_encode_u8_lut(unsigned char *lut, size_t lut_size)
{
    size_t index;
    double unit;

    index = 0u;
    unit = 0.0;
    if (lut == NULL || lut_size == 0u) {
        return;
    }

    for (index = 0u; index < lut_size; ++index) {
        if (lut_size > 1u) {
            unit = (double)index / (double)(lut_size - 1u);
        } else {
            unit = 0.0;
        }
        lut[index] = (unsigned char)(coregraphics_encode_srgb_unit(unit) *
                                     255.0 + 0.5);
    }
}

static unsigned char
coregraphics_encode_linear_to_srgb_u8(double value,
                                      unsigned char const *lut,
                                      size_t lut_size)
{
    size_t index;

    index = 0u;
    if (lut == NULL || lut_size == 0u) {
        return 0u;
    }

    value = coregraphics_clamp_unit(value);
    if (lut_size <= 1u) {
        return lut[0];
    }

    index = (size_t)(value * (double)(lut_size - 1u) + 0.5);
    if (index >= lut_size) {
        index = lut_size - 1u;
    }
    return lut[index];
}

static int
coregraphics_property_to_bool(CFTypeRef value, int *out_value)
{
    CFTypeID type_id;
    int numeric_value;
    Boolean ok;

    type_id = 0;
    numeric_value = 0;
    ok = false;
    if (value == NULL || out_value == NULL) {
        return 0;
    }

    type_id = CFGetTypeID(value);
    if (type_id == CFBooleanGetTypeID()) {
        *out_value = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
        return 1;
    }
    if (type_id == CFNumberGetTypeID()) {
        ok = CFNumberGetValue((CFNumberRef)value,
                              kCFNumberIntType,
                              &numeric_value);
        if (ok) {
            *out_value = numeric_value != 0 ? 1 : 0;
            return 1;
        }
    }

    return 0;
}

static int
coregraphics_dictionary_get_bool(CFDictionaryRef dict,
                                 CFStringRef key,
                                 int default_value)
{
    CFTypeRef value;
    int parsed;
    int result;

    value = NULL;
    parsed = 0;
    result = default_value;
    if (dict == NULL || key == NULL) {
        return default_value;
    }

    value = CFDictionaryGetValue(dict, key);
    parsed = coregraphics_property_to_bool(value, &result);
    if (!parsed) {
        result = default_value;
    }
    return result;
}

static int
coregraphics_dictionary_get_int(CFDictionaryRef dict,
                                CFStringRef key,
                                int default_value)
{
    CFTypeRef value;
    int result;
    Boolean ok;

    value = NULL;
    result = default_value;
    ok = false;
    if (dict == NULL || key == NULL) {
        return default_value;
    }

    value = CFDictionaryGetValue(dict, key);
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return default_value;
    }

    ok = CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &result);
    if (!ok) {
        result = default_value;
    }
    return result;
}

static int
coregraphics_dictionary_get_double(CFDictionaryRef dict,
                                   CFStringRef key,
                                   double *out_value)
{
    CFTypeRef value;
    Boolean ok;
    double parsed;

    value = NULL;
    ok = false;
    parsed = 0.0;
    if (dict == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    value = CFDictionaryGetValue(dict, key);
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }

    ok = CFNumberGetValue((CFNumberRef)value, kCFNumberDoubleType, &parsed);
    if (!ok) {
        return 0;
    }

    *out_value = parsed;
    return 1;
}

static int
coregraphics_sanitize_exif_orientation(int orientation)
{
    if (orientation < 1 || orientation > 8) {
        return 1;
    }
    return orientation;
}

static int
coregraphics_resolve_exif_orientation(CFDictionaryRef props,
                                      int fallback_orientation)
{
    int resolved_orientation;

    resolved_orientation = coregraphics_sanitize_exif_orientation(
        fallback_orientation);
    if (props == NULL) {
        return resolved_orientation;
    }

    resolved_orientation = coregraphics_dictionary_get_int(
        props,
        kCGImagePropertyOrientation,
        resolved_orientation);
    return coregraphics_sanitize_exif_orientation(resolved_orientation);
}

static int
coregraphics_get_animation_keys(CFDictionaryRef props,
                                size_t frame_count,
                                CFDictionaryRef *out_dict,
                                CFStringRef *out_loop_key,
                                CFStringRef *out_delay_key,
                                CFStringRef *out_unclamped_delay_key)
{
    CFDictionaryRef dict;

    dict = NULL;
    if (out_dict == NULL) {
        return 0;
    }

    *out_dict = NULL;
    if (out_loop_key != NULL) {
        *out_loop_key = NULL;
    }
    if (out_delay_key != NULL) {
        *out_delay_key = NULL;
    }
    if (out_unclamped_delay_key != NULL) {
        *out_unclamped_delay_key = NULL;
    }
    if (props == NULL) {
        return 0;
    }

    dict = (CFDictionaryRef)CFDictionaryGetValue(props,
                                                 kCGImagePropertyGIFDictionary);
    if (dict != NULL && CFGetTypeID(dict) == CFDictionaryGetTypeID()) {
        *out_dict = dict;
        if (out_loop_key != NULL) {
            *out_loop_key = kCGImagePropertyGIFLoopCount;
        }
        if (out_delay_key != NULL) {
            *out_delay_key = kCGImagePropertyGIFDelayTime;
        }
        if (out_unclamped_delay_key != NULL) {
            *out_unclamped_delay_key = kCGImagePropertyGIFUnclampedDelayTime;
        }
        return 1;
    }

    dict = (CFDictionaryRef)CFDictionaryGetValue(props,
                                                 kCGImagePropertyPNGDictionary);
    if (dict != NULL &&
        CFGetTypeID(dict) == CFDictionaryGetTypeID() &&
        frame_count > 1u) {
        *out_dict = dict;
        if (out_loop_key != NULL) {
            *out_loop_key = kCGImagePropertyAPNGLoopCount;
        }
        if (out_delay_key != NULL) {
            *out_delay_key = kCGImagePropertyAPNGDelayTime;
        }
        if (out_unclamped_delay_key != NULL) {
            *out_unclamped_delay_key = kCGImagePropertyAPNGUnclampedDelayTime;
        }
        return 1;
    }

    dict = (CFDictionaryRef)CFDictionaryGetValue(
        props,
        kCGImagePropertyWebPDictionary);
    if (dict != NULL &&
        CFGetTypeID(dict) == CFDictionaryGetTypeID() &&
        frame_count > 1u) {
        *out_dict = dict;
        if (out_loop_key != NULL) {
            *out_loop_key = kCGImagePropertyWebPLoopCount;
        }
        if (out_delay_key != NULL) {
            *out_delay_key = kCGImagePropertyWebPDelayTime;
        }
        if (out_unclamped_delay_key != NULL) {
            *out_unclamped_delay_key = kCGImagePropertyWebPUnclampedDelayTime;
        }
        return 1;
    }

    dict = (CFDictionaryRef)CFDictionaryGetValue(
        props,
        kCGImagePropertyHEICSDictionary);
    if (dict != NULL &&
        CFGetTypeID(dict) == CFDictionaryGetTypeID() &&
        frame_count > 1u) {
        *out_dict = dict;
        if (out_loop_key != NULL) {
            *out_loop_key = kCGImagePropertyHEICSLoopCount;
        }
        if (out_delay_key != NULL) {
            *out_delay_key = kCGImagePropertyHEICSDelayTime;
        }
        if (out_unclamped_delay_key != NULL) {
            *out_unclamped_delay_key = kCGImagePropertyHEICSUnclampedDelayTime;
        }
        return 1;
    }

    return 0;
}

static int
coregraphics_resolve_animation_delay_cs(CFDictionaryRef dict,
                                        CFStringRef unclamped_delay_key,
                                        CFStringRef delay_key,
                                        int *delay_cs)
{
    double delay_value;
    double scaled_delay;
    double max_delay_seconds;

    delay_value = 0.0;
    scaled_delay = 0.0;
    max_delay_seconds = (double)INT_MAX / 100.0;
    if (dict == NULL || delay_cs == NULL) {
        return 0;
    }

    if (!coregraphics_dictionary_get_double(dict,
                                            unclamped_delay_key,
                                            &delay_value) &&
        !coregraphics_dictionary_get_double(dict,
                                            delay_key,
                                            &delay_value)) {
        return 0;
    }

    if (!(delay_value >= 0.0)) {
        delay_value = 0.0;
    }
    if (delay_value > max_delay_seconds) {
        *delay_cs = INT_MAX;
        return 1;
    }

    scaled_delay = delay_value * 100.0;
    if (scaled_delay >= (double)INT_MAX) {
        *delay_cs = INT_MAX;
        return 1;
    }

    *delay_cs = (int)scaled_delay;
    if (*delay_cs == 0 && delay_value > 0.0) {
        *delay_cs = 1;
    }
    return 1;
}

static int
coregraphics_image_is_indexed(CGImageRef image)
{
    CGColorSpaceRef color_space;

    color_space = NULL;
    if (image == NULL) {
        return 0;
    }

    color_space = CGImageGetColorSpace(image);
    if (color_space == NULL) {
        return 0;
    }
    return CGColorSpaceGetModel(color_space) == kCGColorSpaceModelIndexed;
}

static int
coregraphics_image_has_alpha(CGImageRef image, CFDictionaryRef frame_props)
{
    CGImageAlphaInfo alpha_info;
    int metadata_has_alpha;

    alpha_info = kCGImageAlphaNone;
    metadata_has_alpha = 0;
    if (image == NULL) {
        return 0;
    }

    metadata_has_alpha = coregraphics_dictionary_get_bool(
        frame_props,
        kCGImagePropertyHasAlpha,
        0);
    alpha_info = CGImageGetAlphaInfo(image);
    switch (alpha_info) {
    case kCGImageAlphaNone:
    case kCGImageAlphaNoneSkipLast:
    case kCGImageAlphaNoneSkipFirst:
        return metadata_has_alpha;
    default:
        return 1;
    }
}

static int
coregraphics_should_promote_float32(CGImageRef image, CFDictionaryRef props)
{
    int depth_bits;
    int is_float;
    size_t image_depth_bits;

    depth_bits = 0;
    is_float = 0;
    image_depth_bits = 0u;
    if (image == NULL) {
        return 0;
    }

    image_depth_bits = CGImageGetBitsPerComponent(image);
    depth_bits = (int)image_depth_bits;
    depth_bits = coregraphics_dictionary_get_int(props,
                                                 kCGImagePropertyDepth,
                                                 depth_bits);
    is_float = coregraphics_dictionary_get_bool(props,
                                                kCGImagePropertyIsFloat,
                                                0);
    if (is_float != 0 || depth_bits > 8) {
        return 1;
    }

    return 0;
}

static void
coregraphics_reset_frame_storage(sixel_frame_t *frame)
{
    if (frame == NULL || frame->allocator == NULL) {
        return;
    }

    if (frame->pixels.u8ptr != NULL) {
        sixel_allocator_free(frame->allocator, frame->pixels.u8ptr);
        frame->pixels.u8ptr = NULL;
    }
    if (frame->palette != NULL) {
        sixel_allocator_free(frame->allocator, frame->palette);
        frame->palette = NULL;
    }
    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
    }
    frame->transparent_mask_size = 0u;
    frame->ncolors = -1;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = 0;
}

static void
coregraphics_parse_png_transparency(CFDictionaryRef frame_props,
                                    int ncolors,
                                    unsigned char *zero_alpha_map,
                                    int *zero_alpha_count,
                                    int *has_partial_alpha)
{
    CFDictionaryRef png_dict;
    CFDataRef transparency_data;
    unsigned char const *alpha_bytes;
    CFIndex alpha_length;
    int alpha_count;
    int index;
    unsigned char alpha;

    png_dict = NULL;
    transparency_data = NULL;
    alpha_bytes = NULL;
    alpha_length = 0;
    alpha_count = 0;
    index = 0;
    alpha = 0u;
    if (zero_alpha_map == NULL ||
        zero_alpha_count == NULL ||
        has_partial_alpha == NULL ||
        ncolors <= 0) {
        return;
    }

    memset(zero_alpha_map, 0, (size_t)ncolors);
    *zero_alpha_count = 0;
    *has_partial_alpha = 0;

    if (frame_props == NULL) {
        return;
    }

    png_dict = (CFDictionaryRef)CFDictionaryGetValue(
        frame_props,
        kCGImagePropertyPNGDictionary);
    if (png_dict == NULL || CFGetTypeID(png_dict) != CFDictionaryGetTypeID()) {
        return;
    }

    transparency_data = (CFDataRef)CFDictionaryGetValue(
        png_dict,
        kCGImagePropertyPNGTransparency);
    if (transparency_data == NULL ||
        CFGetTypeID(transparency_data) != CFDataGetTypeID()) {
        return;
    }

    alpha_bytes = CFDataGetBytePtr(transparency_data);
    alpha_length = CFDataGetLength(transparency_data);
    if (alpha_bytes == NULL || alpha_length <= 0) {
        return;
    }

    alpha_count = (int)alpha_length;
    if (alpha_count > ncolors) {
        alpha_count = ncolors;
    }

    for (index = 0; index < alpha_count; ++index) {
        alpha = alpha_bytes[index];
        if (alpha == 0u) {
            zero_alpha_map[index] = 1u;
            *zero_alpha_count += 1;
        } else if (alpha != 255u) {
            *has_partial_alpha = 1;
        }
    }
}

static unsigned int
coregraphics_read_u32be(unsigned char const *bytes)
{
    unsigned int value;

    value = 0u;
    if (bytes == NULL) {
        return 0u;
    }

    value = (unsigned int)bytes[0] << 24;
    value |= (unsigned int)bytes[1] << 16;
    value |= (unsigned int)bytes[2] << 8;
    value |= (unsigned int)bytes[3];
    return value;
}

static void
coregraphics_png_trns_chunk_cache_init(coregraphics_png_trns_chunk_cache_t
                                       *cache)
{
    if (cache == NULL) {
        return;
    }

    cache->initialized = 0;
    cache->available = 0;
    cache->alpha_count = 0;
    memset(cache->alpha_entries, 0, sizeof(cache->alpha_entries));
}

static void
coregraphics_merge_png_transparency_entries(unsigned char const *alpha_entries,
                                            int alpha_count,
                                            int ncolors,
                                            unsigned char *zero_alpha_map,
                                            int *zero_alpha_count,
                                            int *has_partial_alpha)
{
    int index;
    int merge_count;
    unsigned char alpha;

    index = 0;
    merge_count = 0;
    alpha = 0u;
    if (alpha_entries == NULL ||
        alpha_count <= 0 ||
        ncolors <= 0 ||
        zero_alpha_map == NULL ||
        zero_alpha_count == NULL ||
        has_partial_alpha == NULL) {
        return;
    }

    merge_count = alpha_count;
    if (merge_count > ncolors) {
        merge_count = ncolors;
    }
    if (merge_count > SIXEL_PALETTE_MAX) {
        merge_count = SIXEL_PALETTE_MAX;
    }

    for (index = 0; index < merge_count; ++index) {
        alpha = alpha_entries[index];
        if (alpha == 0u) {
            if (zero_alpha_map[index] == 0u) {
                zero_alpha_map[index] = 1u;
                *zero_alpha_count += 1;
            }
        } else if (alpha != 255u) {
            *has_partial_alpha = 1;
        }
    }
}

static void
coregraphics_parse_png_transparency_chunk(
    sixel_chunk_t const *chunk,
    coregraphics_png_trns_chunk_cache_t *trns_cache,
    int ncolors,
    unsigned char *zero_alpha_map,
    int *zero_alpha_count,
    int *has_partial_alpha)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    size_t chunk_size;
    size_t chunk_length;
    unsigned char const *bytes;
    int alpha_count;
    int use_cache;

    offset = 0u;
    chunk_size = 0u;
    chunk_length = 0u;
    bytes = NULL;
    alpha_count = 0;
    use_cache = 0;
    if (chunk == NULL ||
        chunk->buffer == NULL ||
        chunk->size < sizeof(png_signature) ||
        ncolors <= 0 ||
        zero_alpha_map == NULL ||
        zero_alpha_count == NULL ||
        has_partial_alpha == NULL) {
        return;
    }

    if (trns_cache != NULL) {
        use_cache = 1;
        if (trns_cache->initialized != 0) {
            if (trns_cache->available == 0) {
                return;
            }
            coregraphics_merge_png_transparency_entries(
                trns_cache->alpha_entries,
                trns_cache->alpha_count,
                ncolors,
                zero_alpha_map,
                zero_alpha_count,
                has_partial_alpha);
            return;
        }
        trns_cache->initialized = 1;
        trns_cache->available = 0;
        trns_cache->alpha_count = 0;
        memset(trns_cache->alpha_entries, 0, sizeof(trns_cache->alpha_entries));
    }

    if (memcmp(chunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        return;
    }

    offset = sizeof(png_signature);
    while (offset + 8u <= chunk->size) {
        chunk_length = (size_t)coregraphics_read_u32be(chunk->buffer + offset);
        offset += 4u;
        if (offset + 4u > chunk->size) {
            break;
        }

        bytes = chunk->buffer + offset;
        if (offset + 4u + chunk_length + 4u > chunk->size) {
            break;
        }

        if (memcmp(bytes, "tRNS", 4u) == 0) {
            offset += 4u;
            chunk_size = chunk_length;
            if (chunk_size > (size_t)SIXEL_PALETTE_MAX) {
                chunk_size = (size_t)SIXEL_PALETTE_MAX;
            }
            alpha_count = (int)chunk_size;
            if (use_cache != 0) {
                if (alpha_count > 0) {
                    memcpy(trns_cache->alpha_entries,
                           chunk->buffer + offset,
                           (size_t)alpha_count);
                    trns_cache->alpha_count = alpha_count;
                    trns_cache->available = 1;
                    coregraphics_merge_png_transparency_entries(
                        trns_cache->alpha_entries,
                        trns_cache->alpha_count,
                        ncolors,
                        zero_alpha_map,
                        zero_alpha_count,
                        has_partial_alpha);
                }
            } else {
                coregraphics_merge_png_transparency_entries(
                    chunk->buffer + offset,
                    alpha_count,
                    ncolors,
                    zero_alpha_map,
                    zero_alpha_count,
                    has_partial_alpha);
            }
            break;
        }

        offset += 4u + chunk_length + 4u;
    }
}

static SIXELSTATUS
coregraphics_copy_indexed_palette(CGImageRef image,
                                  sixel_allocator_t *allocator,
                                  unsigned char **ppalette,
                                  int *pncolors)
{
    SIXELSTATUS status;
    CGColorSpaceRef color_space;
    CGColorSpaceRef base_space;
    size_t color_count;
    size_t component_count;
    size_t table_size;
    unsigned char *palette;
    unsigned char *table;
    size_t index;

    status = SIXEL_FALSE;
    color_space = NULL;
    base_space = NULL;
    color_count = 0u;
    component_count = 0u;
    table_size = 0u;
    palette = NULL;
    table = NULL;
    index = 0u;
    if (image == NULL ||
        allocator == NULL ||
        ppalette == NULL ||
        pncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppalette = NULL;
    *pncolors = 0;

    color_space = CGImageGetColorSpace(image);
    if (color_space == NULL ||
        CGColorSpaceGetModel(color_space) != kCGColorSpaceModelIndexed) {
        return SIXEL_FALSE;
    }

    color_count = CGColorSpaceGetColorTableCount(color_space);
    if (color_count == 0u || color_count > SIXEL_PALETTE_MAX) {
        return SIXEL_FALSE;
    }

    base_space = CGColorSpaceGetBaseColorSpace(color_space);
    if (base_space == NULL) {
        return SIXEL_FALSE;
    }

    component_count = CGColorSpaceGetNumberOfComponents(base_space);
    if (component_count == 0u || component_count > 4u) {
        return SIXEL_FALSE;
    }

    if (color_count > SIZE_MAX / component_count) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    table_size = color_count * component_count;
    table = (unsigned char *)sixel_allocator_malloc(allocator, table_size);
    if (table == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    CGColorSpaceGetColorTable(color_space, table);

    if (color_count > SIZE_MAX / 3u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      color_count * 3u);
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (index = 0u; index < color_count; ++index) {
        if (component_count >= 3u) {
            palette[index * 3u + 0u] = table[index * component_count + 0u];
            palette[index * 3u + 1u] = table[index * component_count + 1u];
            palette[index * 3u + 2u] = table[index * component_count + 2u];
        } else {
            palette[index * 3u + 0u] = table[index * component_count + 0u];
            palette[index * 3u + 1u] = table[index * component_count + 0u];
            palette[index * 3u + 2u] = table[index * component_count + 0u];
        }
    }

    *ppalette = palette;
    *pncolors = (int)color_count;
    palette = NULL;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, palette);
    sixel_allocator_free(allocator, table);
    return status;
}

static SIXELSTATUS
coregraphics_copy_indexed_pixels(CGImageRef image,
                                 sixel_allocator_t *allocator,
                                 int width,
                                 int height,
                                 unsigned char **ppixels)
{
    SIXELSTATUS status;
    CGDataProviderRef provider;
    CFDataRef provider_data;
    unsigned char const *src;
    unsigned char const *row;
    unsigned char *pixels;
    size_t bits_per_pixel;
    size_t bytes_per_row;
    size_t packed_row_bytes;
    size_t pixel_count;
    size_t needed_bytes;
    size_t data_size;
    size_t y;
    int x;
    unsigned char packed;
    int shift;

    status = SIXEL_FALSE;
    provider = NULL;
    provider_data = NULL;
    src = NULL;
    row = NULL;
    pixels = NULL;
    bits_per_pixel = 0u;
    bytes_per_row = 0u;
    packed_row_bytes = 0u;
    pixel_count = 0u;
    needed_bytes = 0u;
    data_size = 0u;
    y = 0u;
    x = 0;
    packed = 0u;
    shift = 0;
    if (image == NULL ||
        allocator == NULL ||
        ppixels == NULL ||
        width <= 0 ||
        height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppixels = NULL;
    bits_per_pixel = CGImageGetBitsPerPixel(image);
    bytes_per_row = CGImageGetBytesPerRow(image);
    if (bits_per_pixel != 1u &&
        bits_per_pixel != 2u &&
        bits_per_pixel != 4u &&
        bits_per_pixel != 8u) {
        return SIXEL_FALSE;
    }

    packed_row_bytes = ((size_t)width * bits_per_pixel + 7u) / 8u;
    if (bytes_per_row < packed_row_bytes) {
        return SIXEL_FALSE;
    }
    if ((size_t)height > SIZE_MAX / bytes_per_row) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    needed_bytes = (size_t)height * bytes_per_row;
    if (needed_bytes > (size_t)SIXEL_ALLOCATE_BYTES_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: indexed source row payload is too "
            "large.");
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > (size_t)SIXEL_ALLOCATE_BYTES_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: indexed image is too large.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    provider = CGImageGetDataProvider(image);
    if (provider == NULL) {
        status = SIXEL_FALSE;
        goto cleanup;
    }
    provider_data = CGDataProviderCopyData(provider);
    if (provider_data == NULL) {
        status = SIXEL_FALSE;
        goto cleanup;
    }
    src = CFDataGetBytePtr(provider_data);
    data_size = (size_t)CFDataGetLength(provider_data);
    if (src == NULL || data_size < needed_bytes) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    pixels = (unsigned char *)sixel_allocator_malloc(allocator, pixel_count);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (y = 0u; y < (size_t)height; ++y) {
        row = src + y * bytes_per_row;
        if (bits_per_pixel == 8u) {
            memcpy(pixels + y * (size_t)width, row, (size_t)width);
            continue;
        }
        for (x = 0; x < width; ++x) {
            packed = row[((size_t)x * bits_per_pixel) / 8u];
            shift = (int)(8u - bits_per_pixel
                          - ((size_t)x * bits_per_pixel) % 8u);
            pixels[y * (size_t)width + (size_t)x] =
                (unsigned char)((packed >> shift)
                                & ((1u << bits_per_pixel) - 1u));
        }
    }

    *ppixels = pixels;
    pixels = NULL;
    status = SIXEL_OK;

cleanup:
    if (provider_data != NULL) {
        CFRelease(provider_data);
    }
    sixel_allocator_free(allocator, pixels);
    return status;
}

static SIXELSTATUS
coregraphics_try_handle_indexed_frame(sixel_chunk_t const *chunk,
                                      sixel_frame_t *frame,
                                      CGImageRef image,
                                      CFDictionaryRef frame_props,
                                      coregraphics_png_trns_chunk_cache_t
                                      *png_trns_chunk_cache,
                                      int fuse_palette,
                                      int reqcolors,
                                      unsigned char const *bgcolor,
                                      int *handled,
                                      int *force_alpha)
{
    /*
     * Indexed-path policy:
     *   - emit PAL8 only for opaque indexed frames when palette output is
     *     enabled,
     *   - keep binary keycolor transparency in PAL8 when reqcolors allows it,
     *     otherwise preserve it as RGB+mask (or background composite),
     *   - defer non-binary alpha to the generic RGBA decode path.
     */
    SIXELSTATUS status;
    CGColorSpaceRef color_space;
    unsigned char *palette;
    unsigned char *indexed_pixels;
    unsigned char *rgb_pixels;
    unsigned char *mask;
    unsigned char zero_alpha_map[SIXEL_PALETTE_MAX];
    size_t pixel_count;
    size_t index;
    int ncolors;
    int allow_pal8;
    int reqcolors_clamped;
    int zero_alpha_count;
    int has_partial_alpha;
    int has_keycolor_alpha;
    int key_index;
    unsigned char palette_index;

    status = SIXEL_FALSE;
    color_space = NULL;
    palette = NULL;
    indexed_pixels = NULL;
    rgb_pixels = NULL;
    mask = NULL;
    pixel_count = 0u;
    index = 0u;
    ncolors = 0;
    allow_pal8 = 0;
    reqcolors_clamped = 0;
    zero_alpha_count = 0;
    has_partial_alpha = 0;
    has_keycolor_alpha = 0;
    key_index = -1;
    palette_index = 0u;
    if (chunk == NULL ||
        frame == NULL ||
        image == NULL ||
        handled == NULL ||
        force_alpha == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *handled = 0;
    *force_alpha = 0;

    color_space = CGImageGetColorSpace(image);
    if (color_space == NULL ||
        CGColorSpaceGetModel(color_space) != kCGColorSpaceModelIndexed) {
        return SIXEL_OK;
    }

    status = coregraphics_copy_indexed_palette(image,
                                               frame->allocator,
                                               &palette,
                                               &ncolors);
    if (status == SIXEL_FALSE) {
        return SIXEL_OK;
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    memset(zero_alpha_map, 0, sizeof(zero_alpha_map));
    coregraphics_parse_png_transparency(frame_props,
                                        ncolors,
                                        zero_alpha_map,
                                        &zero_alpha_count,
                                        &has_partial_alpha);
    coregraphics_parse_png_transparency_chunk(chunk,
                                              png_trns_chunk_cache,
                                              ncolors,
                                              zero_alpha_map,
                                              &zero_alpha_count,
                                              &has_partial_alpha);
    if (has_partial_alpha != 0) {
        *force_alpha = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    reqcolors_clamped = reqcolors;
    if (reqcolors_clamped <= 0 || reqcolors_clamped > SIXEL_PALETTE_MAX) {
        reqcolors_clamped = SIXEL_PALETTE_MAX;
    }
    has_keycolor_alpha = zero_alpha_count > 0 ? 1 : 0;
    allow_pal8 = fuse_palette != 0 &&
        ncolors > 0 &&
        ncolors <= reqcolors_clamped;
    if (has_keycolor_alpha != 0 && bgcolor != NULL) {
        allow_pal8 = 0;
    }
    if (has_keycolor_alpha != 0) {
        for (index = 0u; index < (size_t)ncolors; ++index) {
            if (zero_alpha_map[index] != 0u) {
                key_index = (int)index;
                break;
            }
        }
    }

    if (allow_pal8 == 0 && has_keycolor_alpha == 0) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (has_keycolor_alpha == 0) {
        key_index = -1;
    } else if (key_index < 0) {
        status = SIXEL_OK;
        goto cleanup;
    }

    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    status = coregraphics_copy_indexed_pixels(image,
                                              frame->allocator,
                                              frame->width,
                                              frame->height,
                                              &indexed_pixels);
    if (status == SIXEL_FALSE) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (allow_pal8 != 0) {
        if (has_keycolor_alpha != 0 &&
            zero_alpha_count > 1 &&
            key_index >= 0) {
            for (index = 0u; index < pixel_count; ++index) {
                palette_index = indexed_pixels[index];
                if ((int)palette_index < ncolors &&
                    zero_alpha_map[palette_index] != 0u &&
                    (int)palette_index != key_index) {
                    indexed_pixels[index] = (unsigned char)key_index;
                }
            }
        }
        frame->pixels.u8ptr = indexed_pixels;
        indexed_pixels = NULL;
        frame->palette = palette;
        palette = NULL;
        frame->ncolors = ncolors;
        frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->transparent = has_keycolor_alpha ? key_index : -1;
        frame->alpha_zero_is_transparent = 0;
        *handled = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    if (has_keycolor_alpha == 0) {
        status = SIXEL_OK;
        goto cleanup;
    }

    if (pixel_count > SIZE_MAX / 3u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                         pixel_count * 3u);
    if (rgb_pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    if (bgcolor == NULL) {
        mask = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                       pixel_count);
        if (mask == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }

    for (index = 0u; index < pixel_count; ++index) {
        palette_index = indexed_pixels[index];
        if ((int)palette_index >= ncolors) {
            palette_index = 0u;
        }
        if (zero_alpha_map[palette_index] != 0u) {
            if (bgcolor != NULL) {
                rgb_pixels[index * 3u + 0u] = bgcolor[0];
                rgb_pixels[index * 3u + 1u] = bgcolor[1];
                rgb_pixels[index * 3u + 2u] = bgcolor[2];
            } else {
                rgb_pixels[index * 3u + 0u] = 0u;
                rgb_pixels[index * 3u + 1u] = 0u;
                rgb_pixels[index * 3u + 2u] = 0u;
                mask[index] = 1u;
            }
        } else {
            rgb_pixels[index * 3u + 0u] =
                palette[(size_t)palette_index * 3u + 0u];
            rgb_pixels[index * 3u + 1u] =
                palette[(size_t)palette_index * 3u + 1u];
            rgb_pixels[index * 3u + 2u] =
                palette[(size_t)palette_index * 3u + 2u];
            if (mask != NULL) {
                mask[index] = 0u;
            }
        }
    }

    frame->pixels.u8ptr = rgb_pixels;
    rgb_pixels = NULL;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->ncolors = -1;
    frame->alpha_zero_is_transparent = 0;
    if (mask != NULL) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        mask = NULL;
    }
    *handled = 1;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(frame->allocator, palette);
    sixel_allocator_free(frame->allocator, indexed_pixels);
    sixel_allocator_free(frame->allocator, rgb_pixels);
    sixel_allocator_free(frame->allocator, mask);
    return status;
}

static CGColorSpaceRef
coregraphics_create_linear_colorspace(void)
{
    CGColorSpaceRef color_space;

    color_space = NULL;
    color_space = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
    if (color_space == NULL) {
        color_space = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
    }
    if (color_space == NULL) {
        color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    }

    return color_space;
}

static SIXELSTATUS
coregraphics_decode_rgba8_frame(sixel_frame_t *frame,
                                CGImageRef image,
                                unsigned char const *bgcolor,
                                int has_alpha_like,
                                coregraphics_srgb_lut_cache_t *srgb_lut_cache)
{
    /*
     * Draw into premultiplied RGBA and emit three-component output.
     * When alpha-like transparency is present and no bgcolor is given,
     * retain per-pixel zero-alpha semantics through transparent_mask.
     */
    SIXELSTATUS status;
    CGColorSpaceRef color_space;
    CGContextRef context;
    unsigned char *pixels;
    unsigned char *mask;
    size_t stride;
    size_t pixel_count;
    size_t index;
    unsigned int alpha;
    unsigned char r8;
    unsigned char g8;
    unsigned char b8;
    unsigned char out_r;
    unsigned char out_g;
    unsigned char out_b;
    double alpha_unit;
    double bg_linear[3];
    double src_linear_r;
    double src_linear_g;
    double src_linear_b;
    double out_linear_r;
    double out_linear_g;
    double out_linear_b;
    double const *decode_lut;
    unsigned char const *encode_lut;
    size_t encode_lut_size;

    status = SIXEL_FALSE;
    color_space = NULL;
    context = NULL;
    pixels = NULL;
    mask = NULL;
    stride = 0u;
    pixel_count = 0u;
    index = 0u;
    alpha = 0u;
    r8 = 0u;
    g8 = 0u;
    b8 = 0u;
    out_r = 0u;
    out_g = 0u;
    out_b = 0u;
    alpha_unit = 0.0;
    bg_linear[0] = 0.0;
    bg_linear[1] = 0.0;
    bg_linear[2] = 0.0;
    src_linear_r = 0.0;
    src_linear_g = 0.0;
    src_linear_b = 0.0;
    out_linear_r = 0.0;
    out_linear_g = 0.0;
    out_linear_b = 0.0;
    decode_lut = NULL;
    encode_lut = NULL;
    encode_lut_size = 0u;
    if (frame == NULL || image == NULL || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (frame->width > 0 && (size_t)frame->width > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    stride = (size_t)frame->width * 4u;
    if (pixel_count > 0u && (size_t)frame->height > SIZE_MAX / stride) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                     (size_t)frame->height *
                                                     stride);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (color_space == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGColorSpaceCreateWithName failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    context = CGBitmapContextCreate(pixels,
                                    (size_t)frame->width,
                                    (size_t)frame->height,
                                    8,
                                    stride,
                                    color_space,
                                    kCGImageAlphaPremultipliedLast |
                                        kCGBitmapByteOrder32Big);
    if (context == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGBitmapContextCreate failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 0.0);
    CGContextFillRect(context,
                      CGRectMake(0,
                                 0,
                                 (CGFloat)frame->width,
                                 (CGFloat)frame->height));
    CGContextSetBlendMode(context, kCGBlendModeNormal);
    CGContextDrawImage(context,
                       CGRectMake(0,
                                  0,
                                  (CGFloat)frame->width,
                                  (CGFloat)frame->height),
                       image);

    if (bgcolor != NULL) {
        bg_linear[0] = coregraphics_decode_srgb_unit((double)bgcolor[0] /
                                                     255.0);
        bg_linear[1] = coregraphics_decode_srgb_unit((double)bgcolor[1] /
                                                     255.0);
        bg_linear[2] = coregraphics_decode_srgb_unit((double)bgcolor[2] /
                                                     255.0);
    }

    if (has_alpha_like != 0 && bgcolor == NULL) {
        mask = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                       pixel_count);
        if (mask == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }
    if (has_alpha_like != 0) {
        if (srgb_lut_cache == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        if (srgb_lut_cache->prepared == 0) {
            coregraphics_build_srgb_decode_u8_lut(srgb_lut_cache->decode_lut);
            coregraphics_build_srgb_encode_u8_lut(
                srgb_lut_cache->encode_lut,
                sizeof(srgb_lut_cache->encode_lut));
            srgb_lut_cache->prepared = 1;
        }
        decode_lut = srgb_lut_cache->decode_lut;
        encode_lut = srgb_lut_cache->encode_lut;
        encode_lut_size = sizeof(srgb_lut_cache->encode_lut);
    }

    if (has_alpha_like == 0) {
        for (index = 0u; index < pixel_count; ++index) {
            pixels[index * 3u + 0u] = pixels[index * 4u + 0u];
            pixels[index * 3u + 1u] = pixels[index * 4u + 1u];
            pixels[index * 3u + 2u] = pixels[index * 4u + 2u];
        }
        frame->pixels.u8ptr = pixels;
        pixels = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->transparent = -1;
        frame->ncolors = -1;
        frame->alpha_zero_is_transparent = 0;
        status = SIXEL_OK;
        goto cleanup;
    }

    for (index = 0u; index < pixel_count; ++index) {
        alpha = pixels[index * 4u + 3u];
        if (alpha == 0u) {
            r8 = 0u;
            g8 = 0u;
            b8 = 0u;
            alpha_unit = 0.0;
            if (mask != NULL) {
                mask[index] = 1u;
            }
        } else if (alpha >= 255u) {
            r8 = pixels[index * 4u + 0u];
            g8 = pixels[index * 4u + 1u];
            b8 = pixels[index * 4u + 2u];
            alpha_unit = 1.0;
            if (mask != NULL) {
                mask[index] = 0u;
            }
        } else {
            r8 = coregraphics_unpremultiply_channel(pixels[index * 4u + 0u],
                                                    alpha);
            g8 = coregraphics_unpremultiply_channel(pixels[index * 4u + 1u],
                                                    alpha);
            b8 = coregraphics_unpremultiply_channel(pixels[index * 4u + 2u],
                                                    alpha);
            alpha_unit = (double)alpha / 255.0;
            if (mask != NULL) {
                mask[index] = 0u;
            }
        }

        src_linear_r = decode_lut[r8];
        src_linear_g = decode_lut[g8];
        src_linear_b = decode_lut[b8];

        if (bgcolor != NULL) {
            out_linear_r = src_linear_r * alpha_unit
                + bg_linear[0] * (1.0 - alpha_unit);
            out_linear_g = src_linear_g * alpha_unit
                + bg_linear[1] * (1.0 - alpha_unit);
            out_linear_b = src_linear_b * alpha_unit
                + bg_linear[2] * (1.0 - alpha_unit);
        } else {
            out_linear_r = src_linear_r * alpha_unit;
            out_linear_g = src_linear_g * alpha_unit;
            out_linear_b = src_linear_b * alpha_unit;
        }

        out_r = coregraphics_encode_linear_to_srgb_u8(out_linear_r,
                                                      encode_lut,
                                                      encode_lut_size);
        out_g = coregraphics_encode_linear_to_srgb_u8(out_linear_g,
                                                      encode_lut,
                                                      encode_lut_size);
        out_b = coregraphics_encode_linear_to_srgb_u8(out_linear_b,
                                                      encode_lut,
                                                      encode_lut_size);
        pixels[index * 3u + 0u] = out_r;
        pixels[index * 3u + 1u] = out_g;
        pixels[index * 3u + 2u] = out_b;
    }

    frame->pixels.u8ptr = pixels;
    pixels = NULL;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->ncolors = -1;
    frame->alpha_zero_is_transparent = 0;
    if (mask != NULL) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        mask = NULL;
    }
    status = SIXEL_OK;

cleanup:
    if (context != NULL) {
        CGContextRelease(context);
    }
    if (color_space != NULL) {
        CGColorSpaceRelease(color_space);
    }
    sixel_allocator_free(frame->allocator, pixels);
    sixel_allocator_free(frame->allocator, mask);
    return status;
}

static SIXELSTATUS
coregraphics_decode_float32_frame(sixel_frame_t *frame,
                                  CGImageRef image,
                                  unsigned char const *bgcolor,
                                  int has_alpha_like)
{
    /*
     * High-depth sources are decoded through a float32 linear context so
     * background composition and alpha handling stay in linear light.
     */
    SIXELSTATUS status;
    CGColorSpaceRef color_space;
    CGContextRef context;
    float *pixels;
    unsigned char *mask;
    size_t stride;
    size_t pixel_count;
    size_t index;
    float alpha;
    float out_r;
    float out_g;
    float out_b;
    float bg_linear[3];

    status = SIXEL_FALSE;
    color_space = NULL;
    context = NULL;
    pixels = NULL;
    mask = NULL;
    stride = 0u;
    pixel_count = 0u;
    index = 0u;
    alpha = 0.0f;
    out_r = 0.0f;
    out_g = 0.0f;
    out_b = 0.0f;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    if (frame == NULL || image == NULL || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (frame->width > 0 &&
        (size_t)frame->width > SIZE_MAX / (4u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    stride = (size_t)frame->width * 4u * sizeof(float);
    if (pixel_count > 0u && (size_t)frame->height > SIZE_MAX / stride) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixels = (float *)sixel_allocator_malloc(frame->allocator,
                                             (size_t)frame->height * stride);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    color_space = coregraphics_create_linear_colorspace();
    if (color_space == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGColorSpaceCreateWithName failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    context = CGBitmapContextCreate(pixels,
                                    (size_t)frame->width,
                                    (size_t)frame->height,
                                    32,
                                    stride,
                                    color_space,
                                    kCGImageAlphaPremultipliedLast |
                                        kCGBitmapByteOrder32Host |
                                        kCGBitmapFloatComponents);
    if (context == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGBitmapContextCreate failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 0.0);
    CGContextFillRect(context,
                      CGRectMake(0,
                                 0,
                                 (CGFloat)frame->width,
                                 (CGFloat)frame->height));
    CGContextSetBlendMode(context, kCGBlendModeNormal);
    CGContextDrawImage(context,
                       CGRectMake(0,
                                  0,
                                  (CGFloat)frame->width,
                                  (CGFloat)frame->height),
                       image);

    if (bgcolor != NULL) {
        bg_linear[0] = (float)coregraphics_decode_srgb_unit(
            (double)bgcolor[0] / 255.0);
        bg_linear[1] = (float)coregraphics_decode_srgb_unit(
            (double)bgcolor[1] / 255.0);
        bg_linear[2] = (float)coregraphics_decode_srgb_unit(
            (double)bgcolor[2] / 255.0);
    }

    if (has_alpha_like != 0 && bgcolor == NULL) {
        mask = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                       pixel_count);
        if (mask == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }

    if (has_alpha_like == 0) {
        for (index = 0u; index < pixel_count; ++index) {
            pixels[index * 3u + 0u] = pixels[index * 4u + 0u];
            pixels[index * 3u + 1u] = pixels[index * 4u + 1u];
            pixels[index * 3u + 2u] = pixels[index * 4u + 2u];
        }
    } else {
        for (index = 0u; index < pixel_count; ++index) {
            alpha = pixels[index * 4u + 3u];
            if (alpha < 0.0f) {
                alpha = 0.0f;
            } else if (alpha > 1.0f) {
                alpha = 1.0f;
            }

            out_r = pixels[index * 4u + 0u];
            out_g = pixels[index * 4u + 1u];
            out_b = pixels[index * 4u + 2u];
            if (bgcolor != NULL && alpha < 1.0f) {
                out_r += bg_linear[0] * (1.0f - alpha);
                out_g += bg_linear[1] * (1.0f - alpha);
                out_b += bg_linear[2] * (1.0f - alpha);
            } else if (mask != NULL) {
                if (alpha <= 0.0f) {
                    mask[index] = 1u;
                } else {
                    mask[index] = 0u;
                }
            }

            pixels[index * 3u + 0u] = out_r;
            pixels[index * 3u + 1u] = out_g;
            pixels[index * 3u + 2u] = out_b;
        }
    }

    frame->pixels.f32ptr = pixels;
    pixels = NULL;
    frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    frame->colorspace = SIXEL_COLORSPACE_LINEAR;
    frame->transparent = -1;
    frame->ncolors = -1;
    frame->alpha_zero_is_transparent = 0;
    if (mask != NULL) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        mask = NULL;
    }
    status = SIXEL_OK;

cleanup:
    if (context != NULL) {
        CGContextRelease(context);
    }
    if (color_space != NULL) {
        CGColorSpaceRelease(color_space);
    }
    sixel_allocator_free(frame->allocator, pixels);
    sixel_allocator_free(frame->allocator, mask);
    return status;
}


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
coregraphics_parse_frame_cache_max_bytes(size_t *max_bytes,
                                         int *cache_enabled)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    unsigned long long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0ull;
    if (max_bytes == NULL || cache_enabled == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *max_bytes = COREGRAPHICS_FRAME_CACHE_MAX_BYTES_DEFAULT;
    *cache_enabled = 1;
    env_value = sixel_compat_getenv(
        "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    errno = 0;
    parsed = strtoull(env_value, &endptr, 10);
    if (errno == ERANGE || endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES "
            "must be a non-negative integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed == 0ull) {
        *max_bytes = 0u;
        *cache_enabled = 0;
        goto end;
    }
    if (parsed > (unsigned long long)SIZE_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *max_bytes = (size_t)parsed;

end:
    return status;
}

static SIXELSTATUS
coregraphics_frame_measure_storage(sixel_frame_t const *frame,
                                   size_t *storage_bytes)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || storage_bytes == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL ||
        frame_if->vtbl->measure_storage == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->measure_storage(frame_if, storage_bytes);
}

static SIXELSTATUS
coregraphics_frame_set_handoff_shareable(sixel_frame_t *frame,
                                         int shareable)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    sixel_frame_timeline_t timeline;

    status = SIXEL_FALSE;
    frame_if = NULL;
    memset(&timeline, 0, sizeof(timeline));
    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL ||
        frame_if->vtbl->get_timeline == NULL ||
        frame_if->vtbl->set_timeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = frame_if->vtbl->get_timeline(frame_if, &timeline);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    timeline.handoff_shareable = shareable != 0 ? 1 : 0;
    return frame_if->vtbl->set_timeline(frame_if, &timeline);
}

static void
coregraphics_loader_state_init(
    coregraphics_loader_state_t *state,
    sixel_chunk_t const *chunk,
    int fstatic,
    int fuse_palette,
    int reqcolors,
    int enable_orientation,
    unsigned char *bgcolor,
    int loop_control,
    int start_frame_no_set,
    int start_frame_no_override,
    sixel_load_image_function fn_load,
    void *context)
{
    if (state == NULL) {
        return;
    }

    state->status = SIXEL_FALSE;
    state->chunk = chunk;
    state->fstatic = fstatic;
    state->fuse_palette = fuse_palette;
    state->reqcolors = reqcolors;
    state->enable_orientation = enable_orientation;
    state->bgcolor = bgcolor;
    state->loop_control = loop_control;
    state->start_frame_no_set = start_frame_no_set;
    state->start_frame_no_override = start_frame_no_override;
    state->fn_load = fn_load;
    state->context = context;
    state->frame = NULL;
    state->emit_frame = NULL;
    state->decode_frame = NULL;
    state->cached_frame_tmp = NULL;
    state->data = NULL;
    state->source = NULL;
    state->image = NULL;
    state->props = NULL;
    state->frame_props = NULL;
    state->frame_count = 0u;
    state->total_frames = 0;
    state->anim_loop_count = -1;
    state->is_animation_container = 0;
    state->source_orientation = 1;
    state->start_frame_no = INT_MIN;
    state->resolved_start_frame_no = INT_MIN;
    state->frame_index = 0;
    state->loop_no = 0;
    state->frames_in_loop = 0;
    state->stop_loop = 0;
    state->metadata_slots = 0u;
    state->single_meta_slot.delay = 0;
    state->single_meta_slot.orientation = 1;
    state->single_meta_slot.has_alpha = 0;
    state->single_meta_slot.promote_float32 = 0;
    state->single_meta_slot.is_indexed = 0;
    state->single_meta_slot.props_ready = 0u;
    state->single_meta_slot.decode_hint_ready = 0u;
    state->frame_meta_slots = NULL;
    state->active_meta_slots = NULL;
    state->frame_cache_slots = NULL;
    state->frame_cache_enabled = 0;
    state->frame_cache_max_bytes = 0u;
    state->frame_cache_used_bytes = 0u;
    state->frame_cache_frame_bytes = 0u;
    state->image_width = 0u;
    state->image_height = 0u;
    state->frame_cache_keep = 0;
    state->frame_cache_decision_pending = 0;
    state->release_emit_frame = 0;
    state->cache_hit = 0;
    state->indexed_handled = 0;
    state->force_alpha_from_indexed = 0;
    state->has_alpha_like = 0;
    state->promote_float32 = 0;
    state->frame_orientation = 1;
    state->frame_meta_slot = 0u;
    coregraphics_png_trns_chunk_cache_init(&state->png_trns_chunk_cache);
    state->rgba8_lut_cache.prepared = 0;
    state->cf_data_length = 0;
    state->prefetch_index = 0u;
}

static SIXELSTATUS
coregraphics_check_cancel(void *context)
{
    if (sixel_loader_callback_is_canceled(context)) {
        return SIXEL_INTERRUPTED;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
coregraphics_prepare_source_and_root_metadata(
    coregraphics_loader_state_t *state)
{
    SIXELSTATUS status;
    CFDictionaryRef anim_dict;
    CFStringRef anim_loop_key;

    status = SIXEL_OK;
    anim_dict = NULL;
    anim_loop_key = NULL;
    if (state == NULL || state->chunk == NULL || state->fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = coregraphics_parse_frame_cache_max_bytes(
        &state->frame_cache_max_bytes,
        &state->frame_cache_enabled);
    if (status != SIXEL_OK) {
        return status;
    }

    status = sixel_frame_new(&state->frame, state->chunk->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (state->chunk->size > (size_t)LONG_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: input chunk size is too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    state->cf_data_length = (CFIndex)state->chunk->size;
    state->data = CFDataCreate(kCFAllocatorDefault,
                               state->chunk->buffer,
                               state->cf_data_length);
    if (state->data == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CFDataCreate failed.");
        return SIXEL_FALSE;
    }

    state->source = CGImageSourceCreateWithData(state->data, NULL);
    if (state->source == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGImageSourceCreateWithData failed.");
        return SIXEL_FALSE;
    }

    state->frame_count = CGImageSourceGetCount(state->source);
    if (state->frame_count == 0u) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: input has no decodable frames.");
        return SIXEL_FALSE;
    }
    if (state->frame_count > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame count is too large.");
        return SIXEL_BAD_INPUT;
    }
    state->total_frames = (int)state->frame_count;

    state->props = CGImageSourceCopyProperties(state->source, NULL);
    if (state->props != NULL) {
        state->source_orientation = coregraphics_resolve_exif_orientation(
            state->props,
            state->source_orientation);
        /*
         * Treat multi-frame decoding as animation only when the source
         * exposes known animation dictionaries. This keeps multi-size ICO
         * decoding static while enabling APNG/WebP/HEICS animation.
         */
        if (coregraphics_get_animation_keys(state->props,
                                            state->frame_count,
                                            &anim_dict,
                                            &anim_loop_key,
                                            NULL,
                                            NULL)) {
            if (state->frame_count > 1u) {
                state->is_animation_container = 1;
            }
            state->anim_loop_count = coregraphics_dictionary_get_int(
                anim_dict,
                anim_loop_key,
                state->anim_loop_count);
        }
    }

    if (state->is_animation_container != 0) {
        /*
         * Keep start-frame controls animation-only so static decode paths do
         * not reject malformed env values.
         */
        if (state->start_frame_no_set != 0) {
            state->start_frame_no = state->start_frame_no_override;
        } else {
            status = coregraphics_parse_animation_start_frame_no(
                &state->start_frame_no);
            if (status != SIXEL_OK) {
                return status;
            }
        }
        if (state->start_frame_no != INT_MIN) {
            status = coregraphics_resolve_animation_start_frame_no(
                state->start_frame_no,
                state->total_frames,
                &state->resolved_start_frame_no);
            if (status != SIXEL_OK) {
                return status;
            }
        }
    }

    if (state->frame_cache_enabled != 0) {
        if (state->fstatic != 0 || state->is_animation_container == 0) {
            state->frame_cache_enabled = 0;
        }
    }
    if (state->fstatic == 0 && state->is_animation_container != 0) {
        state->metadata_slots = (size_t)state->total_frames;
    } else {
        /*
         * Static and non-animation paths emit one selected frame, so keep
         * metadata caches to a single slot instead of total frame count.
         */
        state->metadata_slots = 1u;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
coregraphics_prepare_metadata_slots(coregraphics_loader_state_t *state)
{
    if (state == NULL || state->frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (state->metadata_slots == 0u) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame metadata is too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (state->metadata_slots > 1u &&
        state->metadata_slots > SIZE_MAX / sizeof(*state->frame_meta_slots)) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame metadata is too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (state->metadata_slots > 1u) {
        state->frame_meta_slots = (coregraphics_frame_meta_slot_t *)
            sixel_allocator_calloc(state->frame->allocator,
                                   state->metadata_slots,
                                   sizeof(*state->frame_meta_slots));
        if (state->frame_meta_slots == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        state->active_meta_slots = state->frame_meta_slots;
    } else {
        state->single_meta_slot.orientation = state->source_orientation;
        state->active_meta_slots = &state->single_meta_slot;
    }
    if (state->frame_cache_enabled != 0) {
        if ((size_t)state->total_frames > SIZE_MAX /
            sizeof(*state->frame_cache_slots)) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: frame metadata is too large.");
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        state->frame_cache_slots = (coregraphics_cache_slot_t *)
            sixel_allocator_calloc(state->frame->allocator,
                                   (size_t)state->total_frames,
                                   sizeof(*state->frame_cache_slots));
        if (state->frame_cache_slots == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    return SIXEL_OK;
}

static SIXELSTATUS
coregraphics_prepare_frame_metadata(coregraphics_loader_state_t *state)
{
    SIXELSTATUS status;
    coregraphics_frame_meta_slot_t *slot;
    CFDictionaryRef frame_anim_dict;
    CFStringRef frame_loop_key;
    CFStringRef frame_delay_key;
    CFStringRef frame_unclamped_delay_key;

    status = SIXEL_OK;
    slot = NULL;
    frame_anim_dict = NULL;
    frame_loop_key = NULL;
    frame_delay_key = NULL;
    frame_unclamped_delay_key = NULL;
    if (state == NULL || state->active_meta_slots == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = &state->active_meta_slots[state->frame_meta_slot];
    if (slot->props_ready == 0u) {
        status = coregraphics_check_cancel(state->context);
        if (status != SIXEL_OK) {
            return status;
        }
        state->frame_props = CGImageSourceCopyPropertiesAtIndex(
            state->source,
            (size_t)state->frame_index,
            NULL);
        slot->delay = 0;
        if (state->frame_props != NULL) {
            if (coregraphics_get_animation_keys(state->frame_props,
                                                state->frame_count,
                                                &frame_anim_dict,
                                                &frame_loop_key,
                                                &frame_delay_key,
                                                &frame_unclamped_delay_key)) {
                state->anim_loop_count = coregraphics_dictionary_get_int(
                    frame_anim_dict,
                    frame_loop_key,
                    state->anim_loop_count);
                coregraphics_resolve_animation_delay_cs(
                    frame_anim_dict,
                    frame_unclamped_delay_key,
                    frame_delay_key,
                    &slot->delay);
            }
        }
        slot->orientation = coregraphics_resolve_exif_orientation(
            state->frame_props,
            state->source_orientation);
        slot->props_ready = 1u;
    }
    state->frame_orientation = slot->orientation;
    return SIXEL_OK;
}

static SIXELSTATUS
coregraphics_process_single_frame(coregraphics_loader_state_t *state)
{
    SIXELSTATUS status;
    coregraphics_frame_meta_slot_t *slot;
    coregraphics_cache_slot_t *cache_slot;

    status = SIXEL_OK;
    slot = NULL;
    cache_slot = NULL;
    if (state == NULL || state->active_meta_slots == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = &state->active_meta_slots[state->frame_meta_slot];
    cache_slot = NULL;
    if (state->frame_cache_slots != NULL) {
        cache_slot = &state->frame_cache_slots[(size_t)state->frame_index];
    }
    state->emit_frame = NULL;
    state->decode_frame = NULL;
    state->frame_cache_keep = 0;
    state->frame_cache_decision_pending = 0;
    state->release_emit_frame = 0;
    state->cache_hit = 0;
    if (cache_slot != NULL && cache_slot->frame != NULL) {
        state->emit_frame = cache_slot->frame;
        state->cache_hit = 1;
    }

    status = coregraphics_prepare_frame_metadata(state);
    if (status != SIXEL_OK) {
        return status;
    }

    if (state->cache_hit == 0) {
        status = coregraphics_check_cancel(state->context);
        if (status != SIXEL_OK) {
            return status;
        }
        state->image = CGImageSourceCreateImageAtIndex(
            state->source,
            (size_t)state->frame_index,
            NULL);
        if (state->image == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: "
                "CGImageSourceCreateImageAtIndex failed.");
            return SIXEL_FALSE;
        }

        if (cache_slot != NULL) {
            if (cache_slot->decided == 0u &&
                state->frame_cache_used_bytes >=
                state->frame_cache_max_bytes) {
                /*
                 * Once cache usage reaches the configured cap, every
                 * remaining frame must bypass cache. Mark it decided early so
                 * later loops skip temporary-frame probes.
                 */
                cache_slot->decided = 1u;
            }
            if (cache_slot->decided == 0u) {
                status = sixel_frame_new(&state->cached_frame_tmp,
                                         state->chunk->allocator);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                state->decode_frame = state->cached_frame_tmp;
                state->frame_cache_decision_pending = 1;
            } else {
                state->decode_frame = state->frame;
            }
        } else {
            state->decode_frame = state->frame;
        }

        if (slot->decode_hint_ready == 0u) {
            slot->is_indexed = coregraphics_image_is_indexed(state->image);
            slot->has_alpha = coregraphics_image_has_alpha(
                state->image,
                state->frame_props);
            slot->promote_float32 = coregraphics_should_promote_float32(
                state->image,
                state->frame_props);
            slot->decode_hint_ready = 1u;
        }

        state->image_width = CGImageGetWidth(state->image);
        state->image_height = CGImageGetHeight(state->image);
        if (state->image_width > (size_t)INT_MAX) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given width parameter is too"
                " huge.");
            return SIXEL_BAD_INPUT;
        }
        if (state->image_height > (size_t)INT_MAX) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given height parameter is too"
                " huge.");
            return SIXEL_BAD_INPUT;
        }
        state->decode_frame->width = (int)state->image_width;
        state->decode_frame->height = (int)state->image_height;
        if (state->image_width > (size_t)SIXEL_WIDTH_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given width parameter is too"
                " huge.");
            return SIXEL_BAD_INPUT;
        }
        if (state->image_height > (size_t)SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: given height parameter is too"
                " huge.");
            return SIXEL_BAD_INPUT;
        }
        if (state->decode_frame->width <= 0) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: an invalid width parameter"
                " detected.");
            return SIXEL_BAD_INPUT;
        }
        if (state->decode_frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: an invalid height parameter"
                " detected.");
            return SIXEL_BAD_INPUT;
        }
        if ((size_t)state->decode_frame->width >
            SIZE_MAX / (size_t)state->decode_frame->height) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: too large image.");
            return SIXEL_RUNTIME_ERROR;
        }

        coregraphics_reset_frame_storage(state->decode_frame);
        status = coregraphics_try_handle_indexed_frame(
            state->chunk,
            state->decode_frame,
            state->image,
            state->frame_props,
            &state->png_trns_chunk_cache,
            state->fuse_palette,
            state->reqcolors,
            state->bgcolor,
            &state->indexed_handled,
            &state->force_alpha_from_indexed);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (state->indexed_handled == 0) {
            status = coregraphics_check_cancel(state->context);
            if (status != SIXEL_OK) {
                return status;
            }
            state->has_alpha_like = state->force_alpha_from_indexed != 0 ||
                slot->has_alpha != 0;
            state->promote_float32 = slot->promote_float32;
            if (state->promote_float32 != 0) {
                status = coregraphics_decode_float32_frame(
                    state->decode_frame,
                    state->image,
                    state->bgcolor,
                    state->has_alpha_like);
            } else {
                status = coregraphics_decode_rgba8_frame(
                    state->decode_frame,
                    state->image,
                    state->bgcolor,
                    state->has_alpha_like,
                    &state->rgba8_lut_cache);
            }
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
        if (state->enable_orientation != 0 &&
            state->frame_orientation >= 2 &&
            state->frame_orientation <= 8) {
            status = loader_frame_apply_orientation(
                state->decode_frame,
                state->frame_orientation);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }

        if (cache_slot != NULL) {
            if (state->frame_cache_decision_pending != 0) {
                state->frame_cache_frame_bytes = 0u;
                if (SIXEL_FAILED(coregraphics_frame_measure_storage(
                        state->decode_frame,
                        &state->frame_cache_frame_bytes))) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: failed to estimate "
                        "decoded frame size.");
                    return SIXEL_BAD_INTEGER_OVERFLOW;
                }
                if (state->frame_cache_frame_bytes <=
                    state->frame_cache_max_bytes &&
                    state->frame_cache_used_bytes <=
                    state->frame_cache_max_bytes -
                    state->frame_cache_frame_bytes) {
                    state->frame_cache_keep = 1;
                }
                cache_slot->decided = 1u;
                if (state->frame_cache_keep != 0) {
                    status = coregraphics_frame_set_handoff_shareable(
                        state->decode_frame, 1);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    cache_slot->frame = state->decode_frame;
                    state->frame_cache_used_bytes +=
                        state->frame_cache_frame_bytes;
                    state->emit_frame = state->decode_frame;
                    state->cached_frame_tmp = NULL;
                } else {
                    status = coregraphics_frame_set_handoff_shareable(
                        state->decode_frame, 0);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    state->emit_frame = state->decode_frame;
                    state->release_emit_frame = 1;
                }
            } else {
                status = coregraphics_frame_set_handoff_shareable(
                    state->decode_frame, 0);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                state->emit_frame = state->decode_frame;
            }
        } else {
            state->emit_frame = state->decode_frame;
        }
    }

    if (state->frame_props != NULL) {
        CFRelease(state->frame_props);
        state->frame_props = NULL;
    }
    if (state->image != NULL) {
        CGImageRelease(state->image);
        state->image = NULL;
    }
    if (state->emit_frame == NULL && cache_slot != NULL) {
        state->emit_frame = cache_slot->frame;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
coregraphics_emit_frame(coregraphics_loader_state_t *state)
{
    SIXELSTATUS status;
    coregraphics_frame_meta_slot_t *slot;

    status = SIXEL_OK;
    slot = NULL;
    if (state == NULL || state->active_meta_slots == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (state->emit_frame == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: failed to select output frame.");
        return SIXEL_FALSE;
    }

    slot = &state->active_meta_slots[state->frame_meta_slot];
    sixel_frame_set_frame_no(state->emit_frame, state->frames_in_loop);
    sixel_frame_set_loop_count(state->emit_frame, state->loop_no);
    sixel_frame_set_delay(state->emit_frame, slot->delay);
    sixel_frame_set_multiframe(
        state->emit_frame,
        state->fstatic == 0 &&
        state->frame_count > 1u &&
        state->is_animation_container != 0);
    status = state->fn_load(state->emit_frame, state->context);
    if (status != SIXEL_OK) {
        return status;
    }

    return coregraphics_check_cancel(state->context);
}

static void
coregraphics_cleanup_state(coregraphics_loader_state_t *state)
{
    if (state == NULL) {
        return;
    }
    if (state->cached_frame_tmp != NULL) {
        sixel_frame_unref(state->cached_frame_tmp);
        state->cached_frame_tmp = NULL;
    }
    if (state->frame_props != NULL) {
        CFRelease(state->frame_props);
        state->frame_props = NULL;
    }
    if (state->image != NULL) {
        CGImageRelease(state->image);
        state->image = NULL;
    }
    if (state->source != NULL) {
        CFRelease(state->source);
        state->source = NULL;
    }
    if (state->props != NULL) {
        CFRelease(state->props);
        state->props = NULL;
    }
    if (state->data != NULL) {
        CFRelease(state->data);
        state->data = NULL;
    }
    if (state->frame != NULL) {
        if (state->frame_cache_slots != NULL) {
            for (state->prefetch_index = 0u;
                 state->prefetch_index < (size_t)state->total_frames;
                 ++state->prefetch_index) {
                if (state->frame_cache_slots[state->prefetch_index].frame !=
                    NULL) {
                    sixel_frame_unref(
                        state->frame_cache_slots[state->prefetch_index].frame);
                    state->frame_cache_slots[state->prefetch_index].frame =
                        NULL;
                }
            }
        }
        sixel_allocator_free(state->frame->allocator, state->frame_cache_slots);
        sixel_allocator_free(state->frame->allocator, state->frame_meta_slots);
        sixel_frame_unref(state->frame);
        state->frame = NULL;
    }
}

static SIXELSTATUS
load_with_coregraphics(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    int                       /* in */     enable_orientation,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    coregraphics_loader_state_t state;

    status = SIXEL_FALSE;
    coregraphics_loader_state_init(&state,
                                   pchunk,
                                   fstatic,
                                   fuse_palette,
                                   reqcolors,
                                   enable_orientation,
                                   bgcolor,
                                   loop_control,
                                   start_frame_no_set,
                                   start_frame_no_override,
                                   fn_load,
                                   context);

    status = coregraphics_prepare_source_and_root_metadata(&state);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = coregraphics_prepare_metadata_slots(&state);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_multiframe(
        state.frame,
        state.fstatic == 0 &&
        state.frame_count > 1u &&
        state.is_animation_container != 0);

    for (;;) {
        status = coregraphics_check_cancel(state.context);
        if (status != SIXEL_OK) {
            goto end;
        }

        state.frame_index = 0;
        if (state.loop_no == 0 && state.resolved_start_frame_no != INT_MIN) {
            /*
             * Apply start-frame override only on the first loop. Later loops
             * always restart from frame 0 to preserve normal replay behavior.
             */
            state.frame_index = state.resolved_start_frame_no;
        }
        state.frames_in_loop = 0;

        while (state.frame_index < state.total_frames) {
            status = coregraphics_check_cancel(state.context);
            if (status != SIXEL_OK) {
                goto end;
            }
            if (state.fstatic == 0 && state.is_animation_container != 0) {
                state.frame_meta_slot = (size_t)state.frame_index;
            } else {
                state.frame_meta_slot = 0u;
            }

            status = coregraphics_process_single_frame(&state);
            if (status != SIXEL_OK) {
                goto end;
            }
            status = coregraphics_emit_frame(&state);
            if (status != SIXEL_OK) {
                goto end;
            }

            ++state.frame_index;
            ++state.frames_in_loop;
            if (state.release_emit_frame != 0) {
                sixel_frame_unref(state.emit_frame);
                state.emit_frame = NULL;
                state.cached_frame_tmp = NULL;
            }
            if (state.fstatic != 0 || state.is_animation_container == 0) {
                status = SIXEL_OK;
                goto end;
            }
        }

        ++state.loop_no;
        state.stop_loop = 0;
        if (state.total_frames <= 1 ||
            state.loop_control == SIXEL_LOOP_DISABLE) {
            state.stop_loop = 1;
        } else if (state.loop_control == SIXEL_LOOP_AUTO) {
            if (state.anim_loop_count < 0) {
                state.stop_loop = 1;
            } else if (state.anim_loop_count > 0 &&
                       state.loop_no >= state.anim_loop_count) {
                state.stop_loop = 1;
            }
        }
        if (state.stop_loop != 0) {
            break;
        }
    }

    status = SIXEL_OK;

end:
    coregraphics_cleanup_state(&state);
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
    case SIXEL_LOADER_COMPONENT_OPTION_COREGRAPHICS_ENABLE_ORIENTATION:
        flag = (int const *)value;
        self->enable_orientation = (flag == NULL || *flag != 0) ? 1 : 0;
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

    self = (sixel_loader_coregraphics_component_t *)component;
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

    status = load_with_coregraphics(chunk,
                                    self->fstatic,
                                    self->fuse_palette,
                                    self->reqcolors,
                                    self->enable_orientation,
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
sixel_loader_coregraphics_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "coregraphics";
}

static int
sixel_loader_coregraphics_predicate(sixel_loader_component_t *component,
                                    sixel_chunk_t const *chunk)
{
    (void)component;
    (void)chunk;
    return 1;
}

static sixel_loader_component_vtbl_t const g_sixel_loader_coregraphics_vtbl = {
    sixel_loader_coregraphics_ref,
    sixel_loader_coregraphics_unref,
    sixel_loader_coregraphics_setopt,
    sixel_loader_coregraphics_load,
    sixel_loader_coregraphics_name,
    sixel_loader_coregraphics_predicate
};

SIXELSTATUS
sixel_loader_coregraphics_new(sixel_allocator_t *allocator,
                              void **ppcomponent)
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
    self->enable_orientation = 1;
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


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
