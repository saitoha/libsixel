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
#include "frame.h"
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
#define COREGRAPHICS_PALETTE_MATCH_TOLERANCE 3u
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

typedef struct coregraphics_png_indexed_metadata_cache {
    int initialized;
    SIXELSTATUS parse_status;
    unsigned char *palette;
    int ncolors;
    unsigned char zero_alpha_map[SIXEL_PALETTE_MAX];
    int zero_alpha_count;
    int has_partial_alpha;
    int has_keycolor;
} coregraphics_png_indexed_metadata_cache_t;

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
coregraphics_parse_png_transparency_chunk(sixel_chunk_t const *chunk,
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
    int index;
    unsigned char alpha;

    offset = 0u;
    chunk_size = 0u;
    chunk_length = 0u;
    bytes = NULL;
    alpha_count = 0;
    index = 0;
    alpha = 0u;
    if (chunk == NULL ||
        chunk->buffer == NULL ||
        chunk->size < sizeof(png_signature) ||
        ncolors <= 0 ||
        zero_alpha_map == NULL ||
        zero_alpha_count == NULL ||
        has_partial_alpha == NULL) {
        return;
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
            if ((int)chunk_size > ncolors) {
                chunk_size = (size_t)ncolors;
            }
            alpha_count = (int)chunk_size;
            for (index = 0; index < alpha_count; ++index) {
                alpha = chunk->buffer[offset + (size_t)index];
                if (alpha == 0u) {
                    if (zero_alpha_map[index] == 0u) {
                        zero_alpha_map[index] = 1u;
                        *zero_alpha_count += 1;
                    }
                } else if (alpha != 255u) {
                    *has_partial_alpha = 1;
                }
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

    provider = CGImageGetDataProvider(image);
    if (provider == NULL) {
        return SIXEL_FALSE;
    }
    provider_data = CGDataProviderCopyData(provider);
    if (provider_data == NULL) {
        return SIXEL_FALSE;
    }
    src = CFDataGetBytePtr(provider_data);
    data_size = (size_t)CFDataGetLength(provider_data);
    if (src == NULL || data_size < needed_bytes) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)width * (size_t)height;
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
coregraphics_parse_png_indexed_metadata(
    sixel_chunk_t const *chunk,
    sixel_allocator_t *allocator,
    unsigned char **ppalette,
    int *pncolors,
    unsigned char *zero_alpha_map,
    int *zero_alpha_count,
    int *has_partial_alpha,
    int *has_keycolor)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    SIXELSTATUS status;
    size_t offset;
    size_t chunk_length;
    size_t color_count;
    unsigned char const *type_ptr;
    unsigned char const *data_ptr;
    unsigned char *palette;
    int ihdr_color_type;
    int index;
    int alpha_count;
    unsigned char alpha;

    status = SIXEL_FALSE;
    offset = 0u;
    chunk_length = 0u;
    color_count = 0u;
    type_ptr = NULL;
    data_ptr = NULL;
    palette = NULL;
    ihdr_color_type = -1;
    index = 0;
    alpha_count = 0;
    alpha = 0u;
    if (chunk == NULL ||
        allocator == NULL ||
        ppalette == NULL ||
        pncolors == NULL ||
        zero_alpha_map == NULL ||
        zero_alpha_count == NULL ||
        has_partial_alpha == NULL ||
        has_keycolor == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppalette = NULL;
    *pncolors = 0;
    *zero_alpha_count = 0;
    *has_partial_alpha = 0;
    *has_keycolor = 0;
    memset(zero_alpha_map, 0, SIXEL_PALETTE_MAX);

    if (chunk->buffer == NULL || chunk->size < sizeof(png_signature)) {
        return SIXEL_FALSE;
    }
    if (memcmp(chunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        return SIXEL_FALSE;
    }

    offset = sizeof(png_signature);
    while (offset + 8u <= chunk->size) {
        chunk_length = (size_t)coregraphics_read_u32be(chunk->buffer + offset);
        offset += 4u;
        if (offset + 4u + chunk_length + 4u > chunk->size) {
            status = SIXEL_FALSE;
            goto cleanup;
        }

        type_ptr = chunk->buffer + offset;
        offset += 4u;
        data_ptr = chunk->buffer + offset;

        if (memcmp(type_ptr, "IHDR", 4u) == 0) {
            if (chunk_length < 13u) {
                status = SIXEL_FALSE;
                goto cleanup;
            }
            ihdr_color_type = (int)data_ptr[9];
        } else if (memcmp(type_ptr, "PLTE", 4u) == 0) {
            if (chunk_length == 0u ||
                chunk_length % 3u != 0u ||
                chunk_length > (size_t)SIXEL_PALETTE_MAX * 3u) {
                status = SIXEL_FALSE;
                goto cleanup;
            }

            color_count = chunk_length / 3u;
            palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                              color_count * 3u);
            if (palette == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            memcpy(palette, data_ptr, color_count * 3u);
        } else if (memcmp(type_ptr, "tRNS", 4u) == 0 &&
                   palette != NULL &&
                   color_count > 0u) {
            alpha_count = (int)chunk_length;
            if (alpha_count > (int)color_count) {
                alpha_count = (int)color_count;
            }
            for (index = 0; index < alpha_count; ++index) {
                alpha = data_ptr[index];
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

        offset += chunk_length + 4u;
        if (memcmp(type_ptr, "IEND", 4u) == 0) {
            break;
        }
    }

    if (ihdr_color_type != 3 || palette == NULL || color_count == 0u) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    *ppalette = palette;
    palette = NULL;
    *pncolors = (int)color_count;
    *has_keycolor = *zero_alpha_count > 0 ? 1 : 0;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, palette);
    return status;
}

static void
coregraphics_png_indexed_metadata_cache_init(
    coregraphics_png_indexed_metadata_cache_t *cache)
{
    if (cache == NULL) {
        return;
    }

    cache->initialized = 0;
    cache->parse_status = SIXEL_FALSE;
    cache->palette = NULL;
    cache->ncolors = 0;
    memset(cache->zero_alpha_map, 0, sizeof(cache->zero_alpha_map));
    cache->zero_alpha_count = 0;
    cache->has_partial_alpha = 0;
    cache->has_keycolor = 0;
}

static void
coregraphics_png_indexed_metadata_cache_reset(
    coregraphics_png_indexed_metadata_cache_t *cache,
    sixel_allocator_t *allocator)
{
    if (cache == NULL || allocator == NULL) {
        return;
    }

    sixel_allocator_free(allocator, cache->palette);
    coregraphics_png_indexed_metadata_cache_init(cache);
}

static SIXELSTATUS
coregraphics_png_indexed_metadata_cache_prepare(
    coregraphics_png_indexed_metadata_cache_t *cache,
    sixel_chunk_t const *chunk,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (cache == NULL || chunk == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (cache->initialized != 0) {
        return cache->parse_status;
    }

    cache->initialized = 1;
    status = coregraphics_parse_png_indexed_metadata(
        chunk,
        allocator,
        &cache->palette,
        &cache->ncolors,
        cache->zero_alpha_map,
        &cache->zero_alpha_count,
        &cache->has_partial_alpha,
        &cache->has_keycolor);
    cache->parse_status = status;
    if (status != SIXEL_OK && status != SIXEL_FALSE) {
        sixel_allocator_free(allocator, cache->palette);
        cache->palette = NULL;
        cache->ncolors = 0;
    }
    return status;
}

static int
coregraphics_find_palette_index(unsigned char const *palette,
                                int ncolors,
                                unsigned char r,
                                unsigned char g,
                                unsigned char b)
{
    int index;

    index = 0;
    if (palette == NULL || ncolors <= 0) {
        return -1;
    }

    for (index = 0; index < ncolors; ++index) {
        if (palette[(size_t)index * 3u + 0u] == r &&
            palette[(size_t)index * 3u + 1u] == g &&
            palette[(size_t)index * 3u + 2u] == b) {
            return index;
        }
    }
    return -1;
}

static int
coregraphics_find_palette_index_with_tolerance(unsigned char const *palette,
                                               int ncolors,
                                               unsigned char r,
                                               unsigned char g,
                                               unsigned char b,
                                               unsigned int tolerance)
{
    int exact_index;
    int best_index;
    int index;
    int dr;
    int dg;
    int db;
    unsigned int distance;
    unsigned int best_distance;
    unsigned int max_distance;

    exact_index = -1;
    best_index = -1;
    index = 0;
    dr = 0;
    dg = 0;
    db = 0;
    distance = 0u;
    best_distance = 0u;
    max_distance = 0u;
    exact_index = coregraphics_find_palette_index(palette, ncolors, r, g, b);
    if (exact_index >= 0) {
        return exact_index;
    }
    if (palette == NULL || ncolors <= 0 || tolerance == 0u) {
        return -1;
    }

    max_distance = tolerance * tolerance * 3u;
    best_distance = max_distance + 1u;
    for (index = 0; index < ncolors; ++index) {
        dr = (int)palette[(size_t)index * 3u + 0u] - (int)r;
        dg = (int)palette[(size_t)index * 3u + 1u] - (int)g;
        db = (int)palette[(size_t)index * 3u + 2u] - (int)b;
        distance = (unsigned int)(dr * dr + dg * dg + db * db);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
            if (distance == 0u) {
                break;
            }
        }
    }

    if (best_index >= 0 && best_distance <= max_distance) {
        return best_index;
    }
    return -1;
}

static SIXELSTATUS
coregraphics_try_handle_png_indexed_keycolor(
    coregraphics_png_indexed_metadata_cache_t const *png_indexed_cache,
    sixel_frame_t *frame,
    CGImageRef image,
    int fuse_palette,
    int reqcolors,
    unsigned char const *bgcolor,
    int *handled,
    int *force_alpha)
{
    /*
     * CoreGraphics can expand indexed PNG+tRNS into RGBA and lose the indexed
     * color space model. Reconstruct PAL8 output by matching decoded RGB pixels
     * to PLTE entries when transparency is binary keycolor.
     */
    SIXELSTATUS status;
    unsigned char *palette;
    unsigned char *indexed_pixels;
    unsigned char *rgba_pixels;
    unsigned char zero_alpha_map[SIXEL_PALETTE_MAX];
    CGColorSpaceRef image_color_space;
    CGColorSpaceRef color_space;
    CGContextRef context;
    size_t stride;
    size_t pixel_count;
    size_t index;
    int ncolors;
    int has_partial_alpha;
    int has_keycolor;
    int allow_pal8;
    int key_index;
    int palette_index;
    unsigned int alpha;
    unsigned char r8;
    unsigned char g8;
    unsigned char b8;
    int owns_color_space;

    status = SIXEL_FALSE;
    palette = NULL;
    indexed_pixels = NULL;
    rgba_pixels = NULL;
    image_color_space = NULL;
    color_space = NULL;
    context = NULL;
    stride = 0u;
    pixel_count = 0u;
    index = 0u;
    ncolors = 0;
    has_partial_alpha = 0;
    has_keycolor = 0;
    allow_pal8 = 0;
    key_index = -1;
    palette_index = -1;
    alpha = 0u;
    r8 = 0u;
    g8 = 0u;
    b8 = 0u;
    owns_color_space = 0;
    if (png_indexed_cache == NULL ||
        frame == NULL ||
        image == NULL ||
        handled == NULL ||
        force_alpha == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *handled = 0;

    status = png_indexed_cache->parse_status;
    if (status == SIXEL_FALSE) {
        return SIXEL_OK;
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (png_indexed_cache->palette == NULL ||
        png_indexed_cache->ncolors <= 0 ||
        png_indexed_cache->ncolors > SIXEL_PALETTE_MAX) {
        return SIXEL_OK;
    }
    ncolors = png_indexed_cache->ncolors;
    memcpy(zero_alpha_map,
           png_indexed_cache->zero_alpha_map,
           sizeof(zero_alpha_map));
    has_partial_alpha = png_indexed_cache->has_partial_alpha;
    has_keycolor = png_indexed_cache->has_keycolor;
    if (has_keycolor == 0) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (has_partial_alpha != 0) {
        *force_alpha = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    allow_pal8 = fuse_palette != 0 &&
        reqcolors > 0 &&
        ncolors > 0 &&
        ncolors <= reqcolors;
    if (bgcolor != NULL) {
        allow_pal8 = 0;
    }
    if (allow_pal8 == 0) {
        *force_alpha = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    for (index = 0u; index < (size_t)ncolors; ++index) {
        if (zero_alpha_map[index] != 0u) {
            key_index = (int)index;
            break;
        }
    }
    if (key_index < 0) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if ((size_t)ncolors > SIZE_MAX / 3u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    palette = (unsigned char *)sixel_allocator_malloc(
        frame->allocator,
        (size_t)ncolors * 3u);
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    memcpy(palette,
           png_indexed_cache->palette,
           (size_t)ncolors * 3u);

    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if ((size_t)frame->width > SIZE_MAX / 4u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    stride = (size_t)frame->width * 4u;

    rgba_pixels = (unsigned char *)sixel_allocator_calloc(frame->allocator,
                                                          pixel_count,
                                                          4u);
    if (rgba_pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_calloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    indexed_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                             pixel_count);
    if (indexed_pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    image_color_space = CGImageGetColorSpace(image);
    if (image_color_space != NULL &&
        CGColorSpaceGetModel(image_color_space) == kCGColorSpaceModelRGB) {
        color_space = image_color_space;
        owns_color_space = 0;
    } else {
        color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        if (color_space == NULL) {
            color_space = CGColorSpaceCreateDeviceRGB();
        }
        owns_color_space = 1;
    }
    if (color_space == NULL) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    context = CGBitmapContextCreate(rgba_pixels,
                                    (size_t)frame->width,
                                    (size_t)frame->height,
                                    8u,
                                    stride,
                                    color_space,
                                    kCGImageAlphaPremultipliedLast |
                                    kCGBitmapByteOrder32Big);
    if (context == NULL) {
        status = SIXEL_FALSE;
        goto cleanup;
    }
    CGContextDrawImage(context,
                       CGRectMake(0.0,
                                  0.0,
                                  frame->width,
                                  frame->height),
                       image);

    for (index = 0u; index < pixel_count; ++index) {
        r8 = rgba_pixels[index * 4u + 0u];
        g8 = rgba_pixels[index * 4u + 1u];
        b8 = rgba_pixels[index * 4u + 2u];
        alpha = rgba_pixels[index * 4u + 3u];
        if (alpha == 0u) {
            indexed_pixels[index] = (unsigned char)key_index;
            continue;
        }
        if (alpha != 255u) {
            *force_alpha = 1;
            status = SIXEL_OK;
            goto cleanup;
        }

        palette_index = coregraphics_find_palette_index_with_tolerance(
            palette,
            ncolors,
            r8,
            g8,
            b8,
            COREGRAPHICS_PALETTE_MATCH_TOLERANCE);
        if (palette_index < 0) {
            *force_alpha = 1;
            status = SIXEL_OK;
            goto cleanup;
        }
        indexed_pixels[index] = (unsigned char)palette_index;
    }

    frame->pixels.u8ptr = indexed_pixels;
    indexed_pixels = NULL;
    frame->palette = palette;
    palette = NULL;
    frame->ncolors = ncolors;
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = key_index;
    frame->alpha_zero_is_transparent = 0;
    *handled = 1;
    status = SIXEL_OK;

cleanup:
    if (context != NULL) {
        CGContextRelease(context);
    }
    if (owns_color_space != 0 && color_space != NULL) {
        CGColorSpaceRelease(color_space);
    }
    sixel_allocator_free(frame->allocator, palette);
    sixel_allocator_free(frame->allocator, indexed_pixels);
    sixel_allocator_free(frame->allocator, rgba_pixels);
    return status;
}

static SIXELSTATUS
coregraphics_try_handle_indexed_frame(sixel_chunk_t const *chunk,
                                      sixel_frame_t *frame,
                                      CGImageRef image,
                                      CFDictionaryRef frame_props,
                                      coregraphics_png_indexed_metadata_cache_t
                                      const *png_indexed_cache,
                                      int fuse_palette,
                                      int reqcolors,
                                      unsigned char const *bgcolor,
                                      int *handled,
                                      int *force_alpha)
{
    /*
     * Indexed fast path policy:
     *   - keep PAL8 when palette size fits reqcolors,
     *   - preserve keycolor transparency when possible,
     *   - otherwise fall back to RGB(+mask or bg composite).
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
    int zero_alpha_count;
    int has_partial_alpha;
    int key_index;
    int has_keycolor;
    int merged_ncolors;
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
    zero_alpha_count = 0;
    has_partial_alpha = 0;
    key_index = -1;
    has_keycolor = 0;
    merged_ncolors = 0;
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
    if (png_indexed_cache != NULL &&
        png_indexed_cache->parse_status != SIXEL_OK &&
        png_indexed_cache->parse_status != SIXEL_FALSE) {
        status = png_indexed_cache->parse_status;
        goto cleanup;
    }
    if (png_indexed_cache != NULL &&
        png_indexed_cache->parse_status == SIXEL_OK &&
        png_indexed_cache->ncolors > 0) {
        merged_ncolors = ncolors;
        if (png_indexed_cache->ncolors < merged_ncolors) {
            merged_ncolors = png_indexed_cache->ncolors;
        }
        for (index = 0u; index < (size_t)merged_ncolors; ++index) {
            if (png_indexed_cache->zero_alpha_map[index] != 0u &&
                zero_alpha_map[index] == 0u) {
                zero_alpha_map[index] = 1u;
                zero_alpha_count += 1;
            }
        }
        if (png_indexed_cache->has_partial_alpha != 0) {
            has_partial_alpha = 1;
        }
    } else {
        coregraphics_parse_png_transparency_chunk(chunk,
                                                  ncolors,
                                                  zero_alpha_map,
                                                  &zero_alpha_count,
                                                  &has_partial_alpha);
    }
    if (has_partial_alpha != 0) {
        *force_alpha = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    if (zero_alpha_count > 0) {
        has_keycolor = 1;
        for (index = 0u; index < (size_t)ncolors; ++index) {
            if (zero_alpha_map[index] != 0u) {
                key_index = (int)index;
                break;
            }
        }
    }

    allow_pal8 = fuse_palette != 0 &&
        reqcolors > 0 &&
        ncolors > 0 &&
        ncolors <= reqcolors;
    if (has_keycolor && bgcolor != NULL) {
        allow_pal8 = 0;
    }

    if (!allow_pal8 && !has_keycolor) {
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

    if (has_keycolor && !allow_pal8) {
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
                    "load_with_coregraphics: sixel_allocator_malloc() "
                    "failed.");
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
        goto cleanup;
    }

    if (!allow_pal8) {
        status = SIXEL_OK;
        goto cleanup;
    }

    if (has_keycolor && zero_alpha_count > 1 && key_index >= 0) {
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
    frame->transparent = has_keycolor ? key_index : -1;
    frame->alpha_zero_is_transparent = 0;
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

static int
coregraphics_measure_frame_cache_bytes(sixel_frame_t const *frame,
                                       size_t *total_bytes)
{
    int depth;
    size_t pixel_count;
    size_t pixels_bytes;
    size_t palette_bytes;
    size_t mask_bytes;

    depth = 0;
    pixel_count = 0u;
    pixels_bytes = 0u;
    palette_bytes = 0u;
    mask_bytes = 0u;
    if (frame == NULL || total_bytes == NULL) {
        return 0;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }
    depth = sixel_helper_compute_depth(frame->pixelformat);
    if (depth <= 0) {
        return 0;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (pixel_count > SIZE_MAX / (size_t)depth) {
        return 0;
    }
    pixels_bytes = pixel_count * (size_t)depth;
    *total_bytes = pixels_bytes;

    /*
     * Include palette and transparency mask storage so cache limiting follows
     * the decoded frame footprint rather than a fixed bytes-per-pixel guess.
     */
    if (frame->palette != NULL && frame->ncolors > 0) {
        if ((size_t)frame->ncolors > SIZE_MAX / 3u) {
            return 0;
        }
        palette_bytes = (size_t)frame->ncolors * 3u;
        if (*total_bytes > SIZE_MAX - palette_bytes) {
            return 0;
        }
        *total_bytes += palette_bytes;
    }
    if (frame->transparent_mask != NULL && frame->transparent_mask_size > 0u) {
        mask_bytes = frame->transparent_mask_size;
        if (*total_bytes > SIZE_MAX - mask_bytes) {
            return 0;
        }
        *total_bytes += mask_bytes;
    }
    return 1;
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
    sixel_frame_t *frame;
    sixel_frame_t *emit_frame;
    sixel_frame_t *decode_frame;
    sixel_frame_t *cached_frame_tmp;
    CFDataRef data;
    CGImageSourceRef source;
    CGImageRef image;
    size_t frame_count;
    int anim_loop_count;
    CFDictionaryRef props;
    CFDictionaryRef anim_dict;
    CFDictionaryRef frame_props;
    CFDictionaryRef frame_anim_dict;
    CFStringRef anim_loop_key;
    CFStringRef frame_loop_key;
    CFStringRef frame_delay_key;
    CFStringRef frame_unclamped_delay_key;
    int start_frame_no;
    int resolved_start_frame_no;
    int total_frames;
    int frame_index;
    int frames_in_loop;
    int loop_no;
    int stop_loop;
    int is_animation_container;
    int indexed_handled;
    int force_alpha_from_indexed;
    int has_alpha_like;
    int promote_float32;
    int source_orientation;
    int frame_orientation;
    coregraphics_png_indexed_metadata_cache_t png_indexed_cache;
    coregraphics_srgb_lut_cache_t rgba8_lut_cache;
    CFDictionaryRef single_frame_props_cache;
    int single_frame_delay_cache;
    int single_frame_orientation_cache;
    unsigned char single_frame_props_ready;
    CFDictionaryRef *frame_props_cache;
    int *frame_delay_cache;
    int *frame_orientation_cache;
    unsigned char *frame_props_ready;
    CFDictionaryRef *active_frame_props_cache;
    int *active_frame_delay_cache;
    int *active_frame_orientation_cache;
    unsigned char *active_frame_props_ready;
    unsigned char *frame_cache_decided;
    sixel_frame_t **frame_cache;
    CFIndex cf_data_length;
    size_t metadata_slots;
    size_t frame_meta_slot;
    size_t prefetch_index;
    size_t frame_cache_max_bytes;
    size_t frame_cache_used_bytes;
    size_t frame_cache_frame_bytes;
    size_t image_width;
    size_t image_height;
    int frame_cache_enabled;
    int frame_cache_keep;
    int frame_cache_decision_pending;
    int release_emit_frame;
    int cache_hit;

    status = SIXEL_FALSE;
    frame = NULL;
    emit_frame = NULL;
    decode_frame = NULL;
    cached_frame_tmp = NULL;
    data = NULL;
    source = NULL;
    image = NULL;
    frame_count = 0u;
    anim_loop_count = -1;
    props = NULL;
    anim_dict = NULL;
    frame_props = NULL;
    frame_anim_dict = NULL;
    anim_loop_key = NULL;
    frame_loop_key = NULL;
    frame_delay_key = NULL;
    frame_unclamped_delay_key = NULL;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    total_frames = 0;
    frame_index = 0;
    frames_in_loop = 0;
    loop_no = 0;
    stop_loop = 0;
    is_animation_container = 0;
    indexed_handled = 0;
    force_alpha_from_indexed = 0;
    has_alpha_like = 0;
    promote_float32 = 0;
    source_orientation = 1;
    frame_orientation = 1;
    coregraphics_png_indexed_metadata_cache_init(&png_indexed_cache);
    rgba8_lut_cache.prepared = 0;
    single_frame_props_cache = NULL;
    single_frame_delay_cache = 0;
    single_frame_orientation_cache = 1;
    single_frame_props_ready = 0u;
    frame_props_cache = NULL;
    frame_delay_cache = NULL;
    frame_orientation_cache = NULL;
    frame_props_ready = NULL;
    active_frame_props_cache = NULL;
    active_frame_delay_cache = NULL;
    active_frame_orientation_cache = NULL;
    active_frame_props_ready = NULL;
    frame_cache_decided = NULL;
    frame_cache = NULL;
    cf_data_length = 0;
    metadata_slots = 0u;
    frame_meta_slot = 0u;
    prefetch_index = 0u;
    frame_cache_max_bytes = 0u;
    frame_cache_used_bytes = 0u;
    frame_cache_frame_bytes = 0u;
    image_width = 0u;
    image_height = 0u;
    frame_cache_enabled = 0;
    frame_cache_keep = 0;
    frame_cache_decision_pending = 0;
    release_emit_frame = 0;
    cache_hit = 0;

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = coregraphics_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = coregraphics_parse_frame_cache_max_bytes(&frame_cache_max_bytes,
                                                      &frame_cache_enabled);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (pchunk->size > (size_t)LONG_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: input chunk size is too large.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    cf_data_length = (CFIndex)pchunk->size;
    data = CFDataCreate(kCFAllocatorDefault,
                        pchunk->buffer,
                        cf_data_length);
    if (! data) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CFDataCreate failed.");
        status = SIXEL_FALSE;
        goto end;
    }

    source = CGImageSourceCreateWithData(data, NULL);
    if (! source) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: CGImageSourceCreateWithData failed.");
        status = SIXEL_FALSE;
        goto end;
    }

    frame_count = CGImageSourceGetCount(source);
    if (! frame_count) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: input has no decodable frames.");
        status = SIXEL_FALSE;
        goto end;
    }

    if (frame_count > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame count is too large.");
        status = SIXEL_BAD_INPUT;
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

    status = coregraphics_png_indexed_metadata_cache_prepare(
        &png_indexed_cache,
        pchunk,
        frame->allocator);
    if (status != SIXEL_OK && status != SIXEL_FALSE) {
        goto end;
    }

    props = CGImageSourceCopyProperties(source, NULL);
    if (props) {
        source_orientation = coregraphics_resolve_exif_orientation(
            props,
            source_orientation);
        /*
         * Treat multi-frame decoding as animation only when the source
         * exposes known animation dictionaries. This keeps multi-size ICO
         * decoding static while enabling APNG/WebP/HEICS animation.
         */
        if (coregraphics_get_animation_keys(props,
                                            frame_count,
                                            &anim_dict,
                                            &anim_loop_key,
                                            NULL,
                                            NULL)) {
            if (frame_count > 1u) {
                is_animation_container = 1;
            }
            anim_loop_count = coregraphics_dictionary_get_int(
                anim_dict,
                anim_loop_key,
                anim_loop_count);
        }
    }
    if (frame_cache_enabled != 0) {
        if (fstatic != 0 || is_animation_container == 0) {
            frame_cache_enabled = 0;
        }
    }
    if (!fstatic && is_animation_container != 0) {
        metadata_slots = (size_t)total_frames;
    } else {
        /*
         * Static and non-animation paths emit one selected frame, so keep
         * metadata caches to a single slot instead of total frame count.
         */
        metadata_slots = 1u;
    }
    if (metadata_slots > 1u &&
        (metadata_slots > SIZE_MAX / sizeof(*frame_props_cache) ||
         metadata_slots > SIZE_MAX / sizeof(*frame_delay_cache) ||
         metadata_slots > SIZE_MAX / sizeof(*frame_orientation_cache) ||
         metadata_slots > SIZE_MAX / sizeof(*frame_props_ready))) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame metadata is too large.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (frame_cache_enabled != 0 &&
        ((size_t)total_frames > SIZE_MAX / sizeof(*frame_cache_decided) ||
         (size_t)total_frames > SIZE_MAX / sizeof(*frame_cache))) {
        sixel_helper_set_additional_message(
            "load_with_coregraphics: frame metadata is too large.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (metadata_slots > 1u) {
        frame_props_cache = (CFDictionaryRef *)sixel_allocator_calloc(
            frame->allocator,
            metadata_slots,
            sizeof(*frame_props_cache));
        if (frame_props_cache == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        frame_delay_cache = (int *)sixel_allocator_calloc(
            frame->allocator,
            metadata_slots,
            sizeof(*frame_delay_cache));
        if (frame_delay_cache == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        frame_orientation_cache = (int *)sixel_allocator_calloc(
            frame->allocator,
            metadata_slots,
            sizeof(*frame_orientation_cache));
        if (frame_orientation_cache == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        frame_props_ready = (unsigned char *)sixel_allocator_calloc(
            frame->allocator,
            metadata_slots,
            sizeof(*frame_props_ready));
        if (frame_props_ready == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        active_frame_props_cache = frame_props_cache;
        active_frame_delay_cache = frame_delay_cache;
        active_frame_orientation_cache = frame_orientation_cache;
        active_frame_props_ready = frame_props_ready;
    } else {
        active_frame_props_cache = &single_frame_props_cache;
        active_frame_delay_cache = &single_frame_delay_cache;
        single_frame_orientation_cache = source_orientation;
        active_frame_orientation_cache = &single_frame_orientation_cache;
        active_frame_props_ready = &single_frame_props_ready;
    }
    if (frame_cache_enabled != 0) {
        frame_cache_decided = (unsigned char *)sixel_allocator_calloc(
            frame->allocator,
            (size_t)total_frames,
            sizeof(*frame_cache_decided));
        if (frame_cache_decided == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        frame_cache = (sixel_frame_t **)sixel_allocator_calloc(
            frame->allocator,
            (size_t)total_frames,
            sizeof(*frame_cache));
        if (frame_cache == NULL) {
            sixel_helper_set_additional_message(
                "load_with_coregraphics: sixel_allocator_calloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
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
            emit_frame = NULL;
            decode_frame = NULL;
            frame_cache_keep = 0;
            frame_cache_decision_pending = 0;
            release_emit_frame = 0;
            cache_hit = 0;
            frame_props = NULL;
            if (frame_cache != NULL &&
                frame_cache[(size_t)frame_index] != NULL) {
                emit_frame = frame_cache[(size_t)frame_index];
                cache_hit = 1;
            }
            if (!fstatic && is_animation_container != 0) {
                frame_meta_slot = (size_t)frame_index;
            } else {
                frame_meta_slot = 0u;
            }

            /*
             * Resolve frame metadata once on first decode of each frame and
             * reuse it on replay loops to avoid repeated property lookups.
             */
            if (active_frame_props_ready[frame_meta_slot] == 0u) {
                active_frame_props_cache[frame_meta_slot] =
                    CGImageSourceCopyPropertiesAtIndex(
                        source,
                        (size_t)frame_index,
                        NULL);
                active_frame_delay_cache[frame_meta_slot] = 0;
                frame_props = active_frame_props_cache[frame_meta_slot];
                if (frame_props != NULL) {
                    frame_anim_dict = NULL;
                    frame_loop_key = NULL;
                    frame_delay_key = NULL;
                    frame_unclamped_delay_key = NULL;
                    if (coregraphics_get_animation_keys(
                            frame_props,
                            frame_count,
                            &frame_anim_dict,
                            &frame_loop_key,
                            &frame_delay_key,
                            &frame_unclamped_delay_key)) {
                        anim_loop_count = coregraphics_dictionary_get_int(
                            frame_anim_dict,
                            frame_loop_key,
                            anim_loop_count);
                        coregraphics_resolve_animation_delay_cs(
                            frame_anim_dict,
                            frame_unclamped_delay_key,
                            frame_delay_key,
                            &active_frame_delay_cache[frame_meta_slot]);
                    }
                }
                frame_orientation = coregraphics_resolve_exif_orientation(
                    frame_props,
                    source_orientation);
                active_frame_orientation_cache[frame_meta_slot] =
                    frame_orientation;
                active_frame_props_ready[frame_meta_slot] = 1u;
            }
            frame_props = active_frame_props_cache[frame_meta_slot];
            frame_orientation = active_frame_orientation_cache[
                frame_meta_slot];
            if (cache_hit == 0) {
                image = CGImageSourceCreateImageAtIndex(
                    source,
                    (size_t)frame_index,
                    NULL);
                if (! image) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: "
                        "CGImageSourceCreateImageAtIndex failed.");
                    status = SIXEL_FALSE;
                    goto end;
                }

                if (frame_cache != NULL) {
                    if (frame_cache_decided[(size_t)frame_index] == 0u &&
                        frame_cache_used_bytes >= frame_cache_max_bytes) {
                        /*
                         * Once cache usage reaches the configured cap, every
                         * remaining frame must bypass cache. Mark it decided
                         * early so later loops skip temporary-frame probes.
                         */
                        frame_cache_decided[(size_t)frame_index] = 1u;
                    }
                    if (frame_cache_decided[(size_t)frame_index] == 0u) {
                        status = sixel_frame_new(&cached_frame_tmp,
                                                 pchunk->allocator);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        decode_frame = cached_frame_tmp;
                        frame_cache_decision_pending = 1;
                    } else {
                        decode_frame = frame;
                    }
                } else {
                    decode_frame = frame;
                }

                image_width = CGImageGetWidth(image);
                image_height = CGImageGetHeight(image);
                if (image_width > (size_t)INT_MAX) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: given width parameter is too"
                        " huge.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (image_height > (size_t)INT_MAX) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: given height parameter is too"
                        " huge.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                decode_frame->width = (int)image_width;
                decode_frame->height = (int)image_height;

                if (image_width > (size_t)SIXEL_WIDTH_LIMIT) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: given width parameter is too"
                        " huge.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (image_height > (size_t)SIXEL_HEIGHT_LIMIT) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: given height parameter is too"
                        " huge.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (decode_frame->width <= 0) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: an invalid width parameter"
                        " detected.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (decode_frame->height <= 0) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: an invalid height parameter"
                        " detected.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if ((size_t)decode_frame->width >
                    SIZE_MAX / (size_t)decode_frame->height) {
                    sixel_helper_set_additional_message(
                        "load_with_coregraphics: too large image.");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }

                coregraphics_reset_frame_storage(decode_frame);
                status = coregraphics_try_handle_indexed_frame(
                    pchunk,
                    decode_frame,
                    image,
                    frame_props,
                    &png_indexed_cache,
                    fuse_palette,
                    reqcolors,
                    bgcolor,
                    &indexed_handled,
                    &force_alpha_from_indexed);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                if (indexed_handled == 0) {
                    status = coregraphics_try_handle_png_indexed_keycolor(
                        &png_indexed_cache,
                        decode_frame,
                        image,
                        fuse_palette,
                        reqcolors,
                        bgcolor,
                        &indexed_handled,
                        &force_alpha_from_indexed);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }

                if (indexed_handled == 0) {
                    has_alpha_like = force_alpha_from_indexed != 0 ||
                        coregraphics_image_has_alpha(image, frame_props);
                    promote_float32 = coregraphics_should_promote_float32(
                        image,
                        frame_props);
                    if (promote_float32 != 0) {
                        status = coregraphics_decode_float32_frame(
                            decode_frame,
                            image,
                            bgcolor,
                            has_alpha_like);
                    } else {
                        status = coregraphics_decode_rgba8_frame(
                            decode_frame,
                            image,
                            bgcolor,
                            has_alpha_like,
                            &rgba8_lut_cache);
                    }
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }
                if (enable_orientation != 0 &&
                    frame_orientation >= 2 &&
                    frame_orientation <= 8) {
                    status = loader_frame_apply_orientation(decode_frame,
                                                           frame_orientation);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }

                if (frame_cache != NULL) {
                    if (frame_cache_decision_pending != 0) {
                        frame_cache_frame_bytes = 0u;
                        if (!coregraphics_measure_frame_cache_bytes(
                                decode_frame,
                                &frame_cache_frame_bytes)) {
                            sixel_helper_set_additional_message(
                                "load_with_coregraphics: failed to estimate "
                                "decoded frame size.");
                            status = SIXEL_BAD_INTEGER_OVERFLOW;
                            goto end;
                        }
                        if (frame_cache_frame_bytes <= frame_cache_max_bytes &&
                            frame_cache_used_bytes <=
                            frame_cache_max_bytes - frame_cache_frame_bytes) {
                            frame_cache_keep = 1;
                        }
                        frame_cache_decided[(size_t)frame_index] = 1u;
                        if (frame_cache_keep != 0) {
                            decode_frame->handoff_shareable = 1;
                            frame_cache[(size_t)frame_index] = decode_frame;
                            frame_cache_used_bytes += frame_cache_frame_bytes;
                            emit_frame = decode_frame;
                            cached_frame_tmp = NULL;
                        } else {
                            decode_frame->handoff_shareable = 0;
                            emit_frame = decode_frame;
                            release_emit_frame = 1;
                        }
                    } else {
                        decode_frame->handoff_shareable = 0;
                        emit_frame = decode_frame;
                    }
                } else {
                    emit_frame = decode_frame;
                }
                CGImageRelease(image);
                image = NULL;
            }
            if (emit_frame == NULL) {
                emit_frame = frame_cache[(size_t)frame_index];
            }
            emit_frame->frame_no = frames_in_loop;
            emit_frame->loop_count = loop_no;
            emit_frame->delay = active_frame_delay_cache[frame_meta_slot];
            emit_frame->multiframe = (!fstatic && frame_count > 1
                                      && is_animation_container);
            status = fn_load(emit_frame, context);
            if (status != SIXEL_OK) {
                goto end;
            }

            if (sixel_loader_callback_is_canceled(context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }

            ++frame_index;
            ++frames_in_loop;

            if (release_emit_frame != 0) {
                sixel_frame_unref(emit_frame);
                emit_frame = NULL;
                cached_frame_tmp = NULL;
            }

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
    if (cached_frame_tmp != NULL) {
        sixel_frame_unref(cached_frame_tmp);
    }
    if (active_frame_props_cache != NULL) {
        for (prefetch_index = 0u;
             prefetch_index < metadata_slots;
             ++prefetch_index) {
            if (active_frame_props_cache[prefetch_index] != NULL) {
                CFRelease(active_frame_props_cache[prefetch_index]);
            }
        }
    }
    if (image != NULL) {
        CGImageRelease(image);
    }
    if (source != NULL) {
        CFRelease(source);
    }
    if (props != NULL) {
        CFRelease(props);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (frame != NULL) {
        if (frame_cache != NULL) {
            for (prefetch_index = 0u;
                 prefetch_index < (size_t)total_frames;
                 ++prefetch_index) {
                if (frame_cache[prefetch_index] != NULL) {
                    sixel_frame_unref(frame_cache[prefetch_index]);
                }
            }
        }
        coregraphics_png_indexed_metadata_cache_reset(
            &png_indexed_cache,
            frame->allocator);
        sixel_allocator_free(frame->allocator, frame_cache);
        sixel_allocator_free(frame->allocator, frame_cache_decided);
        sixel_allocator_free(frame->allocator, frame_props_ready);
        sixel_allocator_free(frame->allocator, frame_orientation_cache);
        sixel_allocator_free(frame->allocator, frame_delay_cache);
        sixel_allocator_free(frame->allocator, frame_props_cache);
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
                                  self->enable_orientation,
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
