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
 */

/* Builtin loader covering SIXEL, PNM, GIF, and stb_image fallbacks.  This
 * module keeps the heavyweight stb_image implementation isolated from the
 * registry so other backends avoid pulling in its macros and includes.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "compat_stub.h"
#include "frame.h"
#include "fromgif.h"
#include "frompng.h"
#include "frompnm.h"
#include "pixelformat.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader.h"
#include "sixel_atomic.h"

#if defined(_MSC_VER)
# define SIXEL_STBI_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__STDC_NO_THREADS__)
# define SIXEL_STBI_TLS _Thread_local
#elif defined(__GNUC__)
# define SIXEL_STBI_TLS __thread
#else
# define SIXEL_STBI_TLS
#endif

static SIXEL_STBI_TLS sixel_allocator_t *stbi_allocator;

#undef SIXEL_STBI_TLS

#define SIXEL_BUILTIN_PNG_COLOR_TYPE_GRAY 0
#define SIXEL_BUILTIN_PNG_COLOR_TYPE_RGB 2
#define SIXEL_BUILTIN_PNG_COLOR_MASK_ALPHA 4

typedef struct sixel_loader_builtin_component {
    sixel_loader_component_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int loop_control;
    int start_frame_no_set;
    int start_frame_no;
    int enable_cms;
} sixel_loader_builtin_component_t;

void *
stbi_malloc(size_t n)
{
    if (stbi_allocator == NULL) {
        return malloc(n);
    }
    return sixel_allocator_malloc(stbi_allocator, n);
}

void *
stbi_realloc(void *p, size_t n)
{
    if (stbi_allocator == NULL) {
        return realloc(p, n);
    }
    return sixel_allocator_realloc(stbi_allocator, p, n);
}

void
stbi_free(void *p)
{
    if (stbi_allocator == NULL) {
        free(p);
        return;
    }
    sixel_allocator_free(stbi_allocator, p);
}

#define STBI_MALLOC stbi_malloc
#define STBI_REALLOC stbi_realloc
#define STBI_FREE stbi_free

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_FAILURE_USERMSG 1
#if defined(_WIN32)
# define STBI_NO_THREAD_LOCALS 1  /* no tls */
#endif
#define STBI_NO_GIF
#define STBI_NO_PNM
#if HAVE_NEON && HAVE_ARM_NEON_H
# define STBI_NEON 1
# define STBI_NO_SIMD 1
#elif !defined(HAVE_SSE2)
# define STBI_NO_SIMD 1
#elif !defined(HAVE_EMMINTRIN_H)
# define STBI_NO_SIMD 1
#endif

#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
# endif
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstrict-overflow"
# endif
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wswitch-default"
# endif
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
# endif
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
# endif
#endif
#include "stb_image.h"
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif

static SIXELSTATUS
load_sixel(unsigned char        /* out */ **result,
           unsigned char        /* in */  *buffer,
           int                  /* in */  size,
           int                  /* out */ *psx,
           int                  /* out */ *psy,
           unsigned char        /* out */ **ppalette,
           int                  /* out */ *pncolors,
           int                  /* in */  reqcolors,
           int                  /* out */ *ppixelformat,
           sixel_allocator_t    /* in */  *allocator)
{
    SIXELSTATUS status;
    unsigned char *decoded_pixels;
    unsigned char *decoded_palette;
    size_t image_bytes;

    if (result == NULL || pncolors == NULL || ppixelformat == NULL ||
            allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_FALSE;
    decoded_pixels = NULL;
    decoded_palette = NULL;
    image_bytes = 0;

    status = sixel_decode_raw(buffer,
                              size,
                              &decoded_pixels,
                              psx,
                              psy,
                              &decoded_palette,
                              pncolors,
                              allocator);
    if (loader_trace_is_enabled()) {
        loader_trace_message("load_sixel: sixel_decode_raw -> %d",
                             (int)status);
    }

    image_bytes = (size_t)(*psx) * (size_t)(*psy);

    if (ppalette == NULL ||
            (reqcolors > 0 && *pncolors > reqcolors)) {
        size_t rgb_bytes;
        size_t index;

        if (decoded_palette == NULL) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        rgb_bytes = image_bytes * 3u;
        *result = sixel_allocator_malloc(allocator, rgb_bytes);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_sixel: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0; index < image_bytes; ++index) {
            size_t palette_index;
            size_t pixel_offset;

            palette_index = decoded_pixels[index] * 3u;
            pixel_offset = index * 3u;
            (*result)[pixel_offset + 0] = decoded_palette[palette_index + 0];
            (*result)[pixel_offset + 1] = decoded_palette[palette_index + 1];
            (*result)[pixel_offset + 2] = decoded_palette[palette_index + 2];
        }
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    } else {
        *result = decoded_pixels;
        decoded_pixels = NULL;
        *ppalette = decoded_palette;
        decoded_palette = NULL;
        *ppixelformat = SIXEL_PIXELFORMAT_PAL8;
    }
    status = SIXEL_OK;

end:
    if (decoded_pixels != NULL) {
        sixel_allocator_free(allocator, decoded_pixels);
        decoded_pixels = NULL;
    }
    if (decoded_palette != NULL) {
        sixel_allocator_free(allocator, decoded_palette);
        decoded_palette = NULL;
    }

    return status;
}

static int
sixel_builtin_extract_jpeg_icc(unsigned char const *buffer,
                               size_t size,
                               unsigned char **profile,
                               size_t *profile_length,
                               sixel_allocator_t *allocator)
{
    unsigned char const *p;
    unsigned int seq_count;
    unsigned char **chunks;
    size_t *chunk_sizes;
    unsigned int index;
    unsigned int max_index;
    size_t total_size;
    unsigned char *assembled;
    size_t offset;
    unsigned int marker;
    unsigned int segment_length;
    size_t payload_size;
    unsigned int seq_no;

    p = NULL;
    seq_count = 0u;
    chunks = NULL;
    chunk_sizes = NULL;
    index = 0u;
    max_index = 0u;
    total_size = 0u;
    assembled = NULL;
    offset = 0u;
    marker = 0u;
    segment_length = 0u;
    payload_size = 0u;
    seq_no = 0u;

    *profile = NULL;
    *profile_length = 0u;

    if (buffer == NULL || size < 4u || allocator == NULL) {
        return 0;
    }
    if (buffer[0] != 0xffu || buffer[1] != 0xd8u) {
        return 0;
    }

    p = buffer + 2u;
    while ((size_t)(p - buffer) + 4u <= size) {
        if (p[0] != 0xffu) {
            goto cleanup;
        }
        while ((size_t)(p - buffer) < size && *p == 0xffu) {
            ++p;
        }
        if ((size_t)(p - buffer) >= size) {
            break;
        }

        marker = (unsigned int)*p;
        ++p;
        if (marker == 0xd9u || marker == 0xdau) {
            break;
        }
        if (marker == 0x01u || (marker >= 0xd0u && marker <= 0xd7u)) {
            continue;
        }
        if ((size_t)(p - buffer) + 2u > size) {
            goto cleanup;
        }

        segment_length = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
        p += 2u;
        if (segment_length < 2u) {
            goto cleanup;
        }
        if ((size_t)segment_length - 2u > size - (size_t)(p - buffer)) {
            goto cleanup;
        }

        if (marker != 0xe2u || segment_length < 16u ||
            memcmp(p, "ICC_PROFILE\0", 12u) != 0) {
            p += (size_t)segment_length - 2u;
            continue;
        }

        seq_no = (unsigned int)p[12];
        if (seq_no == 0u) {
            goto cleanup;
        }
        if (seq_count == 0u) {
            seq_count = (unsigned int)p[13];
            if (seq_count == 0u) {
                goto cleanup;
            }
            chunks = (unsigned char **)sixel_allocator_calloc(
                allocator,
                seq_count,
                sizeof(*chunks));
            chunk_sizes = (size_t *)sixel_allocator_calloc(
                allocator,
                seq_count,
                sizeof(*chunk_sizes));
            if (chunks == NULL || chunk_sizes == NULL) {
                goto cleanup;
            }
        }
        if ((unsigned int)p[13] != seq_count || seq_no > seq_count) {
            goto cleanup;
        }

        index = seq_no - 1u;
        if (chunks[index] != NULL) {
            goto cleanup;
        }
        payload_size = (size_t)segment_length - 16u;
        chunks[index] = (unsigned char *)sixel_allocator_malloc(
            allocator,
            payload_size);
        if (chunks[index] == NULL) {
            goto cleanup;
        }
        memcpy(chunks[index], p + 14u, payload_size);
        chunk_sizes[index] = payload_size;
        if (index > max_index) {
            max_index = index;
        }

        p += (size_t)segment_length - 2u;
    }

    if (seq_count == 0u) {
        goto cleanup;
    }
    if (max_index + 1u != seq_count) {
        goto cleanup;
    }

    for (index = 0u; index < seq_count; ++index) {
        if (chunks[index] == NULL ||
            total_size > SIZE_MAX - chunk_sizes[index]) {
            goto cleanup;
        }
        total_size += chunk_sizes[index];
    }
    if (total_size == 0u) {
        goto cleanup;
    }

    assembled = (unsigned char *)sixel_allocator_malloc(allocator, total_size);
    if (assembled == NULL) {
        goto cleanup;
    }
    for (index = 0u; index < seq_count; ++index) {
        memcpy(assembled + offset, chunks[index], chunk_sizes[index]);
        offset += chunk_sizes[index];
    }

    *profile = assembled;
    *profile_length = total_size;
    assembled = NULL;

cleanup:
    if (chunks != NULL) {
        for (index = 0u; index < seq_count; ++index) {
            sixel_allocator_free(allocator, chunks[index]);
        }
    }
    sixel_allocator_free(allocator, chunks);
    sixel_allocator_free(allocator, chunk_sizes);
    sixel_allocator_free(allocator, assembled);

    return *profile != NULL;
}

typedef enum sixel_builtin_icc_extract_status {
    SIXEL_BUILTIN_ICC_EXTRACT_ABSENT = 0,
    SIXEL_BUILTIN_ICC_EXTRACT_FOUND = 1,
    SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED = -1
} sixel_builtin_icc_extract_status_t;

static int
sixel_builtin_extract_psd_icc(unsigned char const *buffer,
                              size_t size,
                              unsigned char const **profile,
                              size_t *profile_length)
{
    size_t offset;
    size_t section_length;
    size_t resource_end;
    size_t name_length;
    size_t data_length;
    unsigned int resource_id;

    offset = 0u;
    section_length = 0u;
    resource_end = 0u;
    name_length = 0u;
    data_length = 0u;
    resource_id = 0u;

    if (buffer == NULL || profile == NULL || profile_length == NULL ||
        size < 34u) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }
    *profile = NULL;
    *profile_length = 0u;
    if (memcmp(buffer, "8BPS", 4u) != 0) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }

    /* color mode data section */
    offset = 26u;
    section_length = ((size_t)buffer[offset + 0u] << 24) |
                     ((size_t)buffer[offset + 1u] << 16) |
                     ((size_t)buffer[offset + 2u] << 8) |
                     (size_t)buffer[offset + 3u];
    offset += 4u;
    if (section_length > size - offset) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }
    offset += section_length;

    /* image resources section */
    if (offset + 4u > size) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }
    section_length = ((size_t)buffer[offset + 0u] << 24) |
                     ((size_t)buffer[offset + 1u] << 16) |
                     ((size_t)buffer[offset + 2u] << 8) |
                     (size_t)buffer[offset + 3u];
    offset += 4u;
    if (section_length > size - offset) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }

    resource_end = offset + section_length;
    while (offset + 12u <= resource_end) {
        if (memcmp(buffer + offset, "8BIM", 4u) != 0) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        offset += 4u;

        resource_id = ((unsigned int)buffer[offset + 0u] << 8) |
                      (unsigned int)buffer[offset + 1u];
        offset += 2u;

        name_length = (size_t)buffer[offset];
        ++offset;
        if (name_length > resource_end - offset) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        offset += name_length;
        if (((1u + name_length) & 1u) != 0u) {
            if (offset >= resource_end) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            ++offset;
        }

        if (offset + 4u > resource_end) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        data_length = ((size_t)buffer[offset + 0u] << 24) |
                      ((size_t)buffer[offset + 1u] << 16) |
                      ((size_t)buffer[offset + 2u] << 8) |
                      (size_t)buffer[offset + 3u];
        offset += 4u;
        if (data_length > resource_end - offset) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }

        if (resource_id == 0x040fu) {
            if (data_length == 0u) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            *profile = buffer + offset;
            *profile_length = data_length;
            return SIXEL_BUILTIN_ICC_EXTRACT_FOUND;
        }

        offset += data_length;
        if ((data_length & 1u) != 0u) {
            if (offset >= resource_end) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            ++offset;
        }
    }

    return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
}

#if HAVE_LCMS2
static uint16_t
sixel_builtin_tiff_read_u16(unsigned char const *p, int little_endian)
{
    if (little_endian) {
        return (uint16_t)((uint16_t)p[0] |
                          ((uint16_t)p[1] << 8));
    }
    return (uint16_t)(((uint16_t)p[0] << 8) |
                      (uint16_t)p[1]);
}

static uint32_t
sixel_builtin_tiff_read_u32(unsigned char const *p, int little_endian)
{
    if (little_endian) {
        return (uint32_t)p[0] |
               ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static int
sixel_builtin_extract_tiff_icc(unsigned char const *buffer,
                               size_t size,
                               unsigned char **profile,
                               size_t *profile_length,
                               uint16_t *photometric,
                               sixel_allocator_t *allocator)
{
    enum { TIFFTAG_PHOTOMETRIC = 262 };
    enum { TIFFTAG_ICCPROFILE = 34675 };
    enum { TIFF_TYPE_BYTE = 1, TIFF_TYPE_SHORT = 3, TIFF_TYPE_UNDEFINED = 7 };
    int little_endian;
    uint16_t version;
    size_t ifd_offset;
    uint16_t entry_count;
    size_t entries_offset;
    size_t entries_size;
    uint16_t i;
    size_t entry_offset;
    uint16_t tag;
    uint16_t type;
    uint32_t count32;
    size_t count;
    uint32_t value_offset32;
    size_t value_offset;
    unsigned char *copied;

    little_endian = 0;
    version = 0u;
    ifd_offset = 0u;
    entry_count = 0u;
    entries_offset = 0u;
    entries_size = 0u;
    i = 0u;
    entry_offset = 0u;
    tag = 0u;
    type = 0u;
    count32 = 0u;
    count = 0u;
    value_offset32 = 0u;
    value_offset = 0u;
    copied = NULL;

    *profile = NULL;
    *profile_length = 0u;
    if (photometric != NULL) {
        *photometric = (uint16_t)0xffffu;
    }

    if (buffer == NULL || profile == NULL || profile_length == NULL ||
        allocator == NULL || size < 8u) {
        return 0;
    }

    if (buffer[0] == 'I' && buffer[1] == 'I') {
        little_endian = 1;
    } else if (buffer[0] == 'M' && buffer[1] == 'M') {
        little_endian = 0;
    } else {
        return 0;
    }

    version = sixel_builtin_tiff_read_u16(buffer + 2u, little_endian);
    if (version != 42u) {
        /* BigTIFF (43) and other variants are ignored in builtin path. */
        return 0;
    }

    ifd_offset = (size_t)sixel_builtin_tiff_read_u32(buffer + 4u,
                                                     little_endian);
    if (ifd_offset == 0u || ifd_offset > size - 2u) {
        return 0;
    }

    entry_count = sixel_builtin_tiff_read_u16(buffer + ifd_offset,
                                              little_endian);
    entries_offset = ifd_offset + 2u;
    entries_size = (size_t)entry_count * 12u;
    if (entries_size > size - entries_offset) {
        return 0;
    }

    for (i = 0u; i < entry_count; ++i) {
        entry_offset = entries_offset + (size_t)i * 12u;
        tag = sixel_builtin_tiff_read_u16(buffer + entry_offset,
                                          little_endian);
        if (tag == TIFFTAG_PHOTOMETRIC) {
            type = sixel_builtin_tiff_read_u16(buffer + entry_offset + 2u,
                                               little_endian);
            count32 = sixel_builtin_tiff_read_u32(buffer + entry_offset + 4u,
                                                  little_endian);
            if (photometric != NULL &&
                type == TIFF_TYPE_SHORT && count32 == 1u) {
                *photometric = sixel_builtin_tiff_read_u16(buffer + entry_offset + 8u,
                                                           little_endian);
            }
            continue;
        }
        if (tag != TIFFTAG_ICCPROFILE) {
            continue;
        }
        type = sixel_builtin_tiff_read_u16(buffer + entry_offset + 2u,
                                           little_endian);
        if (type != TIFF_TYPE_BYTE && type != TIFF_TYPE_UNDEFINED) {
            return 0;
        }

        count32 = sixel_builtin_tiff_read_u32(buffer + entry_offset + 4u,
                                              little_endian);
        if (count32 == 0u) {
            return 0;
        }
        count = (size_t)count32;

        copied = (unsigned char *)sixel_allocator_malloc(allocator, count);
        if (copied == NULL) {
            return 0;
        }

        if (count <= 4u) {
            memcpy(copied, buffer + entry_offset + 8u, count);
        } else {
            value_offset32 = sixel_builtin_tiff_read_u32(buffer + entry_offset + 8u,
                                                         little_endian);
            value_offset = (size_t)value_offset32;
            if (value_offset > size || count > size - value_offset) {
                sixel_allocator_free(allocator, copied);
                return 0;
            }
            memcpy(copied, buffer + value_offset, count);
        }

        *profile = copied;
        *profile_length = count;
        return 1;
    }

    return 0;
}

static int
sixel_builtin_tiff_photometric_supports_icc(uint16_t photometric)
{
    switch (photometric) {
    case 0u: /* WhiteIsZero (Gray) */
    case 1u: /* BlackIsZero (Gray) */
    case 2u: /* RGB */
    case 3u: /* Palette */
    case 6u: /* YCbCr */
    case 0xffffu: /* unknown/missing tag */
        return 1;
    default:
        return 0;
    }
}
#endif

static int
chunk_is_psd(sixel_chunk_t const *chunk)
{
    if (chunk->size < 4u) {
        return 0;
    }
    if (memcmp(chunk->buffer, "8BPS", 4u) == 0) {
        return 1;
    }
    return 0;
}

typedef struct sixel_builtin_psd_info {
    unsigned int version;
    unsigned int channels;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int color_mode;
    unsigned int compression;
    size_t color_mode_data_offset;
    size_t color_mode_data_length;
    size_t image_resources_offset;
    size_t image_resources_length;
    size_t layer_mask_offset;
    size_t layer_mask_length;
    size_t image_data_offset;
} sixel_builtin_psd_info_t;

static unsigned int
sixel_builtin_read_u16be(unsigned char const *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static size_t
sixel_builtin_read_u32be_size(unsigned char const *p)
{
    return ((size_t)p[0] << 24) |
           ((size_t)p[1] << 16) |
           ((size_t)p[2] << 8) |
           (size_t)p[3];
}

static int
sixel_builtin_parse_psd_info(sixel_chunk_t const *chunk,
                             sixel_builtin_psd_info_t *info)
{
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    size_t section_length;

    buffer = NULL;
    size = 0u;
    offset = 0u;
    section_length = 0u;
    if (chunk == NULL || chunk->buffer == NULL || info == NULL ||
        chunk->size < 30u) {
        return 0;
    }
    buffer = chunk->buffer;
    size = chunk->size;
    if (memcmp(buffer, "8BPS", 4u) != 0) {
        return 0;
    }

    memset(info, 0, sizeof(*info));
    info->version = sixel_builtin_read_u16be(buffer + 4u);
    info->channels = sixel_builtin_read_u16be(buffer + 12u);
    info->height = (unsigned int)sixel_builtin_read_u32be_size(buffer + 14u);
    info->width = (unsigned int)sixel_builtin_read_u32be_size(buffer + 18u);
    info->depth = sixel_builtin_read_u16be(buffer + 22u);
    info->color_mode = sixel_builtin_read_u16be(buffer + 24u);

    offset = 26u;
    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->color_mode_data_offset = offset;
    info->color_mode_data_length = section_length;
    offset += section_length;

    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->image_resources_offset = offset;
    info->image_resources_length = section_length;
    offset += section_length;

    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->layer_mask_offset = offset;
    info->layer_mask_length = section_length;
    offset += section_length;

    if (offset + 2u > size) {
        return 0;
    }
    info->compression = sixel_builtin_read_u16be(buffer + offset);
    info->image_data_offset = offset + 2u;

    return 1;
}

static int
chunk_is_sixel(sixel_chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    p = chunk->buffer;
    end = p + chunk->size;

    if (chunk->size < 3) {
        return 0;
    }

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                continue;
            }
        }
    }
    return 0;
}

static int
chunk_is_pnm(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '6') {
        return 1;
    }
    return 0;
}

static SIXELSTATUS
sixel_builtin_promote_rgb16_to_float32(unsigned char **ppixels,
                                       int width,
                                       int height,
                                       sixel_allocator_t *allocator)
{
    size_t pixel_count;
    size_t sample_count;
    size_t i;
    uint16_t *samples16;
    float *samples32;

    pixel_count = 0u;
    sample_count = 0u;
    i = 0u;
    samples16 = NULL;
    samples32 = NULL;

    if (ppixels == NULL || *ppixels == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    sample_count = pixel_count * 3u;
    if (sample_count > SIZE_MAX / sizeof(float)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    samples16 = (uint16_t *)*ppixels;
    samples32 = (float *)sixel_allocator_malloc(allocator,
                                                sample_count * sizeof(float));
    if (samples32 == NULL) {
        sixel_helper_set_additional_message(
            "sixel_builtin_promote_rgb16_to_float32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < sample_count; ++i) {
        samples32[i] = (float)((double)samples16[i] / 65535.0);
    }

    sixel_allocator_free(allocator, *ppixels);
    *ppixels = (unsigned char *)samples32;

    return SIXEL_OK;
}

static int
sixel_builtin_psd_unpack_packbits_row(unsigned char const *src,
                                      size_t src_length,
                                      unsigned char *dst,
                                      size_t dst_length)
{
    size_t src_offset;
    size_t dst_offset;
    unsigned int control;
    size_t run;
    unsigned char value;

    src_offset = 0u;
    dst_offset = 0u;
    control = 0u;
    run = 0u;
    value = 0u;
    if (src == NULL || dst == NULL) {
        return 0;
    }

    while (dst_offset < dst_length) {
        if (src_offset >= src_length) {
            return 0;
        }
        control = src[src_offset++];
        if (control <= 127u) {
            run = (size_t)control + 1u;
            if (run > dst_length - dst_offset ||
                run > src_length - src_offset) {
                return 0;
            }
            memcpy(dst + dst_offset, src + src_offset, run);
            src_offset += run;
            dst_offset += run;
        } else if (control == 128u) {
            /* no-op */
        } else {
            run = 257u - (size_t)control;
            if (run > dst_length - dst_offset || src_offset >= src_length) {
                return 0;
            }
            value = src[src_offset++];
            memset(dst + dst_offset, (int)value, run);
            dst_offset += run;
        }
    }

    if (src_offset != src_length) {
        return 0;
    }
    return 1;
}

static int
sixel_builtin_decode_psd_8bit_channel(sixel_chunk_t const *chunk,
                                      sixel_builtin_psd_info_t const *info,
                                      unsigned int target_channel,
                                      unsigned char *dst)
{
    size_t pixel_count;
    size_t plane_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t table_entries;
    size_t table_bytes;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;

    pixel_count = 0u;
    plane_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels) {
        return 0;
    }

    if ((size_t)info->width > STBI_MAX_DIMENSIONS ||
        (size_t)info->height > STBI_MAX_DIMENSIONS) {
        return 0;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    plane_bytes = pixel_count;

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            return 0;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        memcpy(dst, chunk->buffer + offset, plane_bytes);
        return 1;
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            return 0;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            return 0;
        }
        table_bytes = table_entries * 2u;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height + (size_t)row) * 2u;
                row_length = (size_t)sixel_builtin_read_u16be(row_table + row_index);
                if (data_cursor > chunk->size || row_length > chunk->size - data_cursor) {
                    return 0;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * (size_t)info->width;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            dst + row_offset,
                            (size_t)info->width)) {
                        return 0;
                    }
                }
                data_cursor += row_length;
            }
        }
        return 1;
    }
    return 0;
}

static int
sixel_builtin_decode_psd_bitmap_channel(sixel_chunk_t const *chunk,
                                        sixel_builtin_psd_info_t const *info,
                                        unsigned int target_channel,
                                        unsigned char *dst,
                                        size_t row_bytes)
{
    size_t plane_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t table_entries;
    size_t table_bytes;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;

    plane_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels || row_bytes == 0u) {
        return 0;
    }
    if (row_bytes > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            return 0;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        memcpy(dst, chunk->buffer + offset, plane_bytes);
        return 1;
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            return 0;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            return 0;
        }
        table_bytes = table_entries * 2u;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height + (size_t)row) * 2u;
                row_length = (size_t)sixel_builtin_read_u16be(row_table + row_index);
                if (data_cursor > chunk->size || row_length > chunk->size - data_cursor) {
                    return 0;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * row_bytes;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            dst + row_offset,
                            row_bytes)) {
                        return 0;
                    }
                }
                data_cursor += row_length;
            }
        }
        return 1;
    }
    return 0;
}

static int
sixel_builtin_decode_psd_8bit_planes(sixel_chunk_t const *chunk,
                                     sixel_builtin_psd_info_t const *info,
                                     int want_alpha,
                                     unsigned char **pplane0,
                                     unsigned char **pplane_alpha)
{
    unsigned char *plane0;
    unsigned char *plane_alpha;
    size_t pixel_count;
    size_t plane_bytes;

    plane0 = NULL;
    plane_alpha = NULL;
    pixel_count = 0u;
    plane_bytes = 0u;
    if (chunk == NULL || info == NULL || pplane0 == NULL || pplane_alpha == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u) {
        return 0;
    }

    *pplane0 = NULL;
    *pplane_alpha = NULL;

    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    plane_bytes = pixel_count;

    plane0 = (unsigned char *)sixel_allocator_malloc(chunk->allocator, plane_bytes);
    if (plane0 == NULL) {
        return 0;
    }
    if (want_alpha && info->channels >= 2u) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              plane_bytes);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            return 0;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane0)) {
        goto fail;
    }
    if (plane_alpha != NULL &&
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_alpha)) {
        goto fail;
    }

    *pplane0 = plane0;
    *pplane_alpha = plane_alpha;
    return 1;

fail:
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);
    return 0;
}

static SIXELSTATUS
sixel_builtin_decode_psd_bitmap_1bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane0;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    size_t row_bytes;
    size_t plane_bytes;
    size_t pixel_count;
    size_t y;
    size_t x;
    size_t row_offset;
    size_t pixel_offset;
    int want_alpha;
    int bit;
    int alpha;
    int r;
    int g;
    int b;

    plane0 = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    row_bytes = 0u;
    plane_bytes = 0u;
    pixel_count = 0u;
    y = 0u;
    x = 0u;
    row_offset = 0u;
    pixel_offset = 0u;
    want_alpha = 0;
    bit = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode != 0u || info->depth != 1u ||
        info->compression > 1u || info->channels < 1u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)info->width > SIZE_MAX - 7u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    row_bytes = ((size_t)info->width + 7u) / 8u;
    if (row_bytes == 0u || row_bytes > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    plane0 = (unsigned char *)sixel_allocator_malloc(chunk->allocator, plane_bytes);
    if (plane0 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    want_alpha = (bgcolor != NULL && info->channels >= 2u) ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              plane_bytes);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_bitmap_channel(chunk,
                                                 info,
                                                 0u,
                                                 plane0,
                                                 row_bytes) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_bitmap_channel(chunk,
                                                  info,
                                                  1u,
                                                  plane_alpha,
                                                  row_bytes))) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed raw/RLE channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < (size_t)info->height; ++y) {
        row_offset = y * row_bytes;
        for (x = 0u; x < (size_t)info->width; ++x) {
            pixel_offset = y * (size_t)info->width + x;
            bit = (plane0[row_offset + (x >> 3u)] &
                   (unsigned char)(1u << (7u - (x & 7u)))) ? 1 : 0;
            /* PSD Bitmap mode stores 0=white, 1=black. */
            r = bit ? 0 : 0xff;
            g = r;
            b = r;
            if (plane_alpha != NULL) {
                alpha = (plane_alpha[row_offset + (x >> 3u)] &
                         (unsigned char)(1u << (7u - (x & 7u)))) ? 0xff : 0;
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            }
            rgb[pixel_offset * 3u + 0u] = (unsigned char)r;
            rgb[pixel_offset * 3u + 1u] = (unsigned char)g;
            rgb[pixel_offset * 3u + 2u] = (unsigned char)b;
        }
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = rgb;
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_gray_or_indexed_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char const *palette_data;
    unsigned char *plane0;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    unsigned char idx;
    unsigned char gray;
    unsigned char alpha;
    int r;
    int g;
    int b;

    palette_data = NULL;
    plane0 = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    idx = 0u;
    gray = 0u;
    alpha = 0u;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((info->color_mode != 1u &&
         info->color_mode != 2u &&
         info->color_mode != 8u) ||
        info->depth != 8u || info->compression > 1u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (info->color_mode == 2u && info->color_mode_data_length < 768u) {
        sixel_helper_set_additional_message(
            "builtin PSD: indexed palette data is too short");
        return SIXEL_STBI_ERROR;
    }

    want_alpha = (bgcolor != NULL && info->channels >= 2u) ? 1 : 0;
    if (!sixel_builtin_decode_psd_8bit_planes(chunk,
                                              info,
                                              want_alpha,
                                              &plane0,
                                              &plane_alpha)) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed raw/RLE channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (info->color_mode == 2u) {
        palette_data = chunk->buffer + info->color_mode_data_offset;
    }

    for (i = 0u; i < pixel_count; ++i) {
        if (info->color_mode == 1u || info->color_mode == 8u) {
            gray = plane0[i];
            r = (int)gray;
            g = (int)gray;
            b = (int)gray;
        } else {
            idx = plane0[i];
            r = (int)palette_data[idx];
            g = (int)palette_data[256u + idx];
            b = (int)palette_data[512u + idx];
        }
        if (plane_alpha != NULL) {
            alpha = plane_alpha[i];
            r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
            g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
            b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = rgb;
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_cmyk_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane_c;
    unsigned char *plane_m;
    unsigned char *plane_y;
    unsigned char *plane_k;
    unsigned char *plane_alpha;
    unsigned char *cmyk;
    unsigned char *rgb;
    float *rgbf32;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int use_cms;
    int cms_applied;
    int c;
    int m;
    int y;
    int k;
    int alpha;
    int r;
    int g;
    int b;

    plane_c = NULL;
    plane_m = NULL;
    plane_y = NULL;
    plane_k = NULL;
    plane_alpha = NULL;
    cmyk = NULL;
    rgb = NULL;
    rgbf32 = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    use_cms = 0;
    cms_applied = 0;
    c = 0;
    m = 0;
    y = 0;
    k = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || pcms_applied == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    if (info->color_mode != 4u || info->depth != 8u ||
        info->compression > 1u || info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    plane_c = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_m = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_y = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_k = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        return SIXEL_BAD_ALLOCATION;
    }

    want_alpha = (bgcolor != NULL && info->channels >= 5u) ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_c) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_m) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_y) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_k) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 4u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed raw/RLE channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (use_cms) {
        if (pixel_count > SIZE_MAX / 4u) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        cmyk = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 4u);
        rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
        if (cmyk == NULL || rgbf32 == NULL) {
            sixel_allocator_free(chunk->allocator, rgbf32);
            sixel_allocator_free(chunk->allocator, cmyk);
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }

        for (i = 0u; i < pixel_count; ++i) {
            cmyk[i * 4u + 0u] = plane_c[i];
            cmyk[i * 4u + 1u] = plane_m[i];
            cmyk[i * 4u + 2u] = plane_y[i];
            cmyk[i * 4u + 3u] = plane_k[i];
        }

        {
            sixel_cms_profile_t *src_profile;
            sixel_cms_profile_t *dst_profile;
            sixel_cms_transform_t *transform;

            src_profile = NULL;
            dst_profile = NULL;
            transform = NULL;
            src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                          icc_profile_length);
            if (src_profile != NULL) {
                dst_profile = sixel_cms_create_srgb_profile();
                if (dst_profile != NULL) {
                    transform = sixel_cms_create_transform(
                        src_profile,
                        SIXEL_CMS_PIXELFORMAT_CMYK_8,
                        dst_profile,
                        SIXEL_CMS_PIXELFORMAT_RGB_F32,
                        SIXEL_CMS_TRANSFORM_DEFAULT);
                }
                if (transform != NULL &&
                    sixel_cms_do_transform(transform,
                                           cmyk,
                                           rgbf32,
                                           pixel_count)) {
                    cms_applied = 1;
                }
            }
            sixel_cms_delete_transform(transform);
            sixel_cms_close_profile(dst_profile);
            sixel_cms_close_profile(src_profile);
        }
        sixel_allocator_free(chunk->allocator, cmyk);
        cmyk = NULL;

        if (cms_applied) {
            if (plane_alpha != NULL) {
                float bg_r;
                float bg_g;
                float bg_b;
                float alpha_f;
                float one_minus_alpha;

                bg_r = (float)bgcolor[0] / 255.0f;
                bg_g = (float)bgcolor[1] / 255.0f;
                bg_b = (float)bgcolor[2] / 255.0f;
                for (i = 0u; i < pixel_count; ++i) {
                    alpha_f = (float)plane_alpha[i] / 255.0f;
                    one_minus_alpha = 1.0f - alpha_f;
                    rgbf32[i * 3u + 0u] =
                        rgbf32[i * 3u + 0u] * alpha_f + bg_r * one_minus_alpha;
                    rgbf32[i * 3u + 1u] =
                        rgbf32[i * 3u + 1u] * alpha_f + bg_g * one_minus_alpha;
                    rgbf32[i * 3u + 2u] =
                        rgbf32[i * 3u + 2u] * alpha_f + bg_b * one_minus_alpha;
                }
            }
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);

            *pcms_applied = 1;
            *ppixels = (unsigned char *)rgbf32;
            *pwidth = (int)info->width;
            *pheight = (int)info->height;
            *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
            return SIXEL_OK;
        }
        sixel_allocator_free(chunk->allocator, rgbf32);
        rgbf32 = NULL;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        c = (int)plane_c[i];
        m = (int)plane_m[i];
        y = (int)plane_y[i];
        k = (int)plane_k[i];
        r = ((0xff - c) * (0xff - k) + 0x7f) / 0xff;
        g = ((0xff - m) * (0xff - k) + 0x7f) / 0xff;
        b = ((0xff - y) * (0xff - k) + 0x7f) / 0xff;
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
            g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
            b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_k);
    sixel_allocator_free(chunk->allocator, plane_y);
    sixel_allocator_free(chunk->allocator, plane_m);
    sixel_allocator_free(chunk->allocator, plane_c);

    *ppixels = rgb;
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pcms_applied = 0;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_rgb_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane_r;
    unsigned char *plane_g;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int alpha;
    int r;
    int g;
    int b;

    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode != 3u || info->depth != 8u ||
        info->compression > 1u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_r = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_g = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_r == NULL || plane_g == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    want_alpha = (bgcolor != NULL && info->channels >= 4u) ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_r) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_g) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed raw/RLE channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        r = (int)plane_r[i];
        g = (int)plane_g[i];
        b = (int)plane_b[i];
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
            g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
            b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);

    *ppixels = rgb;
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

static float
sixel_builtin_psd_lab_decode_l(unsigned char value)
{
    return (float)value / 255.0f;
}

static float
sixel_builtin_psd_lab_decode_ab(unsigned char value)
{
    return ((float)(int)value - 128.0f) / 128.0f;
}

static float
sixel_builtin_psd_lab_clamp_ab(float value)
{
    if (value < -1.5f) {
        return -1.5f;
    }
    if (value > 1.5f) {
        return 1.5f;
    }
    return value;
}

static SIXELSTATUS
sixel_builtin_psd_rgb_bgcolor_to_cielab(unsigned char const *bgcolor,
                                        float out_lab[3])
{
    SIXELSTATUS status;
    unsigned char lab8[3];

    status = SIXEL_FALSE;
    if (bgcolor == NULL || out_lab == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lab8[0] = bgcolor[0];
    lab8[1] = bgcolor[1];
    lab8[2] = bgcolor[2];
    status = sixel_helper_convert_colorspace(lab8,
                                             3u,
                                             SIXEL_PIXELFORMAT_RGB888,
                                             SIXEL_COLORSPACE_GAMMA,
                                             SIXEL_COLORSPACE_CIELAB);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    out_lab[0] = sixel_builtin_psd_lab_decode_l(lab8[0]);
    out_lab[1] = sixel_builtin_psd_lab_decode_ab(lab8[1]);
    out_lab[2] = sixel_builtin_psd_lab_decode_ab(lab8[2]);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_lab_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    SIXELSTATUS status;
    unsigned char *plane_l;
    unsigned char *plane_a;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    float *lab;
    float bg_lab[3];
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int alpha;
    float l;
    float a;
    float b;

    status = SIXEL_FALSE;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    lab = NULL;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    alpha = 0;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode != 9u || info->depth != 8u ||
        info->compression > 1u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_l = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_a = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    want_alpha = (bgcolor != NULL && info->channels >= 4u) ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_a);
            sixel_allocator_free(chunk->allocator, plane_l);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_l) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_a) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed raw/RLE channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (plane_alpha != NULL) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_a);
            sixel_allocator_free(chunk->allocator, plane_l);
            return status;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        l = sixel_builtin_psd_lab_decode_l(plane_l[i]);
        a = sixel_builtin_psd_lab_decode_ab(plane_a[i]);
        b = sixel_builtin_psd_lab_decode_ab(plane_b[i]);
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            l = (l * (float)alpha + bg_lab[0] * (255.0f - (float)alpha)) / 255.0f;
            a = (a * (float)alpha + bg_lab[1] * (255.0f - (float)alpha)) / 255.0f;
            b = (b * (float)alpha + bg_lab[2] * (255.0f - (float)alpha)) / 255.0f;
        }
        lab[i * 3u + 0u] = l;
        lab[i * 3u + 1u] = sixel_builtin_psd_lab_clamp_ab(a);
        lab[i * 3u + 2u] = sixel_builtin_psd_lab_clamp_ab(b);
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);

    *ppixels = (unsigned char *)lab;
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
    return SIXEL_OK;
}

/*
 * Convert palette entries into RGB triplets while resolving alpha.
 *
 * Notes:
 * - RGB/RGBA palettes keep channel order as-is.
 * - Grayscale palettes (1 component) are expanded to RGB.
 * - RGBA entries are pre-blended against the background color so PAL8
 *   output stays opaque and matches the libpng backend.
 */
static SIXELSTATUS
convert_palette_to_rgb(
    unsigned char **ppalette_rgb,
    unsigned char *palette,
    int palette_colors,
    int palette_comp,
    unsigned char *bgcolor,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *rgb_palette;
    int i;
    int bg_r;
    int bg_g;
    int bg_b;
    unsigned char gray;
    int alpha;

    status = SIXEL_FALSE;
    rgb_palette = NULL;
    i = 0;
    bg_r = 0;
    bg_g = 0;
    bg_b = 0;
    gray = 0;
    alpha = 0;

    if (ppalette_rgb == NULL || palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette_colors <= 0 ||
        (palette_comp != 1 && palette_comp != 3 && palette_comp != 4)) {
        return SIXEL_BAD_INPUT;
    }

    rgb_palette = (unsigned char *)
        sixel_allocator_malloc(allocator, (size_t)palette_colors * 3);
    if (rgb_palette == NULL) {
        sixel_helper_set_additional_message(
            "convert_palette_to_rgb: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (bgcolor != NULL) {
        bg_r = bgcolor[0];
        bg_g = bgcolor[1];
        bg_b = bgcolor[2];
    }

    for (i = 0; i < palette_colors; ++i) {
        if (palette_comp == 4) {
            alpha = palette[i * 4 + 3];
            if (alpha < 0xff) {
                /*
                 * Keep integer blending consistent with the libpng loader.
                 */
                rgb_palette[i * 3 + 0] =
                    (unsigned char)(((0xff - alpha) * bg_r
                                     + alpha * palette[i * 4 + 0]) >> 8);
                rgb_palette[i * 3 + 1] =
                    (unsigned char)(((0xff - alpha) * bg_g
                                     + alpha * palette[i * 4 + 1]) >> 8);
                rgb_palette[i * 3 + 2] =
                    (unsigned char)(((0xff - alpha) * bg_b
                                     + alpha * palette[i * 4 + 2]) >> 8);
            } else {
                rgb_palette[i * 3 + 0] = palette[i * 4 + 0];
                rgb_palette[i * 3 + 1] = palette[i * 4 + 1];
                rgb_palette[i * 3 + 2] = palette[i * 4 + 2];
            }
        } else if (palette_comp == 3) {
            rgb_palette[i * 3 + 0] = palette[i * 3 + 0];
            rgb_palette[i * 3 + 1] = palette[i * 3 + 1];
            rgb_palette[i * 3 + 2] = palette[i * 3 + 2];
        } else {
            gray = palette[i];
            rgb_palette[i * 3 + 0] = gray;
            rgb_palette[i * 3 + 1] = gray;
            rgb_palette[i * 3 + 2] = gray;
        }
    }

    stbi_free(palette);
    *ppalette_rgb = rgb_palette;
    status = SIXEL_OK;

    return status;
}

typedef union _fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} fn_pointer;

static int
sixel_builtin_trns_keycolor_mode(void)
{
    char const *env_value;
    static int initialized = 0;
    static int mode = 1;
    /*
     * mode:
     *   0 -> disabled
     *   1 -> tRNS keycolor only (default when env is unset)
     *   2 -> tRNS keycolor + alpha-channel keycolor
     */

    if (initialized) {
        return mode;
    }
    initialized = 1;
    mode = 1;

    env_value = sixel_compat_getenv("SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR");
    if (env_value == NULL || env_value[0] == '\0') {
        return mode;
    }
    if (env_value[0] == '1' && env_value[1] == '\0') {
        mode = 2;
    } else if (env_value[0] == '0' && env_value[1] == '\0') {
        mode = 0;
    } else {
        mode = 0;
    }

    return mode;
}

static int
sixel_builtin_parse_png_transparency_info(
    sixel_chunk_t const *pchunk,
    int *color_type_out,
    int *has_alpha_chunk_out,
    int *has_trns_chunk_out)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    uint32_t length;
    size_t chunk_total;
    unsigned char const *chunk_type;
    int color_type;
    int has_trns_chunk;

    if (pchunk == NULL ||
        color_type_out == NULL ||
        has_alpha_chunk_out == NULL ||
        has_trns_chunk_out == NULL) {
        return 0;
    }

    buffer = pchunk->buffer;
    size = pchunk->size;
    if (buffer == NULL || size < (8u + 12u + 13u)) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }
    if ((((uint32_t)buffer[8u] << 24)
         | ((uint32_t)buffer[9u] << 16)
         | ((uint32_t)buffer[10u] << 8)
         | (uint32_t)buffer[11u]) != 13u) {
        return 0;
    }
    if (memcmp(buffer + 12u, "IHDR", 4u) != 0) {
        return 0;
    }

    color_type = (int)buffer[25u];
    has_trns_chunk = 0;
    offset = 8u;
    while (offset + 12u <= size) {
        length = ((uint32_t)buffer[offset + 0u] << 24)
               | ((uint32_t)buffer[offset + 1u] << 16)
               | ((uint32_t)buffer[offset + 2u] << 8)
               | (uint32_t)buffer[offset + 3u];
        chunk_total = (size_t)length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "tRNS", 4u) == 0) {
            has_trns_chunk = 1;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
        offset += chunk_total;
    }

    *color_type_out = color_type;
    *has_alpha_chunk_out =
        (color_type & SIXEL_BUILTIN_PNG_COLOR_MASK_ALPHA) != 0 ? 1 : 0;
    *has_trns_chunk_out = has_trns_chunk;

    return 1;
}

static int
sixel_builtin_png_keycolor_mode_enabled(
    sixel_chunk_t const *pchunk,
    unsigned char const *bgcolor,
    int enable_cms)
{
    int trns_keycolor_mode;
    int color_type;
    int has_alpha_chunk;
    int has_trns_chunk;

    trns_keycolor_mode = 0;
    color_type = (-1);
    has_alpha_chunk = 0;
    has_trns_chunk = 0;

    trns_keycolor_mode = sixel_builtin_trns_keycolor_mode();
    if (trns_keycolor_mode == 0) {
        return 0;
    }
    if (bgcolor != NULL || enable_cms) {
        return 0;
    }
    if (!sixel_builtin_parse_png_transparency_info(
            pchunk,
            &color_type,
            &has_alpha_chunk,
            &has_trns_chunk)) {
        return 0;
    }

    return ((has_trns_chunk && !has_alpha_chunk)
            || (has_alpha_chunk && trns_keycolor_mode == 2))
        ? 1
        : 0;
}

static SIXELSTATUS
sixel_builtin_parse_animation_start_frame_no(int *start_frame_no)
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
sixel_builtin_resolve_animation_start_frame_no(int start_frame_no,
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
sixel_builtin_count_gif_frames(sixel_chunk_t const *pchunk, int *frame_count)
{
    static size_t const gif_color_table_bytes[8] = {
        6u, 12u, 24u, 48u, 96u, 192u, 384u, 768u
    };
    SIXELSTATUS status;
    unsigned char const *p;
    unsigned char const *end;
    size_t gct_size;
    size_t lct_size;
    unsigned char marker;
    unsigned char packed;
    unsigned char block_size;
    int count;

    status = SIXEL_OK;
    p = NULL;
    end = NULL;
    gct_size = 0;
    lct_size = 0;
    marker = 0;
    packed = 0;
    block_size = 0;
    count = 0;

    if (pchunk->size < 13) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    p = pchunk->buffer;
    end = pchunk->buffer + pchunk->size;
    if (memcmp(p, "GIF", 3) != 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    packed = p[10];
    p += 13;
    if ((packed & 0x80) != 0) {
        gct_size = gif_color_table_bytes[(size_t)packed & 0x07u];
        if ((size_t)(end - p) < gct_size) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        p += gct_size;
    }

    while (p < end) {
        marker = *p++;
        if (marker == 0x3b) {
            break;
        }

        if (marker == 0x2c) {
            if ((size_t)(end - p) < 9) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }

            packed = p[8];
            p += 9;
            if ((packed & 0x80) != 0) {
                lct_size = gif_color_table_bytes[(size_t)packed & 0x07u];
                if ((size_t)(end - p) < lct_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += lct_size;
            }

            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++p;

            for (;;) {
                if (p >= end) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                block_size = *p++;
                if (block_size == 0) {
                    break;
                }
                if ((size_t)(end - p) < block_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += block_size;
            }
            ++count;
            continue;
        }

        if (marker != 0x21) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (p >= end) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        ++p;

        for (;;) {
            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            block_size = *p++;
            if (block_size == 0) {
                break;
            }
            if ((size_t)(end - p) < block_size) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            p += block_size;
        }
    }

    if (count <= 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *frame_count = count;

end:
    return status;
}

/*
 * Builtin APNG pipeline (stb_image backend):
 *
 *   PNG byte stream
 *      -> parse APNG chunks (acTL/fcTL/fdAT/IDAT)
 *      -> rebuild one normal PNG per frame
 *      -> decode frame RGBA with stb_image
 *      -> composite into canvas using blend/dispose ops
 *      -> emit sixel_frame_t through loader callback
 *
 * This keeps APNG available even when libpng loader is disabled.
 */
typedef struct sixel_builtin_apng_frame_control {
    uint32_t width;
    uint32_t height;
    uint32_t x_offset;
    uint32_t y_offset;
    unsigned int delay_cs;
    unsigned int dispose_op;
    unsigned int blend_op;
} sixel_builtin_apng_frame_control_t;

typedef struct sixel_builtin_apng_canvas {
    unsigned char *pixels;
    unsigned char *backup;
    int width;
    int height;
} sixel_builtin_apng_canvas_t;

typedef struct sixel_builtin_apng_state {
    unsigned char const *ihdr;
    size_t ihdr_size;
    unsigned char *shared_chunks;
    size_t shared_chunks_size;
    size_t shared_chunks_capacity;
    unsigned char *chunk_base;
    size_t chunk_size;
    size_t chunk_capacity;
    uint32_t expected_sequence;
} sixel_builtin_apng_state_t;

static uint32_t
sixel_builtin_read_be32(unsigned char const *p)
{
    uint32_t value;

    value = ((uint32_t)p[0] << 24)
          | ((uint32_t)p[1] << 16)
          | ((uint32_t)p[2] << 8)
          | (uint32_t)p[3];
    return value;
}

static void
sixel_builtin_write_be32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static uint32_t
sixel_builtin_crc32_update(unsigned char const *data, size_t length,
                           uint32_t seed)
{
    uint32_t crc;
    size_t i;
    int bit;

    crc = ~seed;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xedb88320U;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

static int
sixel_builtin_apng_ensure_shared_capacity(
    sixel_builtin_apng_state_t *state,
    size_t append_size,
    sixel_allocator_t *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->shared_chunks_size) {
        return 0;
    }
    needed = state->shared_chunks_size + append_size;
    if (needed <= state->shared_chunks_capacity) {
        return 1;
    }

    next_capacity = state->shared_chunks_capacity;
    if (next_capacity == 0) {
        next_capacity = 1024;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }

    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->shared_chunks_size > 0 && state->shared_chunks != NULL) {
        memcpy(next, state->shared_chunks, state->shared_chunks_size);
    }

    sixel_allocator_free(allocator, state->shared_chunks);
    state->shared_chunks = next;
    state->shared_chunks_capacity = next_capacity;
    return 1;
}

static int
sixel_builtin_apng_append_shared_chunk(
    sixel_builtin_apng_state_t *state,
    unsigned char const *chunk,
    size_t chunk_size,
    sixel_allocator_t *allocator)
{
    if (!sixel_builtin_apng_ensure_shared_capacity(state, chunk_size,
                                                   allocator)) {
        return 0;
    }

    memcpy(state->shared_chunks + state->shared_chunks_size,
           chunk,
           chunk_size);
    state->shared_chunks_size += chunk_size;
    return 1;
}

static int
sixel_builtin_apng_ensure_chunk_capacity(
    sixel_builtin_apng_state_t *state,
    size_t append_size,
    sixel_allocator_t *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->chunk_size) {
        return 0;
    }
    needed = state->chunk_size + append_size;
    if (needed <= state->chunk_capacity) {
        return 1;
    }

    next_capacity = state->chunk_capacity;
    if (next_capacity == 0) {
        next_capacity = 4096;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }

    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->chunk_size > 0 && state->chunk_base != NULL) {
        memcpy(next, state->chunk_base, state->chunk_size);
    }

    sixel_allocator_free(allocator, state->chunk_base);
    state->chunk_base = next;
    state->chunk_capacity = next_capacity;
    return 1;
}

static int
sixel_builtin_apng_append_chunk(
    sixel_builtin_apng_state_t *state,
    char const *type,
    unsigned char const *data,
    uint32_t length,
    sixel_allocator_t *allocator)
{
    unsigned char *dst;
    uint32_t crc;
    size_t chunk_bytes;

    chunk_bytes = (size_t)length + 12;
    if (!sixel_builtin_apng_ensure_chunk_capacity(state, chunk_bytes,
                                                  allocator)) {
        return 0;
    }

    dst = state->chunk_base + state->chunk_size;
    sixel_builtin_write_be32(dst, length);
    memcpy(dst + 4, type, 4);
    if (length > 0 && data != NULL) {
        memcpy(dst + 8, data, length);
    }

    crc = sixel_builtin_crc32_update(dst + 4, 4, 0);
    if (length > 0 && data != NULL) {
        crc = sixel_builtin_crc32_update(data, length, crc);
    }
    sixel_builtin_write_be32(dst + 8 + length, crc);
    state->chunk_size += chunk_bytes;
    return 1;
}

static int
sixel_builtin_apng_parse_fctl(
    unsigned char const *data,
    uint32_t length,
    uint32_t *sequence_no,
    sixel_builtin_apng_frame_control_t *control)
{
    uint32_t delay_num;
    uint32_t delay_den;

    if (length != 26 || sequence_no == NULL || control == NULL) {
        return 0;
    }

    *sequence_no = sixel_builtin_read_be32(data + 0);
    control->width = sixel_builtin_read_be32(data + 4);
    control->height = sixel_builtin_read_be32(data + 8);
    control->x_offset = sixel_builtin_read_be32(data + 12);
    control->y_offset = sixel_builtin_read_be32(data + 16);
    delay_num = (uint32_t)(((unsigned int)data[20] << 8) | data[21]);
    delay_den = (uint32_t)(((unsigned int)data[22] << 8) | data[23]);
    control->dispose_op = data[24];
    control->blend_op = data[25];

    if (control->dispose_op > 2 || control->blend_op > 1) {
        return 0;
    }

    if (delay_den == 0) {
        delay_den = 100;
    }
    control->delay_cs = (unsigned int)((delay_num * 100U) / delay_den);
    if (control->delay_cs == 0 && delay_num > 0) {
        control->delay_cs = 1;
    }

    return 1;
}

static void
sixel_builtin_apng_clear_rect(sixel_builtin_apng_canvas_t const *canvas,
                              sixel_builtin_apng_frame_control_t *control)
{
    int x;
    int y;
    int px;
    int py;
    unsigned char *dst;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            dst = canvas->pixels + ((py * canvas->width + px) * 4);
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 0;
        }
    }
}

static void
sixel_builtin_apng_blend_rect(sixel_builtin_apng_canvas_t const *canvas,
                              sixel_builtin_apng_frame_control_t *control,
                              unsigned char const *src)
{
    int x;
    int y;
    int px;
    int py;
    int idx;
    unsigned int sa;
    unsigned int da;
    unsigned int oa;
    unsigned char const *sp;
    unsigned char *dp;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            idx = y * (int)control->width + x;
            sp = src + idx * 4;
            dp = canvas->pixels + ((py * canvas->width + px) * 4);

            if (control->blend_op == 0) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
                continue;
            }

            sa = sp[3];
            da = dp[3];
            oa = sa + ((da * (255 - sa)) / 255);
            if (oa == 0) {
                dp[0] = 0;
                dp[1] = 0;
                dp[2] = 0;
                dp[3] = 0;
                continue;
            }
            dp[0] = (unsigned char)((sp[0] * sa + dp[0] * da
                                     * (255 - sa) / 255) / oa);
            dp[1] = (unsigned char)((sp[1] * sa + dp[1] * da
                                     * (255 - sa) / 255) / oa);
            dp[2] = (unsigned char)((sp[2] * sa + dp[2] * da
                                     * (255 - sa) / 255) / oa);
            dp[3] = (unsigned char)oa;
        }
    }
}

static SIXELSTATUS
sixel_builtin_apng_emit_frame(
    sixel_builtin_apng_state_t const *state,
    sixel_builtin_apng_frame_control_t *control,
    int frame_no,
    int loop_no,
    int multiframe,
    int emit_callback,
    unsigned char *bgcolor,
    int alpha_zero_is_transparent,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_load_image_function fn_load,
    void *callback_context,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    stbi__context stb_context;
    size_t png_size;
    unsigned char *png_data;
    unsigned char *subframe;
    unsigned char *emitted;
    unsigned char ihdr_copy[13];
    size_t canvas_bytes;
    int width;
    int height;
    int depth;

    status = SIXEL_FALSE;
    frame = NULL;
    stb_context = (stbi__context){ 0 };
    png_data = NULL;
    subframe = NULL;
    emitted = NULL;
    width = 0;
    height = 0;
    depth = 0;

    if (state->ihdr == NULL || state->ihdr_size != 13) {
        return SIXEL_BAD_INPUT;
    }
    if (state->chunk_size > SIZE_MAX - 8 - 25) {
        return SIXEL_BAD_ALLOCATION;
    }

    png_size = 8 + 25 + state->shared_chunks_size + state->chunk_size + 12;
    png_data = (unsigned char *)sixel_allocator_malloc(allocator, png_size);
    if (png_data == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(png_data, "\x89PNG\r\n\x1a\n", 8);
    memcpy(ihdr_copy, state->ihdr, sizeof(ihdr_copy));
    sixel_builtin_write_be32(ihdr_copy + 0, control->width);
    sixel_builtin_write_be32(ihdr_copy + 4, control->height);

    memcpy(png_data + 8, "\x00\x00\x00\x0dIHDR", 8);
    memcpy(png_data + 16, ihdr_copy, sizeof(ihdr_copy));
    sixel_builtin_write_be32(
        png_data + 29,
        sixel_builtin_crc32_update((unsigned char const *)(png_data + 12),
                                   17,
                                   0));

    if (state->shared_chunks_size > 0) {
        memcpy(png_data + 33, state->shared_chunks, state->shared_chunks_size);
    }
    memcpy(png_data + 33 + state->shared_chunks_size,
           state->chunk_base,
           state->chunk_size);
    memcpy(png_data + 33 + state->shared_chunks_size + state->chunk_size,
           "\x00\x00\x00\x00"
           "IEND"
           "\xae\x42\x60\x82",
           12);

    if (png_size > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    stbi__start_mem(&stb_context, png_data, (int)png_size);
    subframe = stbi__load_and_postprocess_8bit(&stb_context,
                                               &width,
                                               &height,
                                               &depth,
                                               4);
    if (subframe == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        status = SIXEL_STBI_ERROR;
        goto end;
    }
    if (width != (int)control->width || height != (int)control->height) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    canvas_bytes = (size_t)canvas->width * (size_t)canvas->height * 4;
    if (control->dispose_op == 2) {
        memcpy(canvas->backup, canvas->pixels, canvas_bytes);
    }
    sixel_builtin_apng_blend_rect(canvas, control, subframe);

    if (!emit_callback) {
        status = SIXEL_OK;
        goto dispose;
    }

    emitted = (unsigned char *)sixel_allocator_malloc(allocator, canvas_bytes);
    if (emitted == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(emitted, canvas->pixels, canvas_bytes);

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->width = canvas->width;
    frame->height = canvas->height;
    frame->palette = NULL;
    frame->ncolors = 0;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent =
        alpha_zero_is_transparent != 0 ? 1 : 0;
    sixel_frame_set_delay(frame, (int)control->delay_cs);
    sixel_frame_set_frame_no(frame, frame_no);
    sixel_frame_set_loop_count(frame, loop_no);
    sixel_frame_set_multiframe(frame, multiframe);
    sixel_frame_set_pixels(frame, emitted);
    emitted = NULL;

    if (!frame->alpha_zero_is_transparent) {
        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = fn_load(frame, callback_context);

dispose:

    if (control->dispose_op == 1) {
        sixel_builtin_apng_clear_rect(canvas, control);
    } else if (control->dispose_op == 2) {
        memcpy(canvas->pixels, canvas->backup, canvas_bytes);
    }

end:
    sixel_allocator_free(allocator, png_data);
    stbi_free(subframe);
    sixel_allocator_free(allocator, emitted);
    sixel_frame_unref(frame);
    return status;
}

static SIXELSTATUS
sixel_builtin_load_apng_frames(
    sixel_chunk_t const *pchunk,
    int fstatic,
    unsigned char *bgcolor,
    int enable_cms,
    int loop_control,
    int start_frame_no,
    sixel_load_image_function fn_load,
    void *context)
{
    SIXELSTATUS status;
    sixel_builtin_apng_state_t state;
    sixel_builtin_apng_frame_control_t control;
    sixel_builtin_apng_canvas_t canvas;
    unsigned char const *p;
    size_t remain;
    size_t canvas_bytes;
    int seen_actl;
    int saw_animation;
    int has_frame;
    int emit_frame_no;
    int source_frame_no;
    int num_frames;
    int num_plays;
    int loop_no;
    int frames_in_loop;
    int stop_loop;
    int emit_callback;
    int seen_fctl;
    int seen_idat;
    int alpha_zero_is_transparent;
    int color_type;
    int has_alpha_chunk;
    int has_trns_chunk;
    int trns_keycolor_mode;
    uint32_t length;
    uint32_t canvas_width;
    uint32_t canvas_height;
    uint32_t sequence_no;
    uint32_t fd_sequence;

    status = SIXEL_FALSE;
    memset(&state, 0, sizeof(state));
    memset(&control, 0, sizeof(control));
    memset(&canvas, 0, sizeof(canvas));
    p = NULL;
    remain = 0;
    canvas_bytes = 0;
    seen_actl = 0;
    saw_animation = 0;
    has_frame = 0;
    emit_frame_no = 0;
    source_frame_no = 0;
    num_frames = 0;
    num_plays = 0;
    loop_no = 0;
    frames_in_loop = 0;
    stop_loop = 0;
    emit_callback = 1;
    seen_fctl = 0;
    seen_idat = 0;
    alpha_zero_is_transparent = 0;
    color_type = (-1);
    has_alpha_chunk = 0;
    has_trns_chunk = 0;
    trns_keycolor_mode = sixel_builtin_trns_keycolor_mode();
    length = 0;
    canvas_width = 0;
    canvas_height = 0;
    sequence_no = 0;
    fd_sequence = 0;

    if (pchunk->size < 8) {
        return SIXEL_FALSE;
    }

    for (;;) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        memset(&state, 0, sizeof(state));
        memset(&control, 0, sizeof(control));
        p = pchunk->buffer + 8;
        remain = pchunk->size - 8;
        seen_actl = 0;
        has_frame = 0;
        source_frame_no = 0;
        frames_in_loop = 0;
        seen_fctl = 0;
        seen_idat = 0;
        alpha_zero_is_transparent = 0;
        color_type = (-1);
        has_alpha_chunk = 0;
        has_trns_chunk = 0;

        if (loop_no > 0 && canvas_bytes > 0) {
            memset(canvas.pixels, 0, canvas_bytes);
            memset(canvas.backup, 0, canvas_bytes);
        }

        while (remain >= 12) {
            if (sixel_loader_callback_is_canceled(context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }

            length = sixel_builtin_read_be32(p);
            if ((size_t)length > remain - 12) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }

            if (memcmp(p + 4, "IHDR", 4) == 0) {
                if (length != 13) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                state.ihdr = p + 8;
                state.ihdr_size = length;
                color_type = (int)p[17];
                has_alpha_chunk = (color_type
                                   & SIXEL_BUILTIN_PNG_COLOR_MASK_ALPHA) != 0
                    ? 1
                    : 0;
                if (canvas_bytes == 0) {
                    canvas_width = sixel_builtin_read_be32(p + 8);
                    canvas_height = sixel_builtin_read_be32(p + 12);
                    if (canvas_width == 0 || canvas_height == 0 ||
                        canvas_width > INT_MAX || canvas_height > INT_MAX) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    canvas.width = (int)canvas_width;
                    canvas.height = (int)canvas_height;
                    canvas_bytes = (size_t)canvas.width
                                 * (size_t)canvas.height * 4;
                    canvas.pixels = (unsigned char *)sixel_allocator_malloc(
                        pchunk->allocator,
                        canvas_bytes);
                    canvas.backup = (unsigned char *)sixel_allocator_malloc(
                        pchunk->allocator,
                        canvas_bytes);
                    if (canvas.pixels == NULL || canvas.backup == NULL) {
                        status = SIXEL_BAD_ALLOCATION;
                        goto end;
                    }
                    memset(canvas.pixels, 0, canvas_bytes);
                    memset(canvas.backup, 0, canvas_bytes);
                }
                alpha_zero_is_transparent =
                    trns_keycolor_mode != 0 &&
                    bgcolor == NULL &&
                    !enable_cms &&
                    ((has_trns_chunk &&
                      !has_alpha_chunk)
                     || (has_alpha_chunk && trns_keycolor_mode == 2));
            } else if (memcmp(p + 4, "acTL", 4) == 0) {
                if (length != 8) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                seen_actl = 1;
                saw_animation = 1;
                num_frames = (int)sixel_builtin_read_be32(p + 8);
                num_plays = (int)sixel_builtin_read_be32(p + 12);
                state.expected_sequence = 0;
                if (num_frames <= 0) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (loop_no == 0 && start_frame_no != INT_MIN) {
                    status = sixel_builtin_resolve_animation_start_frame_no(
                        start_frame_no,
                        num_frames,
                        &start_frame_no);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }
            } else if (memcmp(p + 4, "fcTL", 4) == 0 && seen_actl) {
                if (has_frame && state.chunk_size > 0) {
                    emit_callback = 1;
                    if (loop_no == 0 && start_frame_no != INT_MIN &&
                        frames_in_loop < start_frame_no) {
                        emit_callback = 0;
                    }
                    if (loop_no == 0 && start_frame_no != INT_MIN) {
                        /*
                         * frame_no is consumed by the encoder to determine
                         * whether DECSC (first emitted frame) or DECRC
                         * (subsequent frame) should be written in tty scroll.
                         * Keep it as an emitted-frame index for the first loop
                         * when start-frame skips leading source frames.
                         */
                        emit_frame_no = source_frame_no - start_frame_no;
                    } else {
                        emit_frame_no = source_frame_no;
                    }
                    status = sixel_builtin_apng_emit_frame(
                        &state,
                        &control,
                        emit_frame_no,
                        loop_no,
                        (!fstatic && num_frames > 1),
                        emit_callback,
                        bgcolor,
                        alpha_zero_is_transparent,
                        &canvas,
                        fn_load,
                        context,
                        pchunk->allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    ++source_frame_no;
                    ++frames_in_loop;
                    if (fstatic && emit_callback) {
                        status = SIXEL_OK;
                        goto end;
                    }
                    state.chunk_size = 0;
                }

                if (!sixel_builtin_apng_parse_fctl(p + 8,
                                                   length,
                                                   &sequence_no,
                                                   &control)) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (sequence_no != state.expected_sequence) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                ++state.expected_sequence;
                if (control.width == 0 || control.height == 0 ||
                    control.x_offset > (uint32_t)canvas.width ||
                    control.y_offset > (uint32_t)canvas.height ||
                    control.width > (uint32_t)canvas.width
                                    - control.x_offset ||
                    control.height > (uint32_t)canvas.height
                                     - control.y_offset) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                seen_fctl = 1;
                has_frame = 1;
            } else if (memcmp(p + 4, "fdAT", 4) == 0 && seen_actl) {
                if (!has_frame || !seen_fctl || length < 4) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                fd_sequence = sixel_builtin_read_be32(p + 8);
                if (fd_sequence != state.expected_sequence) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                ++state.expected_sequence;
                if (!sixel_builtin_apng_append_chunk(
                        &state,
                        "IDAT",
                        p + 12,
                        length - 4,
                        pchunk->allocator)) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
            } else if (memcmp(p + 4, "IDAT", 4) == 0) {
                if (seen_actl && !has_frame) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (!sixel_builtin_apng_append_chunk(
                        &state,
                        "IDAT",
                        p + 8,
                        length,
                        pchunk->allocator)) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (seen_actl && !seen_fctl && !seen_idat) {
                    control.width = (uint32_t)canvas.width;
                    control.height = (uint32_t)canvas.height;
                    control.x_offset = 0;
                    control.y_offset = 0;
                    control.delay_cs = 0;
                    control.dispose_op = 0;
                    control.blend_op = 0;
                }
                seen_idat = 1;
                has_frame = 1;
            } else if (memcmp(p + 4, "IEND", 4) == 0) {
                break;
            } else if (memcmp(p + 4, "tRNS", 4) == 0) {
                has_trns_chunk = 1;
                alpha_zero_is_transparent =
                    trns_keycolor_mode != 0 &&
                    bgcolor == NULL &&
                    !enable_cms &&
                    ((has_trns_chunk &&
                      !has_alpha_chunk)
                     || (has_alpha_chunk && trns_keycolor_mode == 2));
            } else if (memcmp(p + 4, "acTL", 4) != 0 &&
                       memcmp(p + 4, "fcTL", 4) != 0 &&
                       memcmp(p + 4, "fdAT", 4) != 0 &&
                       memcmp(p + 4, "IHDR", 4) != 0 &&
                       memcmp(p + 4, "IEND", 4) != 0 &&
                       state.chunk_size == 0) {
                if (!sixel_builtin_apng_append_shared_chunk(
                        &state,
                        p,
                        (size_t)length + 12,
                        pchunk->allocator)) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
            }

            p += (size_t)length + 12;
            remain -= (size_t)length + 12;
        }

        if (!seen_actl || !has_frame) {
            status = SIXEL_FALSE;
            goto end;
        }

        if (state.chunk_size > 0) {
            emit_callback = 1;
            if (loop_no == 0 && start_frame_no != INT_MIN &&
                frames_in_loop < start_frame_no) {
                emit_callback = 0;
            }
            if (loop_no == 0 && start_frame_no != INT_MIN) {
                /*
                 * frame_no is used by the encoder/tty path to select DECSC
                 * for the first emitted frame and DECRC for subsequent
                 * frames. Keep frame_no aligned to emitted order when the
                 * first loop skips leading source frames.
                 */
                emit_frame_no = source_frame_no - start_frame_no;
            } else {
                emit_frame_no = source_frame_no;
            }
            status = sixel_builtin_apng_emit_frame(&state,
                                                   &control,
                                                   emit_frame_no,
                                                   loop_no,
                                                   (!fstatic && num_frames > 1),
                                                   emit_callback,
                                                   bgcolor,
                                                   alpha_zero_is_transparent,
                                                   &canvas,
                                                   fn_load,
                                                   context,
                                                   pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            ++source_frame_no;
            ++frames_in_loop;
            if (fstatic && emit_callback) {
                status = SIXEL_OK;
                goto end;
            }
        }

        if (frames_in_loop == 0) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (num_frames > 0 && frames_in_loop != num_frames) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        ++loop_no;
        stop_loop = 0;
        if (loop_control == SIXEL_LOOP_DISABLE || frames_in_loop == 1) {
            stop_loop = 1;
        } else if (loop_control == SIXEL_LOOP_AUTO &&
                   num_plays > 0 &&
                   loop_no >= num_plays) {
            stop_loop = 1;
        }

        sixel_allocator_free(pchunk->allocator, state.shared_chunks);
        sixel_allocator_free(pchunk->allocator, state.chunk_base);
        state.shared_chunks = NULL;
        state.chunk_base = NULL;

        if (stop_loop) {
            status = SIXEL_OK;
            goto end;
        }
    }

end:
    sixel_allocator_free(pchunk->allocator, canvas.pixels);
    sixel_allocator_free(pchunk->allocator, canvas.backup);
    sixel_allocator_free(pchunk->allocator, state.shared_chunks);
    sixel_allocator_free(pchunk->allocator, state.chunk_base);
    if (!saw_animation && status == SIXEL_FALSE) {
        return SIXEL_FALSE;
    }
    return status;
}

static void
sixel_loader_builtin_ref(sixel_loader_component_t *component)
{
    sixel_loader_builtin_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_builtin_component_t *)component;
    (void)sixel_atomic_fetch_add_u32(&self->ref, 1u);
}

static void
sixel_loader_builtin_unref(sixel_loader_component_t *component)
{
    sixel_loader_builtin_component_t *self;
    unsigned int previous;

    self = NULL;
    previous = 0u;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_builtin_component_t *)component;
    previous = sixel_atomic_fetch_sub_u32(&self->ref, 1u);
    if (previous != 1u) {
        return;
    }

    sixel_allocator_unref(self->allocator);
    sixel_allocator_free(self->allocator, self);
}

static SIXELSTATUS
sixel_loader_builtin_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_builtin_component_t *self;
    int const *int_value;
    unsigned char const *bgcolor;

    self = NULL;
    int_value = NULL;
    bgcolor = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_builtin_component_t *)component;

    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        int_value = (int const *)value;
        self->fstatic = int_value != NULL ? *int_value : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        int_value = (int const *)value;
        self->fuse_palette = int_value != NULL ? *int_value : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->reqcolors = *int_value;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        bgcolor = (unsigned char const *)value;
        self->bgcolor[0] = bgcolor[0];
        self->bgcolor[1] = bgcolor[1];
        self->bgcolor[2] = bgcolor[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->loop_control = *int_value;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->start_frame_no = *int_value;
            self->start_frame_no_set = 1;
        } else {
            self->start_frame_no_set = 0;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_CMS:
        int_value = (int const *)value;
        self->enable_cms = (int_value != NULL && *int_value != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE:
        int_value = (int const *)value;
        if (int_value != NULL && *int_value >= 0) {
            self->enable_cms =
                (*int_value == SIXEL_CMS_ENGINE_NONE) ? 0 : 1;
        }
        sixel_helper_set_loader_cms_engine(int_value != NULL ? *int_value : -1);
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_builtin_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_builtin_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_builtin_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_builtin(chunk,
                             self->fstatic,
                             self->fuse_palette,
                             self->reqcolors,
                             bgcolor,
                             self->loop_control,
                             self->start_frame_no_set,
                             self->start_frame_no,
                             self->enable_cms,
                             fn_load,
                             context);
}

static char const *
sixel_loader_builtin_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "builtin";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_builtin_vtbl = {
    sixel_loader_builtin_ref,
    sixel_loader_builtin_unref,
    sixel_loader_builtin_setopt,
    sixel_loader_builtin_load,
    sixel_loader_builtin_name,
};

SIXELSTATUS
sixel_loader_builtin_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    sixel_loader_builtin_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_builtin_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    self->base.vtbl = &g_sixel_loader_builtin_vtbl;
    self->ref = 1u;
    self->allocator = allocator;
    sixel_allocator_ref(allocator);
    self->fstatic = 0;
    self->fuse_palette = 0;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->has_bgcolor = 0;
    self->bgcolor[0] = 0;
    self->bgcolor[1] = 0;
    self->bgcolor[2] = 0;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no_set = 0;
    self->start_frame_no = INT_MIN;
    self->enable_cms = 0;

    *ppcomponent = &self->base;
    return SIXEL_OK;
}

SIXELSTATUS
load_with_builtin(
    sixel_chunk_t const *pchunk,
    int fstatic,
    int fuse_palette,
    int reqcolors,
    unsigned char *bgcolor,
    int loop_control,
    int start_frame_no_set,
    int start_frame_no_override,
    int enable_cms,
    sixel_load_image_function fn_load,
    void *context)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    int depth;
    unsigned char *palette;
    int palette_colors;
    int palette_comp;
    sixel_frame_t *frame;
    fn_pointer fnp;
    stbi__context stb_context;
    stbi__result_info ri;
    char message[80];
    int nwrite;
    int start_frame_no;
    int resolved_start_frame_no;
    int gif_frame_count;
    int png_keycolor_mode;
    int palette_keycolor_index;
    int palette_zero_alpha_count;
    size_t palette_pixel_count;
    size_t palette_pixel_index;
    unsigned int palette_index;
    unsigned char palette_zero_alpha_map[SIXEL_PALETTE_MAX];
    unsigned char *icc_profile;
    size_t icc_profile_length;
    int cms_converted;
    int psd_icc_status;
#if HAVE_LCMS2
    uint16_t tiff_photometric;
#endif

    status = SIXEL_BAD_INPUT;
    pixels = NULL;
    depth = 0;
    palette = NULL;
    palette_colors = 0;
    palette_comp = 0;
    frame = NULL;
    fnp.fn = NULL;
    stb_context = (stbi__context){ 0 };
    ri = (stbi__result_info){ 0 };
    nwrite = 0;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = -1;
    gif_frame_count = 0;
    png_keycolor_mode = 0;
    palette_keycolor_index = -1;
    palette_zero_alpha_count = 0;
    palette_pixel_count = 0u;
    palette_pixel_index = 0u;
    palette_index = 0u;
    memset(palette_zero_alpha_map, 0, sizeof(palette_zero_alpha_map));
    icc_profile = NULL;
    icc_profile_length = 0u;
    cms_converted = 0;
    psd_icc_status = SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
#if HAVE_LCMS2
    tiff_photometric = (uint16_t)0xffffu;
#endif

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = sixel_builtin_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (pchunk == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (chunk_is_sixel(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_sixel(&pixels,
                            pchunk->buffer,
                            (int)pchunk->size,
                            &frame->width,
                            &frame->height,
                            fuse_palette ? &frame->palette: NULL,
                            &frame->ncolors,
                            reqcolors,
                            &frame->pixelformat,
                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_frame_set_pixels(frame, pixels);
    } else if (chunk_is_pnm(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_pnm(pchunk->buffer,
                          (int)pchunk->size,
                          frame->allocator,
                          &pixels,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          &frame->pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_frame_set_pixels(frame, pixels);
    } else if (chunk_is_gif(pchunk)) {
        fnp.fn = fn_load;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        if (start_frame_no != INT_MIN) {
            status = sixel_builtin_count_gif_frames(pchunk, &gif_frame_count);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            status = sixel_builtin_resolve_animation_start_frame_no(
                start_frame_no,
                gif_frame_count,
                &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        status = load_gif(pchunk->buffer,
                          (int)pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          resolved_start_frame_no,
                          fnp.p,
                          context,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;
    } else {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        stbi_allocator = pchunk->allocator;
        if (chunk_is_png(pchunk)) {
            png_keycolor_mode = sixel_builtin_png_keycolor_mode_enabled(
                pchunk,
                bgcolor,
                enable_cms);
        }
        if (chunk_is_png(pchunk)) {
            /*
             * Try APNG first for builtin PNG path. Regular PNG input returns
             * SIXEL_FALSE and then falls through to existing single-frame
             * decode logic.
             */
            status = sixel_builtin_load_apng_frames(pchunk,
                                                    fstatic,
                                                    bgcolor,
                                                    enable_cms,
                                                    loop_control,
                                                    start_frame_no,
                                                    fn_load,
                                                    context);
            if (status == SIXEL_OK || status == SIXEL_INTERRUPTED) {
                goto end;
            }
        }
        if (fuse_palette && chunk_is_png(pchunk) && !png_keycolor_mode) {
            /*
             * Try indexed PNG first to keep PAL8 output. If the PNG is not
             * paletted, fall back to the normal RGB path.
             */
            stbi__start_mem(&stb_context,
                            pchunk->buffer,
                            (int)pchunk->size);
            pixels = stbi__png_load_palette(&stb_context,
                                            &frame->width,
                                            &frame->height,
                                            &palette_comp,
                                            &palette,
                                            &palette_colors,
                                            &ri);
            if (pixels != NULL && palette != NULL) {
                if (palette_colors > reqcolors) {
                    /*
                     * Match libpng behavior: when the source palette exceeds
                     * reqcolors, drop indexed output and fall back to the
                     * non-indexed RGB path below.
                     */
                    sixel_allocator_free(pchunk->allocator, pixels);
                    stbi_free(palette);
                    pixels = NULL;
                    palette = NULL;
                } else {
                    status = convert_palette_to_rgb(&frame->palette,
                                                palette,
                                                palette_colors,
                                                palette_comp,
                                                bgcolor,
                                                pchunk->allocator);
                    if (SIXEL_FAILED(status)) {
                        stbi_free(palette);
                        sixel_allocator_free(pchunk->allocator, pixels);
                        goto end;
                    }
                    frame->ncolors = palette_colors;
                    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
                    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
                    /*
                     * Indexed PNG keeps palette entries in RGB triplets. Follow
                     * PNG colorspace fallback rules (iCCP > sRGB > cHRM/gAMA)
                     * before exposing PAL8 output.
                     */
                    if (enable_cms) {
                        sixel_frompng_apply_colorspace_fallback(
                            frame->palette,
                            palette_colors,
                            1,
                            pchunk->buffer,
                            pchunk->size,
                            pchunk->allocator);
                    }
                    sixel_frame_set_pixels(frame, pixels);
                    frame->loop_count = 1;
                    goto done;
                }
            }
            pixels = NULL;
            palette = NULL;
        }
        if (fuse_palette) {
            /*
             * Try indexed TGA next to keep PAL8 output. The TGA loader only
             * supports 8-bit indices for this path and falls back to RGB
             * otherwise.
             */
            stbi__start_mem(&stb_context,
                            pchunk->buffer,
                            (int)pchunk->size);
            if (stbi__tga_test(&stb_context)) {
                pixels = stbi__tga_load_palette(&stb_context,
                                                &frame->width,
                                                &frame->height,
                                                &palette_comp,
                                                &palette,
                                                &palette_colors,
                                                &ri);
                if (pixels != NULL && palette != NULL) {
                    status = convert_palette_to_rgb(&frame->palette,
                                                    palette,
                                                    palette_colors,
                                                    palette_comp,
                                                    bgcolor,
                                                    pchunk->allocator);
                    if (SIXEL_FAILED(status)) {
                        stbi_free(palette);
                        sixel_allocator_free(pchunk->allocator, pixels);
                        goto end;
                    }
                    frame->ncolors = palette_colors;
                    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
                    sixel_frame_set_pixels(frame, pixels);
                    frame->loop_count = 1;
                    goto done;
                }
                pixels = NULL;
                palette = NULL;
            }
        }

        if (chunk_is_png(pchunk)) {
            if (png_keycolor_mode) {
                stbi__start_mem(&stb_context,
                                pchunk->buffer,
                                (int)pchunk->size);
                pixels = stbi__png_load_palette(&stb_context,
                                                &frame->width,
                                                &frame->height,
                                                &palette_comp,
                                                &palette,
                                                &palette_colors,
                                                &ri);
                if (pixels != NULL &&
                    palette != NULL &&
                    palette_comp == 4 &&
                    palette_colors > 0 &&
                    palette_colors <= reqcolors) {
                    palette_keycolor_index = -1;
                    palette_zero_alpha_count = 0;
                    memset(palette_zero_alpha_map,
                           0,
                           sizeof(palette_zero_alpha_map));
                    for (palette_index = 0u;
                         palette_index < (unsigned int)palette_colors &&
                         palette_index < SIXEL_PALETTE_MAX;
                         ++palette_index) {
                        if (palette[palette_index * 4u + 3u] == 0u) {
                            if (palette_keycolor_index < 0) {
                                palette_keycolor_index = (int)palette_index;
                            }
                            palette_zero_alpha_map[palette_index] = 1u;
                            ++palette_zero_alpha_count;
                        }
                    }

                    status = convert_palette_to_rgb(&frame->palette,
                                                    palette,
                                                    palette_colors,
                                                    palette_comp,
                                                    bgcolor,
                                                    pchunk->allocator);
                    palette = NULL;
                    if (SIXEL_FAILED(status)) {
                        sixel_allocator_free(pchunk->allocator, pixels);
                        goto end;
                    }
                    frame->ncolors = palette_colors;
                    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
                    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
                    if (palette_keycolor_index >= 0) {
                        sixel_frame_set_transparent(frame,
                                                    palette_keycolor_index);
                    }
                    if (palette_zero_alpha_count > 1 &&
                        palette_keycolor_index >= 0 &&
                        frame->width > 0 &&
                        frame->height > 0 &&
                        (size_t)frame->width <=
                            SIZE_MAX / (size_t)frame->height) {
                        palette_pixel_count =
                            (size_t)frame->width * (size_t)frame->height;
                        for (palette_pixel_index = 0u;
                             palette_pixel_index < palette_pixel_count;
                             ++palette_pixel_index) {
                            palette_index = pixels[palette_pixel_index];
                            if ((int)palette_index != palette_keycolor_index &&
                                palette_index < SIXEL_PALETTE_MAX &&
                                palette_zero_alpha_map[palette_index] != 0u) {
                                pixels[palette_pixel_index] =
                                    (unsigned char)palette_keycolor_index;
                            }
                        }
                    }
                    sixel_frame_set_pixels(frame, pixels);
                    frame->loop_count = 1;
                    goto done;
                }

                if (palette != NULL) {
                    stbi_free(palette);
                    palette = NULL;
                }
                if (pixels != NULL) {
                    sixel_allocator_free(pchunk->allocator, pixels);
                    pixels = NULL;
                }
                pixels = stbi_load_from_memory(pchunk->buffer,
                                               (int)pchunk->size,
                                               &frame->width,
                                               &frame->height,
                                               &depth,
                                               4);
                if (pixels == NULL) {
                    sixel_helper_set_additional_message(stbi_failure_reason());
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                sixel_frame_set_pixels(frame, pixels);
                frame->loop_count = 1;
                frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                frame->colorspace = SIXEL_COLORSPACE_GAMMA;
                frame->alpha_zero_is_transparent = 1;
            } else {
                status = sixel_frompng_load_nonindexed(pchunk,
                                                       frame,
                                                       enable_cms,
                                                       bgcolor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        } else {
            stbi__start_mem(&stb_context,
                            pchunk->buffer,
                            (int)pchunk->size);
            if (chunk_is_jpeg(pchunk)) {
                float *float_pixels;

                float_pixels = stbi__jpeg_loadf(&stb_context,
                                                &frame->width,
                                                &frame->height,
                                                &depth,
                                                3,
                                                &ri);
                if (float_pixels == NULL) {
                    sixel_helper_set_additional_message(stbi_failure_reason());
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                pixels = (unsigned char *)float_pixels;
                sixel_frame_set_pixels(frame, pixels);
                frame->loop_count = 1;
                if (enable_cms) {
                    if (sixel_builtin_extract_jpeg_icc(pchunk->buffer,
                                                       pchunk->size,
                                                       &icc_profile,
                                                       &icc_profile_length,
                                                       pchunk->allocator)) {
                        cms_converted =
                            sixel_cms_convert_to_srgb_with_profile_bytes(
                            (unsigned char *)float_pixels,
                            frame->width,
                            frame->height,
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            icc_profile,
                            icc_profile_length);
                        if (!cms_converted) {
                            loader_trace_message(
                                "builtin JPEG: embedded ICC conversion failed");
                        }
                    }
                }
                frame->pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
                frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            } else if (chunk_is_psd(pchunk)) {
                stbi__result_info psd_ri;
                int psd_depth;
                int psd_pixelformat;
                int psd_req_comp;
                int psd_header_bitdepth;
                int psd_custom_decode_mode;
                int psd_skip_icc_conversion;
                int psd_colorspace;
                int psd_cmyk_icc_applied;
                unsigned char const *psd_icc_profile;
                size_t psd_icc_profile_length;
                sixel_builtin_psd_info_t psd_info;
                int psd_info_ok;

                psd_ri = (stbi__result_info){ 0 };
                psd_depth = 0;
                psd_pixelformat = SIXEL_PIXELFORMAT_RGB888;
                psd_req_comp = 3;
                psd_custom_decode_mode = 0;
                psd_skip_icc_conversion = 0;
                psd_colorspace = SIXEL_COLORSPACE_GAMMA;
                psd_cmyk_icc_applied = 0;
                psd_icc_profile = NULL;
                psd_icc_profile_length = 0u;
                psd_icc_status = SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
                psd_info_ok = sixel_builtin_parse_psd_info(pchunk, &psd_info);
                if (!psd_info_ok) {
                    sixel_helper_set_additional_message(
                        "builtin PSD: malformed section length/offset");
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                if (psd_info.version != 1u) {
                    sixel_helper_set_additional_message(
                        "builtin PSD: unsupported version (expected 1)");
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                if (psd_info.color_mode == 0u) {
                    psd_custom_decode_mode = 5;
                    if (psd_info.depth != 1u) {
                        nwrite = snprintf(message,
                                          sizeof(message),
                                          "builtin PSD: unsupported bit depth (%u)",
                                          psd_info.depth);
                        if (nwrite > 0) {
                            sixel_helper_set_additional_message(message);
                        }
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_info.channels < 1u) {
                        sixel_helper_set_additional_message(
                            "builtin PSD: Bitmap requires at least 1 channel");
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                } else if (psd_info.color_mode == 3u) {
                    if (psd_info.depth != 8u && psd_info.depth != 16u) {
                        nwrite = snprintf(message,
                                          sizeof(message),
                                          "builtin PSD: unsupported bit depth (%u)",
                                          psd_info.depth);
                        if (nwrite > 0) {
                            sixel_helper_set_additional_message(message);
                        }
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_info.depth == 8u && psd_info.channels < 3u) {
                        sixel_helper_set_additional_message(
                            "builtin PSD: RGB requires at least 3 channels");
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_info.depth == 8u) {
                        psd_custom_decode_mode = 4;
                    }
                } else if (psd_info.color_mode == 1u ||
                           psd_info.color_mode == 2u ||
                           psd_info.color_mode == 8u) {
                    psd_custom_decode_mode = 1;
                    if (psd_info.depth != 8u) {
                        nwrite = snprintf(message,
                                          sizeof(message),
                                          "builtin PSD: unsupported bit depth (%u)",
                                          psd_info.depth);
                        if (nwrite > 0) {
                            sixel_helper_set_additional_message(message);
                        }
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                } else if (psd_info.color_mode == 4u) {
                    psd_custom_decode_mode = 2;
                    if (psd_info.depth != 8u) {
                        nwrite = snprintf(message,
                                          sizeof(message),
                                          "builtin PSD: unsupported bit depth (%u)",
                                          psd_info.depth);
                        if (nwrite > 0) {
                            sixel_helper_set_additional_message(message);
                        }
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_info.channels < 4u) {
                        sixel_helper_set_additional_message(
                            "builtin PSD: CMYK requires at least 4 channels");
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                } else if (psd_info.color_mode == 9u) {
                    psd_custom_decode_mode = 3;
                    if (psd_info.depth != 8u) {
                        nwrite = snprintf(message,
                                          sizeof(message),
                                          "builtin PSD: unsupported bit depth (%u)",
                                          psd_info.depth);
                        if (nwrite > 0) {
                            sixel_helper_set_additional_message(message);
                        }
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_info.channels < 3u) {
                        sixel_helper_set_additional_message(
                            "builtin PSD: Lab requires at least 3 channels");
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                } else {
                    nwrite = snprintf(message,
                                      sizeof(message),
                                      "builtin PSD: unsupported color mode (%u)",
                                      psd_info.color_mode);
                    if (nwrite > 0) {
                        sixel_helper_set_additional_message(message);
                    }
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                if (psd_info.compression > 1u) {
                    nwrite = snprintf(
                        message,
                        sizeof(message),
                        "builtin PSD: unsupported compression (%u)",
                        psd_info.compression);
                    if (nwrite > 0) {
                        sixel_helper_set_additional_message(message);
                    }
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                if (enable_cms) {
                    psd_icc_status = sixel_builtin_extract_psd_icc(
                        pchunk->buffer,
                        pchunk->size,
                        &psd_icc_profile,
                        &psd_icc_profile_length);
                }
                psd_header_bitdepth = (int)psd_info.depth;
                if (psd_custom_decode_mode == 5) {
                    status = sixel_builtin_decode_psd_bitmap_1bit(
                        pchunk,
                        &psd_info,
                        bgcolor,
                        &pixels,
                        &frame->width,
                        &frame->height,
                        &psd_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    psd_depth = 3;
                } else if (psd_custom_decode_mode == 1) {
                    status = sixel_builtin_decode_psd_gray_or_indexed_8bit(
                        pchunk,
                        &psd_info,
                        bgcolor,
                        &pixels,
                        &frame->width,
                        &frame->height,
                        &psd_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    psd_depth = 3;
                } else if (psd_custom_decode_mode == 2) {
                    status = sixel_builtin_decode_psd_cmyk_8bit(
                        pchunk,
                        &psd_info,
                        bgcolor,
                        psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_FOUND
                            ? psd_icc_profile
                            : NULL,
                        psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_FOUND
                            ? psd_icc_profile_length
                            : 0u,
                        &psd_cmyk_icc_applied,
                        &pixels,
                        &frame->width,
                        &frame->height,
                        &psd_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    psd_depth = 3;
                    psd_skip_icc_conversion = 1;
                } else if (psd_custom_decode_mode == 3) {
                    status = sixel_builtin_decode_psd_lab_8bit(
                        pchunk,
                        &psd_info,
                        bgcolor,
                        &pixels,
                        &frame->width,
                        &frame->height,
                        &psd_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    psd_depth = 3;
                    psd_skip_icc_conversion = 1;
                    psd_colorspace = SIXEL_COLORSPACE_CIELAB;
                } else if (psd_custom_decode_mode == 4) {
                    status = sixel_builtin_decode_psd_rgb_8bit(
                        pchunk,
                        &psd_info,
                        bgcolor,
                        &pixels,
                        &frame->width,
                        &frame->height,
                        &psd_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    psd_depth = 3;
                } else {
                    if (psd_header_bitdepth == 16) {
                        /*
                         * Keep the existing 16-bpc path in RGB so promotion to
                         * float32 remains lossless and compatible.
                         */
                        psd_req_comp = 3;
                    } else if (bgcolor != NULL && psd_info.channels >= 4u) {
                        /*
                         * Preserve alpha only when a background color was
                         * requested and the PSD actually provides alpha.
                         * For 3-channel PSD, forcing req_comp=4 injects opaque
                         * alpha and can trigger false CMS failure traces.
                         */
                        psd_req_comp = 4;
                    } else {
                        psd_req_comp = 3;
                    }

                    pixels = (unsigned char *)stbi__load_main(&stb_context,
                                                              &frame->width,
                                                              &frame->height,
                                                              &psd_depth,
                                                              psd_req_comp,
                                                              &psd_ri,
                                                              16);
                    if (pixels == NULL) {
                        sixel_helper_set_additional_message(stbi_failure_reason());
                        status = SIXEL_STBI_ERROR;
                        goto end;
                    }
                    if (psd_header_bitdepth == 16 &&
                        psd_ri.bits_per_channel != 16) {
                        loader_trace_message(
                            "builtin PSD: 16-bpc source decoded as 8-bpc "
                            "fallback path");
                    }

                    if (psd_ri.bits_per_channel == 16) {
                        status = sixel_builtin_promote_rgb16_to_float32(
                            &pixels,
                            frame->width,
                            frame->height,
                            pchunk->allocator);
                        if (SIXEL_FAILED(status)) {
                            sixel_allocator_free(pchunk->allocator, pixels);
                            pixels = NULL;
                            goto end;
                        }
                        psd_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
                    } else {
                        switch (psd_depth) {
                        case 1:
                        case 3:
                        case 4:
                            if (psd_req_comp == 4) {
                                psd_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                            } else {
                                psd_pixelformat = SIXEL_PIXELFORMAT_RGB888;
                            }
                            break;
                        default:
                            nwrite = snprintf(message,
                                              sizeof(message),
                                              "load_with_builtin() failed.\n"
                                              "reason: unknown pixel-format.(depth: %d)\n",
                                              psd_depth);
                            if (nwrite > 0) {
                                sixel_helper_set_additional_message(message);
                            }
                            sixel_allocator_free(pchunk->allocator, pixels);
                            pixels = NULL;
                            status = SIXEL_STBI_ERROR;
                            goto end;
                        }
                    }
                }

                if (enable_cms) {
                    if (psd_skip_icc_conversion) {
                        if (psd_icc_status ==
                            SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
                            loader_trace_message(
                                "builtin PSD: malformed ICC resource section; "
                                "skipping ICC conversion");
                        } else if (psd_custom_decode_mode == 2 &&
                                   psd_icc_status ==
                                       SIXEL_BUILTIN_ICC_EXTRACT_FOUND &&
                                   !psd_cmyk_icc_applied) {
                            loader_trace_message(
                                "builtin PSD: embedded ICC conversion failed");
                        } else if (psd_custom_decode_mode == 3 &&
                                   psd_icc_status ==
                                       SIXEL_BUILTIN_ICC_EXTRACT_FOUND) {
                            loader_trace_message(
                                "builtin PSD: skipping embedded ICC conversion "
                                "for Lab custom decode path");
                        }
                    } else {
                        if (psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_FOUND) {
                            cms_converted =
                                sixel_cms_convert_to_srgb_with_profile_bytes(
                                pixels,
                                frame->width,
                                frame->height,
                                psd_pixelformat,
                                psd_icc_profile,
                                psd_icc_profile_length);
                            if (!cms_converted) {
                                loader_trace_message(
                                    "builtin PSD: embedded ICC conversion failed");
                            }
                        } else if (psd_icc_status ==
                                   SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
                            loader_trace_message(
                                "builtin PSD: malformed ICC resource section; "
                                "skipping ICC conversion");
                        }
                    }
                }

                sixel_frame_set_pixels(frame, pixels);
                frame->loop_count = 1;
                frame->pixelformat = psd_pixelformat;
                frame->colorspace = psd_colorspace;
            } else {
                pixels = stbi__load_and_postprocess_8bit(&stb_context,
                                                         &frame->width,
                                                         &frame->height,
                                                         &depth,
                                                         3);
                if (pixels == NULL) {
                    sixel_helper_set_additional_message(stbi_failure_reason());
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
                sixel_frame_set_pixels(frame, pixels);
                frame->loop_count = 1;
#if HAVE_LCMS2
                if (enable_cms && chunk_is_tiff(pchunk)) {
                    if (sixel_builtin_extract_tiff_icc(
                            pchunk->buffer,
                            pchunk->size,
                            &icc_profile,
                            &icc_profile_length,
                            &tiff_photometric,
                            pchunk->allocator)) {
                        if (sixel_builtin_tiff_photometric_supports_icc(
                                tiff_photometric)) {
                            cms_converted =
                                sixel_cms_convert_to_srgb_with_profile_bytes(
                                pixels,
                                frame->width,
                                frame->height,
                                SIXEL_PIXELFORMAT_RGB888,
                                icc_profile,
                                icc_profile_length);
                            if (!cms_converted) {
                                loader_trace_message(
                                    "builtin TIFF: embedded ICC conversion "
                                    "failed");
                            }
                        }
                    }
                }
#endif
                switch (depth) {
                case 1:
                case 3:
                case 4:
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
                    break;
                default:
                    nwrite = snprintf(message,
                                      sizeof(message),
                                      "load_with_builtin() failed.\n"
                                      "reason: unknown pixel-format.(depth: %d)\n",
                                      depth);
                    if (nwrite > 0) {
                        sixel_helper_set_additional_message(message);
                    }
                    status = SIXEL_STBI_ERROR;
                    goto end;
                }
            }
        }
    }

done:
    if (!frame->alpha_zero_is_transparent) {
        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (icc_profile != NULL) {
        sixel_allocator_free(pchunk->allocator, icc_profile);
    }
    sixel_frame_unref(frame);

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
