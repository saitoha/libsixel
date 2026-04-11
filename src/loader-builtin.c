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
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "compat_stub.h"
#include "frame.h"
#include "fromgif.h"
#include "frombmp.h"
#include "fromhdr.h"
#include "frompng.h"
#include "frompsd.h"
#include "frompnm.h"
#include "pixelformat.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader.h"
#include "sixel_atomic.h"
#include "threading.h"

#if defined(_MSC_VER)
# define SIXEL_STBI_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__STDC_NO_THREADS__) \
    && !defined(__PCC__)
# define SIXEL_STBI_TLS _Thread_local
#elif defined(__GNUC__) && !defined(__PCC__)
# define SIXEL_STBI_TLS __thread
#else
# define SIXEL_STBI_TLS
#endif

static SIXEL_STBI_TLS sixel_allocator_t *stbi_allocator;

#undef SIXEL_STBI_TLS

#if SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32)
/*
 * pcc builds currently avoid TLS qualifiers.  stb_image integration keeps
 * allocator and error-state pointers in process globals in that mode, so we
 * serialize builtin loader decode sections to preserve allocator integrity.
 */
static pthread_mutex_t sixel_loader_builtin_decode_lock;
static pthread_once_t sixel_loader_builtin_decode_lock_once
    = PTHREAD_ONCE_INIT;
static int sixel_loader_builtin_decode_lock_ready = 0;

#if !defined(PTHREAD_MUTEX_RECURSIVE) && defined(PTHREAD_MUTEX_RECURSIVE_NP)
# define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

static void
sixel_loader_builtin_decode_lock_init_once(void)
{
    pthread_mutexattr_t attr;
    int rc;

    rc = pthread_mutexattr_init(&attr);
    if (rc != 0) {
        return;
    }
    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (rc == 0) {
        rc = pthread_mutex_init(&sixel_loader_builtin_decode_lock, &attr);
    }
    pthread_mutexattr_destroy(&attr);
    if (rc == 0) {
        sixel_loader_builtin_decode_lock_ready = 1;
    }
}

static int
sixel_loader_builtin_lock_acquire(void)
{
    pthread_once(&sixel_loader_builtin_decode_lock_once,
                 sixel_loader_builtin_decode_lock_init_once);
    if (!sixel_loader_builtin_decode_lock_ready) {
        return 0;
    }
    if (pthread_mutex_lock(&sixel_loader_builtin_decode_lock) != 0) {
        return 0;
    }
    return 1;
}

static void
sixel_loader_builtin_lock_release(int acquired)
{
    if (acquired != 0 && sixel_loader_builtin_decode_lock_ready) {
        pthread_mutex_unlock(&sixel_loader_builtin_decode_lock);
    }
}
#endif  /* SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32) */

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
    int cms_engine;
    int bmp_info40_mode;
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
#if defined(_WIN32) || defined(__PCC__)
# define STBI_NO_THREAD_LOCALS 1  /* no tls */
#endif
#define STBI_NO_GIF
#define STBI_NO_PNM
#define STBI_NO_PSD
#define STBI_NO_BMP
/*
 * Keep HDR decode behavior deterministic in fromhdr.c and disable the
 * stb_image HDR path.
 */
#define STBI_NO_HDR
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
    if (chunk == NULL || chunk->buffer == NULL) {
        return 0;
    }
    if (chunk->size < 4u) {
        return 0;
    }
    if (memcmp(chunk->buffer, "8BPS", 4u) == 0 ||
        memcmp(chunk->buffer, "8BPB", 4u) == 0) {
        return 1;
    }
    return 0;
}

static int
chunk_is_pic(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 92u) {
        return 0;
    }
    if (memcmp(chunk->buffer, "\x53\x80\xf6\x34", 4u) != 0) {
        return 0;
    }
    if (memcmp(chunk->buffer + 88u, "PICT", 4u) != 0) {
        return 0;
    }
    return 1;
}

static int
chunk_is_sixel(sixel_chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 3u) {
        return 0;
    }

    p = chunk->buffer;
    end = p + chunk->size;

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                p++;
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                p++;
                continue;
            }
            p++;
        }
    }
    return 0;
}

static int
chunk_is_pnm(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 3u) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '7' &&
        isspace((unsigned char)chunk->buffer[2])) {
        return 1;
    }
    return 0;
}

static SIXELSTATUS
convert_palette_to_rgb(
    unsigned char **ppalette_rgb,
    unsigned char **ppalette,
    int palette_colors,
    int palette_comp,
    unsigned char *bgcolor,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *rgb_palette;
    unsigned char *palette_src;
    int i;
    int bg_r;
    int bg_g;
    int bg_b;
    unsigned char gray;
    int alpha;

    status = SIXEL_FALSE;
    rgb_palette = NULL;
    palette_src = NULL;
    i = 0;
    bg_r = 0;
    bg_g = 0;
    bg_b = 0;
    gray = 0;
    alpha = 0;

    if (ppalette_rgb == NULL ||
        ppalette == NULL ||
        *ppalette == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette_colors <= 0 ||
        (palette_comp != 1 && palette_comp != 3 && palette_comp != 4)) {
        return SIXEL_BAD_INPUT;
    }
    palette_src = *ppalette;

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
            alpha = palette_src[i * 4 + 3];
            if (alpha < 0xff) {
                /*
                 * Keep integer blending consistent with the libpng loader.
                 */
                rgb_palette[i * 3 + 0] =
                    (unsigned char)(((0xff - alpha) * bg_r
                                     + alpha * palette_src[i * 4 + 0]) >> 8);
                rgb_palette[i * 3 + 1] =
                    (unsigned char)(((0xff - alpha) * bg_g
                                     + alpha * palette_src[i * 4 + 1]) >> 8);
                rgb_palette[i * 3 + 2] =
                    (unsigned char)(((0xff - alpha) * bg_b
                                     + alpha * palette_src[i * 4 + 2]) >> 8);
            } else {
                rgb_palette[i * 3 + 0] = palette_src[i * 4 + 0];
                rgb_palette[i * 3 + 1] = palette_src[i * 4 + 1];
                rgb_palette[i * 3 + 2] = palette_src[i * 4 + 2];
            }
        } else if (palette_comp == 3) {
            rgb_palette[i * 3 + 0] = palette_src[i * 3 + 0];
            rgb_palette[i * 3 + 1] = palette_src[i * 3 + 1];
            rgb_palette[i * 3 + 2] = palette_src[i * 3 + 2];
        } else {
            gray = palette_src[i];
            rgb_palette[i * 3 + 0] = gray;
            rgb_palette[i * 3 + 1] = gray;
            rgb_palette[i * 3 + 2] = gray;
        }
    }

    stbi_free(palette_src);
    *ppalette = NULL;
    *ppalette_rgb = rgb_palette;
    status = SIXEL_OK;

    return status;
}

static unsigned char
sixel_builtin_blend_channel_with_bg(
    unsigned int channel,
    unsigned int alpha,
    unsigned int bg_channel)
{
    return (unsigned char)((channel * alpha
                            + bg_channel * (0xffu - alpha)) >> 8);
}

static float
sixel_builtin_decode_srgb_unit(float gamma_value)
{
    if (!(gamma_value > 0.0f)) {
        return 0.0f;
    }
    if (gamma_value >= 1.0f) {
        return 1.0f;
    }
    if (gamma_value <= 0.04045f) {
        return gamma_value / 12.92f;
    }
    return powf((gamma_value + 0.055f) / 1.055f, 2.4f);
}

static void
sixel_builtin_fill_linear_bgcolor(float bg_linear[3],
                                  unsigned char const *bgcolor)
{
    int channel;
    int bg_colorspace;
    float gamma_value;

    channel = 0;
    bg_colorspace = SIXEL_COLORSPACE_GAMMA;
    gamma_value = 0.0f;
    if (bg_linear == NULL || bgcolor == NULL) {
        return;
    }

    bg_colorspace = loader_background_colorspace();
    for (channel = 0; channel < 3; ++channel) {
        if (bg_colorspace == SIXEL_COLORSPACE_LINEAR) {
            bg_linear[channel] = (float)bgcolor[channel] / 255.0f;
        } else {
            gamma_value = (float)bgcolor[channel] / 255.0f;
            bg_linear[channel] = sixel_builtin_decode_srgb_unit(gamma_value);
        }
    }
}

static SIXELSTATUS
sixel_builtin_apply_bmp_alpha_policy(
    sixel_frame_t *frame,
    unsigned char *bgcolor)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *transparent_mask;
    float *float_pixels;
    float bg_linear[3];
    size_t pixel_count;
    size_t float_count;
    size_t float_size;
    size_t index;
    unsigned int alpha_u8;
    unsigned int r;
    unsigned int g;
    unsigned int b;
    float alpha_unit;
    float inv_alpha;
    int channel;
    int has_zero_alpha;
    float src_gamma;

    status = SIXEL_FALSE;
    pixels = NULL;
    transparent_mask = NULL;
    float_pixels = NULL;
    memset(bg_linear, 0, sizeof(bg_linear));
    pixel_count = 0u;
    float_count = 0u;
    float_size = 0u;
    index = 0u;
    alpha_u8 = 0u;
    r = 0u;
    g = 0u;
    b = 0u;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    channel = 0;
    has_zero_alpha = 0;
    src_gamma = 0.0f;
    if (frame == NULL ||
        frame->allocator == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->width <= 0 ||
        frame->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    pixels = frame->pixels.u8ptr;

    if (bgcolor != NULL) {
        if (pixel_count > SIZE_MAX / 3u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        float_count = pixel_count * 3u;
        if (float_count > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        float_size = float_count * sizeof(float);
        float_pixels = (float *)sixel_allocator_malloc(frame->allocator,
                                                       float_size);
        if (float_pixels == NULL) {
            sixel_helper_set_additional_message(
                "builtin BMP: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        sixel_builtin_fill_linear_bgcolor(bg_linear, bgcolor);

        for (index = 0u; index < pixel_count; ++index) {
            alpha_unit = (float)pixels[index * 4u + 3u] / 255.0f;
            inv_alpha = 1.0f - alpha_unit;
            for (channel = 0; channel < 3; ++channel) {
                src_gamma = (float)pixels[index * 4u + (size_t)channel]
                    / 255.0f;
                float_pixels[index * 3u + (size_t)channel] =
                    sixel_builtin_decode_srgb_unit(src_gamma) * alpha_unit
                    + bg_linear[channel] * inv_alpha;
            }
        }

        if (frame->transparent_mask != NULL) {
            sixel_allocator_free(frame->allocator, frame->transparent_mask);
            frame->transparent_mask = NULL;
            frame->transparent_mask_size = 0u;
        }
        sixel_allocator_free(frame->allocator, pixels);
        sixel_frame_set_pixels_float32(frame, float_pixels);
        float_pixels = NULL;
        frame->transparent = -1;
        frame->alpha_zero_is_transparent = 0;
        frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        frame->colorspace = SIXEL_COLORSPACE_LINEAR;
        status = SIXEL_OK;
        goto end;
    }

    transparent_mask = (unsigned char *)sixel_allocator_malloc(
        frame->allocator,
        pixel_count);
    if (transparent_mask == NULL) {
        sixel_helper_set_additional_message(
            "builtin BMP: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < pixel_count; ++index) {
        alpha_u8 = pixels[index * 4u + 3u];
        transparent_mask[index] = alpha_u8 == 0u ? 1u : 0u;
        if (alpha_u8 == 0u) {
            has_zero_alpha = 1;
        }
        if (alpha_u8 < 0xffu) {
            r = sixel_builtin_blend_channel_with_bg(pixels[index * 4u + 0u],
                                                    alpha_u8,
                                                    0u);
            g = sixel_builtin_blend_channel_with_bg(pixels[index * 4u + 1u],
                                                    alpha_u8,
                                                    0u);
            b = sixel_builtin_blend_channel_with_bg(pixels[index * 4u + 2u],
                                                    alpha_u8,
                                                    0u);
        } else {
            r = pixels[index * 4u + 0u];
            g = pixels[index * 4u + 1u];
            b = pixels[index * 4u + 2u];
        }
        pixels[index * 3u + 0u] = (unsigned char)r;
        pixels[index * 3u + 1u] = (unsigned char)g;
        pixels[index * 3u + 2u] = (unsigned char)b;
    }

    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    if (has_zero_alpha != 0) {
        frame->transparent_mask = transparent_mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        transparent_mask = NULL;
    } else {
        frame->alpha_zero_is_transparent = 0;
    }
    frame->transparent = -1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    status = SIXEL_OK;

end:
    sixel_allocator_free(frame->allocator, transparent_mask);
    sixel_allocator_free(frame->allocator, float_pixels);
    return status;
}

static SIXELSTATUS
sixel_builtin_apply_bmp_png16_no_bg_policy(
    sixel_frame_t *frame,
    uint16_t const *pixels16)
{
    SIXELSTATUS status;
    float *float_pixels;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t float_count;
    size_t float_size;
    size_t index;
    float alpha_unit;
    int has_zero_alpha;

    status = SIXEL_FALSE;
    float_pixels = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    float_count = 0u;
    float_size = 0u;
    index = 0u;
    alpha_unit = 0.0f;
    has_zero_alpha = 0;
    if (frame == NULL ||
        frame->allocator == NULL ||
        pixels16 == NULL ||
        frame->width <= 0 ||
        frame->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_count = pixel_count * 3u;
    if (float_count > SIZE_MAX / sizeof(float)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_size = float_count * sizeof(float);
    float_pixels = (float *)sixel_allocator_malloc(frame->allocator,
                                                   float_size);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "builtin BMP: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    transparent_mask = (unsigned char *)sixel_allocator_malloc(
        frame->allocator,
        pixel_count);
    if (transparent_mask == NULL) {
        sixel_helper_set_additional_message(
            "builtin BMP: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Keep the no-bg policy semantics: collapse partial alpha against black,
     * but preserve fully transparent pixels in a side-channel mask.
     */
    for (index = 0u; index < pixel_count; ++index) {
        alpha_unit = (float)pixels16[index * 4u + 3u] / 65535.0f;
        if (pixels16[index * 4u + 3u] == 0u) {
            transparent_mask[index] = 1u;
            has_zero_alpha = 1;
        } else {
            transparent_mask[index] = 0u;
        }
        float_pixels[index * 3u + 0u] =
            ((float)pixels16[index * 4u + 0u] / 65535.0f) * alpha_unit;
        float_pixels[index * 3u + 1u] =
            ((float)pixels16[index * 4u + 1u] / 65535.0f) * alpha_unit;
        float_pixels[index * 3u + 2u] =
            ((float)pixels16[index * 4u + 2u] / 65535.0f) * alpha_unit;
    }

    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    sixel_frame_set_pixels_float32(frame, float_pixels);
    float_pixels = NULL;
    if (has_zero_alpha != 0) {
        frame->transparent_mask = transparent_mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        transparent_mask = NULL;
    } else {
        frame->alpha_zero_is_transparent = 0;
    }
    frame->transparent = -1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    status = SIXEL_OK;

end:
    sixel_allocator_free(frame->allocator, transparent_mask);
    sixel_allocator_free(frame->allocator, float_pixels);
    return status;
}

static int
sixel_builtin_apply_bmp_icc_to_rgba_channels(
    sixel_frame_t *frame,
    unsigned char const *icc_profile,
    size_t icc_profile_length)
{
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_size;
    size_t index;
    int converted;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_size = 0u;
    index = 0u;
    converted = 0;
    if (frame == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->allocator == NULL ||
        frame->width <= 0 ||
        frame->height <= 0 ||
        icc_profile == NULL ||
        icc_profile_length == 0u) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    rgb_size = pixel_count * 3u;
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                         rgb_size);
    if (rgb_pixels == NULL) {
        loader_trace_message(
            "builtin BMP: skipping embedded ICC conversion "
            "(temporary RGB allocation failed)");
        return 0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        rgb_pixels[index * 3u + 0u] = frame->pixels.u8ptr[index * 4u + 0u];
        rgb_pixels[index * 3u + 1u] = frame->pixels.u8ptr[index * 4u + 1u];
        rgb_pixels[index * 3u + 2u] = frame->pixels.u8ptr[index * 4u + 2u];
    }

    converted = sixel_cms_convert_to_srgb_with_profile_bytes(
        rgb_pixels,
        frame->width,
        frame->height,
        SIXEL_PIXELFORMAT_RGB888,
        icc_profile,
        icc_profile_length);
    if (!converted) {
        loader_trace_message("builtin BMP: embedded ICC conversion failed");
        sixel_allocator_free(frame->allocator, rgb_pixels);
        return 0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        frame->pixels.u8ptr[index * 4u + 0u] = rgb_pixels[index * 3u + 0u];
        frame->pixels.u8ptr[index * 4u + 1u] = rgb_pixels[index * 3u + 1u];
        frame->pixels.u8ptr[index * 4u + 2u] = rgb_pixels[index * 3u + 2u];
    }

    sixel_allocator_free(frame->allocator, rgb_pixels);
    return 1;
}

static int
sixel_builtin_apply_bmp_calibrated_rgb_to_rgb8(
    unsigned char *rgb_pixels,
    int width,
    int height,
    sixel_frombmp_probe_t const *bmp_probe)
{
    sixel_cms_profile_t *src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (rgb_pixels == NULL ||
        width <= 0 ||
        height <= 0 ||
        bmp_probe == NULL ||
        bmp_probe->has_calibrated_rgb == 0) {
        return 0;
    }

    src_profile = sixel_cms_create_rgb_profile_from_gammas_chrm(
        bmp_probe->calibrated_gamma_r,
        bmp_probe->calibrated_gamma_g,
        bmp_probe->calibrated_gamma_b,
        bmp_probe->white_x,
        bmp_probe->white_y,
        bmp_probe->red_x,
        bmp_probe->red_y,
        bmp_probe->green_x,
        bmp_probe->green_y,
        bmp_probe->blue_x,
        bmp_probe->blue_y);
    if (src_profile == NULL) {
        src_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(
            bmp_probe->calibrated_gamma,
            bmp_probe->white_x,
            bmp_probe->white_y,
            bmp_probe->red_x,
            bmp_probe->red_y,
            bmp_probe->green_x,
            bmp_probe->green_y,
            bmp_probe->blue_x,
            bmp_probe->blue_y);
    }
    if (src_profile == NULL) {
        loader_trace_message(
            "builtin BMP: calibrated profile creation failed");
        return 0;
    }

    converted = sixel_cms_convert_profile_to_srgb(
        rgb_pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        src_profile);
    sixel_cms_close_profile(src_profile);
    if (!converted) {
        loader_trace_message(
            "builtin BMP: calibrated RGB conversion failed");
    }
    return converted;
}

static int
sixel_builtin_apply_bmp_calibrated_to_rgba_channels(
    sixel_frame_t *frame,
    sixel_frombmp_probe_t const *bmp_probe)
{
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_size;
    size_t index;
    int converted;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_size = 0u;
    index = 0u;
    converted = 0;
    if (frame == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->allocator == NULL ||
        frame->width <= 0 ||
        frame->height <= 0 ||
        bmp_probe == NULL ||
        bmp_probe->has_calibrated_rgb == 0) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    rgb_size = pixel_count * 3u;
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                         rgb_size);
    if (rgb_pixels == NULL) {
        loader_trace_message(
            "builtin BMP: skipping calibrated conversion "
            "(temporary RGB allocation failed)");
        return 0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        rgb_pixels[index * 3u + 0u] = frame->pixels.u8ptr[index * 4u + 0u];
        rgb_pixels[index * 3u + 1u] = frame->pixels.u8ptr[index * 4u + 1u];
        rgb_pixels[index * 3u + 2u] = frame->pixels.u8ptr[index * 4u + 2u];
    }
    converted = sixel_builtin_apply_bmp_calibrated_rgb_to_rgb8(
        rgb_pixels,
        frame->width,
        frame->height,
        bmp_probe);
    if (!converted) {
        sixel_allocator_free(frame->allocator, rgb_pixels);
        return 0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        frame->pixels.u8ptr[index * 4u + 0u] = rgb_pixels[index * 3u + 0u];
        frame->pixels.u8ptr[index * 4u + 1u] = rgb_pixels[index * 3u + 1u];
        frame->pixels.u8ptr[index * 4u + 2u] = rgb_pixels[index * 3u + 2u];
    }
    sixel_allocator_free(frame->allocator, rgb_pixels);
    return 1;
}

static void
sixel_builtin_bmp_convert_cmyk8_to_rgb8_device(unsigned char *rgb_pixels,
                                                unsigned char const
                                                    *cmyk_pixels,
                                                size_t pixel_count)
{
    size_t index;
    unsigned int c;
    unsigned int m;
    unsigned int y;
    unsigned int k;
    unsigned int c_term;
    unsigned int m_term;
    unsigned int y_term;

    index = 0u;
    c = 0u;
    m = 0u;
    y = 0u;
    k = 0u;
    c_term = 0u;
    m_term = 0u;
    y_term = 0u;
    if (rgb_pixels == NULL || cmyk_pixels == NULL) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        c = (unsigned int)cmyk_pixels[index * 4u + 0u];
        m = (unsigned int)cmyk_pixels[index * 4u + 1u];
        y = (unsigned int)cmyk_pixels[index * 4u + 2u];
        k = (unsigned int)cmyk_pixels[index * 4u + 3u];
        c_term = (255u - c) * (255u - k);
        m_term = (255u - m) * (255u - k);
        y_term = (255u - y) * (255u - k);
        rgb_pixels[index * 3u + 0u] = (unsigned char)((c_term + 127u) / 255u);
        rgb_pixels[index * 3u + 1u] = (unsigned char)((m_term + 127u) / 255u);
        rgb_pixels[index * 3u + 2u] = (unsigned char)((y_term + 127u) / 255u);
    }
}

static unsigned char
sixel_builtin_cms_clamp_unit_to_u8(float value)
{
    double unit;

    unit = (double)value;
    if (!(unit > 0.0)) {
        return 0u;
    }
    if (unit >= 1.0) {
        return 255u;
    }
    return (unsigned char)(unit * 255.0 + 0.5);
}

static int
sixel_builtin_bmp_convert_cmyk8_to_rgb8_icc(unsigned char *rgb_pixels,
                                             unsigned char const *cmyk_pixels,
                                             size_t pixel_count,
                                             unsigned char const *icc_profile,
                                             size_t icc_profile_length)
{
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    float *rgb_float;
    size_t float_count;
    size_t float_size;
    size_t index;
    int converted;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    rgb_float = NULL;
    float_count = 0u;
    float_size = 0u;
    index = 0u;
    converted = 0;
    if (rgb_pixels == NULL ||
        cmyk_pixels == NULL ||
        pixel_count == 0u ||
        icc_profile == NULL ||
        icc_profile_length == 0u) {
        return 0;
    }
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    float_count = pixel_count * 3u;
    if (float_count > SIZE_MAX / sizeof(float)) {
        return 0;
    }
    float_size = float_count * sizeof(float);
    rgb_float = (float *)malloc(float_size);
    if (rgb_float == NULL) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                   icc_profile_length);
    if (src_profile == NULL) {
        goto cleanup;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_CMYK_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }

    converted = sixel_cms_do_transform(transform,
                                       cmyk_pixels,
                                       rgb_float,
                                       pixel_count);
    if (converted) {
        for (index = 0u; index < pixel_count; ++index) {
            rgb_pixels[index * 3u + 0u] =
                sixel_builtin_cms_clamp_unit_to_u8(
                    rgb_float[index * 3u + 0u]);
            rgb_pixels[index * 3u + 1u] =
                sixel_builtin_cms_clamp_unit_to_u8(
                    rgb_float[index * 3u + 1u]);
            rgb_pixels[index * 3u + 2u] =
                sixel_builtin_cms_clamp_unit_to_u8(
                    rgb_float[index * 3u + 2u]);
        }
    }

cleanup:
    free(rgb_float);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    if (!converted) {
        loader_trace_message("builtin BMP: CMYK ICC conversion failed");
    }
    return converted;
}

static int
sixel_builtin_bmp_has_compressed_payload(unsigned int compression)
{
    return compression == SIXEL_FROMBMP_COMPRESSION_JPEG ||
        compression == SIXEL_FROMBMP_COMPRESSION_PNG;
}

static int
sixel_builtin_apply_bmp_png_colorspace_to_rgba_channels(
    sixel_frame_t *frame,
    unsigned char const *payload,
    size_t payload_size)
{
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_size;
    size_t index;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_size = 0u;
    index = 0u;
    if (frame == NULL ||
        frame->allocator == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->width <= 0 ||
        frame->height <= 0 ||
        payload == NULL ||
        payload_size == 0u) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    rgb_size = pixel_count * 3u;
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                         rgb_size);
    if (rgb_pixels == NULL) {
        loader_trace_message(
            "builtin BMP: skipping embedded PNG colorspace fallback "
            "(temporary RGB allocation failed)");
        return 0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        rgb_pixels[index * 3u + 0u] = frame->pixels.u8ptr[index * 4u + 0u];
        rgb_pixels[index * 3u + 1u] = frame->pixels.u8ptr[index * 4u + 1u];
        rgb_pixels[index * 3u + 2u] = frame->pixels.u8ptr[index * 4u + 2u];
    }

    sixel_frompng_apply_colorspace_fallback(rgb_pixels,
                                            frame->width,
                                            frame->height,
                                            payload,
                                            payload_size,
                                            frame->allocator);
    for (index = 0u; index < pixel_count; ++index) {
        frame->pixels.u8ptr[index * 4u + 0u] = rgb_pixels[index * 3u + 0u];
        frame->pixels.u8ptr[index * 4u + 1u] = rgb_pixels[index * 3u + 1u];
        frame->pixels.u8ptr[index * 4u + 2u] = rgb_pixels[index * 3u + 2u];
    }

    sixel_allocator_free(frame->allocator, rgb_pixels);
    return 1;
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
    static int mode = 2;
    /*
     * mode:
     *   0 -> disabled
     *   1 -> tRNS keycolor only
     *   2 -> tRNS keycolor + alpha-channel keycolor
     *        (default when env is unset)
     */

    if (initialized) {
        return mode;
    }
    initialized = 1;
    mode = 2;

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

typedef struct sixel_builtin_apng_runtime {
    unsigned char const *p;
    size_t remain;
    size_t canvas_bytes;
    int seen_actl;
    int saw_animation;
    int has_frame;
    int source_frame_no;
    int num_frames;
    int num_plays;
    int loop_no;
    int frames_in_loop;
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
} sixel_builtin_apng_runtime_t;

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

/*
 * Emit one compact APNG parser snapshot per chunk so CI crashes can be
 * correlated with parser state right before frame reconstruction starts.
 */
static void
sixel_builtin_apng_trace_chunk_state(
    unsigned char const *chunk,
    uint32_t length,
    sixel_builtin_apng_state_t const *state,
    sixel_builtin_apng_runtime_t const *runtime)
{
    char type[5];
    char const *chunk_type;
    size_t remain;
    uint32_t expected_sequence;
    int has_frame;
    size_t chunk_size;
    size_t shared_size;

    chunk_type = "????";
    remain = 0u;
    expected_sequence = 0u;
    has_frame = 0;
    chunk_size = 0u;
    shared_size = 0u;
    if (chunk != NULL) {
        memcpy(type, chunk + 4, 4);
        type[4] = '\0';
        chunk_type = type;
    }
    if (runtime != NULL) {
        remain = runtime->remain;
        has_frame = runtime->has_frame;
    }
    if (state != NULL) {
        expected_sequence = state->expected_sequence;
        chunk_size = state->chunk_size;
        shared_size = state->shared_chunks_size;
    }

    sixel_trace_topic_message(
        "apng",
        "chunk=%s len=%u remain=%zu expected_seq=%u has_frame=%d "
        "chunk_size=%zu shared_size=%zu",
        chunk_type,
        (unsigned int)length,
        remain,
        expected_sequence,
        has_frame,
        chunk_size,
        shared_size);
}

static int
sixel_builtin_apng_ensure_buffer_capacity(
    unsigned char **buffer,
    size_t current_size,
    size_t *pcapacity,
    size_t append_size,
    size_t initial_capacity,
    sixel_allocator_t *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    next = NULL;
    needed = 0u;
    next_capacity = 0u;
    if (buffer == NULL ||
        pcapacity == NULL ||
        allocator == NULL) {
        return 0;
    }
    if (append_size > SIZE_MAX - current_size) {
        return 0;
    }

    needed = current_size + append_size;
    if (needed <= *pcapacity) {
        return 1;
    }

    next_capacity = *pcapacity;
    if (next_capacity == 0u) {
        next_capacity = initial_capacity;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2u) {
            return 0;
        }
        next_capacity *= 2u;
    }

    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (current_size > 0u && *buffer != NULL) {
        memcpy(next, *buffer, current_size);
    }

    sixel_allocator_free(allocator, *buffer);
    *buffer = next;
    *pcapacity = next_capacity;
    return 1;
}

static int
sixel_builtin_apng_ensure_shared_capacity(
    sixel_builtin_apng_state_t *state,
    size_t append_size,
    sixel_allocator_t *allocator)
{
    if (state == NULL) {
        return 0;
    }
    return sixel_builtin_apng_ensure_buffer_capacity(
        &state->shared_chunks,
        state->shared_chunks_size,
        &state->shared_chunks_capacity,
        append_size,
        1024u,
        allocator);
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
    if (state == NULL) {
        return 0;
    }
    return sixel_builtin_apng_ensure_buffer_capacity(
        &state->chunk_base,
        state->chunk_size,
        &state->chunk_capacity,
        append_size,
        4096u,
        allocator);
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
    char const *stbi_reason;

    status = SIXEL_FALSE;
    frame = NULL;
    stb_context = (stbi__context){ 0 };
    png_data = NULL;
    subframe = NULL;
    emitted = NULL;
    width = 0;
    height = 0;
    depth = 0;
    stbi_reason = NULL;

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

    sixel_trace_topic_message(
        "apng",
        "emit begin frame=%d loop=%d control=%ux%u+%u+%u "
        "dispose=%u blend=%u delay_cs=%u chunk=%zu shared=%zu "
        "canvas=%dx%d alpha0=%d callback=%d",
        frame_no,
        loop_no,
        (unsigned int)control->width,
        (unsigned int)control->height,
        (unsigned int)control->x_offset,
        (unsigned int)control->y_offset,
        control->dispose_op,
        control->blend_op,
        control->delay_cs,
        state->chunk_size,
        state->shared_chunks_size,
        canvas->width,
        canvas->height,
        alpha_zero_is_transparent,
        emit_callback);

    stbi__start_mem(&stb_context, png_data, (int)png_size);
    subframe = stbi__load_and_postprocess_8bit(&stb_context,
                                               &width,
                                               &height,
                                               &depth,
                                               4);
    if (subframe == NULL) {
        stbi_reason = stbi_failure_reason();
        sixel_trace_topic_message(
            "apng",
            "emit decode failed frame=%d loop=%d png_size=%zu reason=%s",
            frame_no,
            loop_no,
            png_size,
            stbi_reason != NULL ? stbi_reason : "(null)");
        sixel_helper_set_additional_message(stbi_reason);
        status = SIXEL_STBI_ERROR;
        goto end;
    }
    sixel_trace_topic_message(
        "apng",
        "emit decode done frame=%d loop=%d decoded=%dx%d depth=%d",
        frame_no,
        loop_no,
        width,
        height,
        depth);
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
    sixel_trace_topic_message(
        "apng",
        "emit callback frame=%d loop=%d status=%s",
        frame_no,
        loop_no,
        sixel_helper_format_error(status));

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

static int
sixel_builtin_apng_should_emit_callback(
    int loop_no,
    int start_frame_no,
    int frames_in_loop)
{
    if (loop_no == 0 &&
        start_frame_no != INT_MIN &&
        frames_in_loop < start_frame_no) {
        return 0;
    }
    return 1;
}

static int
sixel_builtin_apng_resolve_emit_frame_no(
    int loop_no,
    int start_frame_no,
    int source_frame_no)
{
    /*
     * Keep frame numbers aligned with emitted order in the first loop when
     * start-frame skips leading source frames.
     */
    if (loop_no == 0 && start_frame_no != INT_MIN) {
        return source_frame_no - start_frame_no;
    }
    return source_frame_no;
}

static SIXELSTATUS
sixel_builtin_apng_emit_pending_frame(
    sixel_builtin_apng_state_t const *state,
    sixel_builtin_apng_frame_control_t *control,
    int loop_no,
    int start_frame_no,
    int frames_in_loop,
    int source_frame_no,
    int fstatic,
    int num_frames,
    unsigned char *bgcolor,
    int alpha_zero_is_transparent,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_load_image_function fn_load,
    void *context,
    sixel_allocator_t *allocator,
    int *emit_callback_out)
{
    SIXELSTATUS status;
    int emit_callback;
    int emit_frame_no;

    status = SIXEL_FALSE;
    emit_callback = 0;
    emit_frame_no = 0;
    if (state == NULL || control == NULL || canvas == NULL || fn_load == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    emit_callback = sixel_builtin_apng_should_emit_callback(
        loop_no,
        start_frame_no,
        frames_in_loop);
    emit_frame_no = sixel_builtin_apng_resolve_emit_frame_no(
        loop_no,
        start_frame_no,
        source_frame_no);
    status = sixel_builtin_apng_emit_frame(
        state,
        control,
        emit_frame_no,
        loop_no,
        (!fstatic && num_frames > 1),
        emit_callback,
        bgcolor,
        alpha_zero_is_transparent,
        canvas,
        fn_load,
        context,
        allocator);
    if (emit_callback_out != NULL) {
        *emit_callback_out = emit_callback;
    }
    return status;
}

static int
sixel_builtin_apng_should_stop_loop(
    int loop_control,
    int frames_in_loop,
    int loop_no,
    int num_plays)
{
    if (loop_control == SIXEL_LOOP_DISABLE) {
        return 1;
    }
    if (frames_in_loop <= 1) {
        return 1;
    }
    if (loop_control == SIXEL_LOOP_AUTO &&
        num_plays > 0 &&
        loop_no >= num_plays) {
        return 1;
    }
    return 0;
}

static int
sixel_builtin_apng_resolve_alpha_zero_transparent(
    int trns_keycolor_mode,
    unsigned char const *bgcolor,
    int enable_cms,
    int has_trns_chunk,
    int has_alpha_chunk)
{
    if (trns_keycolor_mode == 0 || bgcolor != NULL || enable_cms != 0) {
        return 0;
    }
    if ((has_trns_chunk != 0 && has_alpha_chunk == 0) ||
        (has_alpha_chunk != 0 && trns_keycolor_mode == 2)) {
        return 1;
    }
    return 0;
}

static void
sixel_builtin_apng_release_loop_buffers(
    sixel_builtin_apng_state_t *state,
    sixel_allocator_t *allocator)
{
    if (state == NULL || allocator == NULL) {
        return;
    }
    sixel_allocator_free(allocator, state->shared_chunks);
    sixel_allocator_free(allocator, state->chunk_base);
    state->shared_chunks = NULL;
    state->shared_chunks_size = 0u;
    state->shared_chunks_capacity = 0u;
    state->chunk_base = NULL;
    state->chunk_size = 0u;
    state->chunk_capacity = 0u;
}

static void
sixel_builtin_apng_init_runtime(
    sixel_builtin_apng_runtime_t *runtime,
    int trns_keycolor_mode)
{
    if (runtime == NULL) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->emit_callback = 1;
    runtime->color_type = (-1);
    runtime->trns_keycolor_mode = trns_keycolor_mode;
}

static void
sixel_builtin_apng_begin_loop_iteration(
    sixel_builtin_apng_state_t *state,
    sixel_builtin_apng_frame_control_t *control,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_builtin_apng_runtime_t *runtime,
    sixel_chunk_t const *pchunk)
{
    if (state == NULL ||
        control == NULL ||
        canvas == NULL ||
        runtime == NULL ||
        pchunk == NULL ||
        pchunk->size < 8) {
        return;
    }

    memset(state, 0, sizeof(*state));
    memset(control, 0, sizeof(*control));
    runtime->p = pchunk->buffer + 8;
    runtime->remain = pchunk->size - 8;
    runtime->seen_actl = 0;
    runtime->has_frame = 0;
    runtime->source_frame_no = 0;
    runtime->frames_in_loop = 0;
    runtime->seen_fctl = 0;
    runtime->seen_idat = 0;
    runtime->alpha_zero_is_transparent = 0;
    runtime->color_type = (-1);
    runtime->has_alpha_chunk = 0;
    runtime->has_trns_chunk = 0;
    runtime->emit_callback = 1;
    runtime->length = 0u;
    runtime->sequence_no = 0u;
    runtime->fd_sequence = 0u;
    if (runtime->loop_no > 0 && runtime->canvas_bytes > 0u) {
        memset(canvas->pixels, 0, runtime->canvas_bytes);
        memset(canvas->backup, 0, runtime->canvas_bytes);
    }
    sixel_trace_topic_message(
        "apng",
        "loop begin loop=%d remain=%zu trns_mode=%d",
        runtime->loop_no,
        runtime->remain,
        runtime->trns_keycolor_mode);
}

static SIXELSTATUS
sixel_builtin_apng_prepare_canvas_from_ihdr(
    sixel_chunk_t const *pchunk,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_builtin_apng_runtime_t *runtime)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (pchunk == NULL || canvas == NULL || runtime == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (runtime->canvas_bytes != 0u) {
        return SIXEL_OK;
    }

    runtime->canvas_width = sixel_builtin_read_be32(runtime->p + 8);
    runtime->canvas_height = sixel_builtin_read_be32(runtime->p + 12);
    if (runtime->canvas_width == 0u ||
        runtime->canvas_height == 0u ||
        runtime->canvas_width > INT_MAX ||
        runtime->canvas_height > INT_MAX) {
        return SIXEL_BAD_INPUT;
    }

    canvas->width = (int)runtime->canvas_width;
    canvas->height = (int)runtime->canvas_height;
    runtime->canvas_bytes = (size_t)canvas->width * (size_t)canvas->height * 4u;
    canvas->pixels = (unsigned char *)sixel_allocator_malloc(
        pchunk->allocator,
        runtime->canvas_bytes);
    canvas->backup = (unsigned char *)sixel_allocator_malloc(
        pchunk->allocator,
        runtime->canvas_bytes);
    if (canvas->pixels == NULL || canvas->backup == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        return status;
    }
    memset(canvas->pixels, 0, runtime->canvas_bytes);
    memset(canvas->backup, 0, runtime->canvas_bytes);
    return status;
}

static SIXELSTATUS
sixel_builtin_apng_flush_pending_frame(
    sixel_builtin_apng_state_t *state,
    sixel_builtin_apng_frame_control_t *control,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_builtin_apng_runtime_t *runtime,
    int start_frame_no,
    int fstatic,
    unsigned char *bgcolor,
    sixel_load_image_function fn_load,
    void *context,
    sixel_allocator_t *allocator,
    int *stop_after_emit)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (stop_after_emit != NULL) {
        *stop_after_emit = 0;
    }
    if (state == NULL ||
        control == NULL ||
        canvas == NULL ||
        runtime == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (state->chunk_size == 0u) {
        return SIXEL_OK;
    }

    status = sixel_builtin_apng_emit_pending_frame(
        state,
        control,
        runtime->loop_no,
        start_frame_no,
        runtime->frames_in_loop,
        runtime->source_frame_no,
        fstatic,
        runtime->num_frames,
        bgcolor,
        runtime->alpha_zero_is_transparent,
        canvas,
        fn_load,
        context,
        allocator,
        &runtime->emit_callback);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    ++runtime->source_frame_no;
    ++runtime->frames_in_loop;
    state->chunk_size = 0u;
    sixel_trace_topic_message(
        "apng",
        "flush done loop=%d source_frame=%d emitted_frames=%d",
        runtime->loop_no,
        runtime->source_frame_no,
        runtime->frames_in_loop);
    if (fstatic && runtime->emit_callback != 0 && stop_after_emit != NULL) {
        *stop_after_emit = 1;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_apng_process_chunk(
    sixel_chunk_t const *pchunk,
    sixel_builtin_apng_state_t *state,
    sixel_builtin_apng_frame_control_t *control,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_builtin_apng_runtime_t *runtime,
    unsigned char *bgcolor,
    int enable_cms,
    int fstatic,
    int *start_frame_no,
    sixel_load_image_function fn_load,
    void *context,
    int *stop_decode,
    int *stop_scan)
{
    SIXELSTATUS status;
    int stop_after_emit;

    status = SIXEL_OK;
    stop_after_emit = 0;
    if (stop_decode != NULL) {
        *stop_decode = 0;
    }
    if (stop_scan != NULL) {
        *stop_scan = 0;
    }
    if (pchunk == NULL ||
        state == NULL ||
        control == NULL ||
        canvas == NULL ||
        runtime == NULL ||
        start_frame_no == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    runtime->length = sixel_builtin_read_be32(runtime->p);
    if ((size_t)runtime->length > runtime->remain - 12u) {
        return SIXEL_BAD_INPUT;
    }
    sixel_builtin_apng_trace_chunk_state(runtime->p,
                                         runtime->length,
                                         state,
                                         runtime);

    if (memcmp(runtime->p + 4, "IHDR", 4) == 0) {
        if (runtime->length != 13u) {
            return SIXEL_BAD_INPUT;
        }
        state->ihdr = runtime->p + 8;
        state->ihdr_size = runtime->length;
        runtime->color_type = (int)runtime->p[17];
        runtime->has_alpha_chunk =
            (runtime->color_type & SIXEL_BUILTIN_PNG_COLOR_MASK_ALPHA) != 0
                ? 1
                : 0;
        status = sixel_builtin_apng_prepare_canvas_from_ihdr(pchunk,
                                                             canvas,
                                                             runtime);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        runtime->alpha_zero_is_transparent =
            sixel_builtin_apng_resolve_alpha_zero_transparent(
                runtime->trns_keycolor_mode,
                bgcolor,
                enable_cms,
                runtime->has_trns_chunk,
                runtime->has_alpha_chunk);
    } else if (memcmp(runtime->p + 4, "acTL", 4) == 0) {
        if (runtime->length != 8u) {
            return SIXEL_BAD_INPUT;
        }
        runtime->seen_actl = 1;
        runtime->saw_animation = 1;
        runtime->num_frames = (int)sixel_builtin_read_be32(runtime->p + 8);
        runtime->num_plays = (int)sixel_builtin_read_be32(runtime->p + 12);
        state->expected_sequence = 0u;
        if (runtime->num_frames <= 0) {
            return SIXEL_BAD_INPUT;
        }
        if (runtime->loop_no == 0 && *start_frame_no != INT_MIN) {
            status = sixel_builtin_resolve_animation_start_frame_no(
                *start_frame_no,
                runtime->num_frames,
                start_frame_no);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    } else if (memcmp(runtime->p + 4, "fcTL", 4) == 0 &&
               runtime->seen_actl != 0) {
        if (runtime->has_frame != 0 && state->chunk_size > 0u) {
            status = sixel_builtin_apng_flush_pending_frame(
                state,
                control,
                canvas,
                runtime,
                *start_frame_no,
                fstatic,
                bgcolor,
                fn_load,
                context,
                pchunk->allocator,
                &stop_after_emit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (stop_after_emit != 0 && stop_decode != NULL) {
                *stop_decode = 1;
                return SIXEL_OK;
            }
        }
        if (!sixel_builtin_apng_parse_fctl(runtime->p + 8,
                                           runtime->length,
                                           &runtime->sequence_no,
                                           control)) {
            return SIXEL_BAD_INPUT;
        }
        if (runtime->sequence_no != state->expected_sequence) {
            return SIXEL_BAD_INPUT;
        }
        ++state->expected_sequence;
        if (control->width == 0u ||
            control->height == 0u ||
            control->x_offset > (uint32_t)canvas->width ||
            control->y_offset > (uint32_t)canvas->height ||
            control->width > (uint32_t)canvas->width - control->x_offset ||
            control->height > (uint32_t)canvas->height - control->y_offset) {
            return SIXEL_BAD_INPUT;
        }
        runtime->seen_fctl = 1;
        runtime->has_frame = 1;
    } else if (memcmp(runtime->p + 4, "fdAT", 4) == 0 &&
               runtime->seen_actl != 0) {
        if (runtime->has_frame == 0 || runtime->seen_fctl == 0 ||
            runtime->length < 4u) {
            return SIXEL_BAD_INPUT;
        }
        runtime->fd_sequence = sixel_builtin_read_be32(runtime->p + 8);
        if (runtime->fd_sequence != state->expected_sequence) {
            return SIXEL_BAD_INPUT;
        }
        ++state->expected_sequence;
        if (!sixel_builtin_apng_append_chunk(state,
                                             "IDAT",
                                             runtime->p + 12,
                                             runtime->length - 4u,
                                             pchunk->allocator)) {
            return SIXEL_BAD_ALLOCATION;
        }
    } else if (memcmp(runtime->p + 4, "IDAT", 4) == 0) {
        if (runtime->seen_actl != 0 && runtime->has_frame == 0) {
            return SIXEL_BAD_INPUT;
        }
        if (!sixel_builtin_apng_append_chunk(state,
                                             "IDAT",
                                             runtime->p + 8,
                                             runtime->length,
                                             pchunk->allocator)) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (runtime->seen_actl != 0 &&
            runtime->seen_fctl == 0 &&
            runtime->seen_idat == 0) {
            control->width = (uint32_t)canvas->width;
            control->height = (uint32_t)canvas->height;
            control->x_offset = 0u;
            control->y_offset = 0u;
            control->delay_cs = 0u;
            control->dispose_op = 0u;
            control->blend_op = 0u;
        }
        runtime->seen_idat = 1;
        runtime->has_frame = 1;
    } else if (memcmp(runtime->p + 4, "IEND", 4) == 0) {
        if (stop_scan != NULL) {
            *stop_scan = 1;
        }
    } else if (memcmp(runtime->p + 4, "tRNS", 4) == 0) {
        runtime->has_trns_chunk = 1;
        runtime->alpha_zero_is_transparent =
            sixel_builtin_apng_resolve_alpha_zero_transparent(
                runtime->trns_keycolor_mode,
                bgcolor,
                enable_cms,
                runtime->has_trns_chunk,
                runtime->has_alpha_chunk);
    } else if (memcmp(runtime->p + 4, "acTL", 4) != 0 &&
               memcmp(runtime->p + 4, "fcTL", 4) != 0 &&
               memcmp(runtime->p + 4, "fdAT", 4) != 0 &&
               memcmp(runtime->p + 4, "IHDR", 4) != 0 &&
               memcmp(runtime->p + 4, "IEND", 4) != 0 &&
               state->chunk_size == 0u) {
        if (!sixel_builtin_apng_append_shared_chunk(state,
                                                    runtime->p,
                                                    (size_t)runtime->length
                                                        + 12u,
                                                    pchunk->allocator)) {
            return SIXEL_BAD_ALLOCATION;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_apng_scan_loop_chunks(
    sixel_chunk_t const *pchunk,
    sixel_builtin_apng_state_t *state,
    sixel_builtin_apng_frame_control_t *control,
    sixel_builtin_apng_canvas_t *canvas,
    sixel_builtin_apng_runtime_t *runtime,
    unsigned char *bgcolor,
    int enable_cms,
    int fstatic,
    int *start_frame_no,
    sixel_load_image_function fn_load,
    void *context,
    int *stop_decode)
{
    SIXELSTATUS status;
    int stop_scan;

    status = SIXEL_OK;
    stop_scan = 0;
    if (pchunk == NULL ||
        state == NULL ||
        control == NULL ||
        canvas == NULL ||
        runtime == NULL ||
        start_frame_no == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (stop_decode != NULL) {
        *stop_decode = 0;
    }

    while (runtime->remain >= 12u) {
        if (sixel_loader_callback_is_canceled(context)) {
            return SIXEL_INTERRUPTED;
        }
        stop_scan = 0;
        status = sixel_builtin_apng_process_chunk(
            pchunk,
            state,
            control,
            canvas,
            runtime,
            bgcolor,
            enable_cms,
            fstatic,
            start_frame_no,
            fn_load,
            context,
            stop_decode,
            &stop_scan);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (stop_decode != NULL && *stop_decode != 0) {
            return SIXEL_OK;
        }
        if (stop_scan != 0) {
            break;
        }
        runtime->p += (size_t)runtime->length + 12u;
        runtime->remain -= (size_t)runtime->length + 12u;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_apng_complete_loop_iteration(
    sixel_builtin_apng_state_t *state,
    sixel_builtin_apng_runtime_t *runtime,
    int loop_control,
    sixel_allocator_t *allocator,
    int *stop_loop)
{
    if (state == NULL || runtime == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (stop_loop != NULL) {
        *stop_loop = 0;
    }
    if (runtime->frames_in_loop == 0) {
        return SIXEL_BAD_INPUT;
    }
    if (runtime->num_frames > 0 &&
        runtime->frames_in_loop != runtime->num_frames) {
        return SIXEL_BAD_INPUT;
    }

    ++runtime->loop_no;
    sixel_builtin_apng_release_loop_buffers(state, allocator);
    sixel_trace_topic_message(
        "apng",
        "loop complete loop=%d frames_in_loop=%d num_plays=%d",
        runtime->loop_no,
        runtime->frames_in_loop,
        runtime->num_plays);
    if (stop_loop != NULL &&
        sixel_builtin_apng_should_stop_loop(loop_control,
                                            runtime->frames_in_loop,
                                            runtime->loop_no,
                                            runtime->num_plays)) {
        *stop_loop = 1;
    }
    return SIXEL_OK;
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
    sixel_builtin_apng_runtime_t runtime;
    int apng_start_frame_no;
    int stop_decode;
    int stop_loop;
    int trns_keycolor_mode;

    status = SIXEL_FALSE;
    memset(&state, 0, sizeof(state));
    memset(&control, 0, sizeof(control));
    memset(&canvas, 0, sizeof(canvas));
    memset(&runtime, 0, sizeof(runtime));
    apng_start_frame_no = start_frame_no;
    stop_decode = 0;
    stop_loop = 0;
    trns_keycolor_mode = sixel_builtin_trns_keycolor_mode();
    sixel_builtin_apng_init_runtime(&runtime, trns_keycolor_mode);
    sixel_trace_topic_message(
        "apng",
        "decode start size=%zu static=%d cms=%d loop_control=%d "
        "start_frame=%d trns_mode=%d",
        pchunk != NULL ? pchunk->size : 0u,
        fstatic,
        enable_cms,
        loop_control,
        start_frame_no,
        trns_keycolor_mode);

    if (pchunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pchunk->size < 8) {
        return SIXEL_FALSE;
    }

    for (;;) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        sixel_builtin_apng_begin_loop_iteration(&state,
                                                &control,
                                                &canvas,
                                                &runtime,
                                                pchunk);
        stop_decode = 0;
        status = sixel_builtin_apng_scan_loop_chunks(
            pchunk,
            &state,
            &control,
            &canvas,
            &runtime,
            bgcolor,
            enable_cms,
            fstatic,
            &apng_start_frame_no,
            fn_load,
            context,
            &stop_decode);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (!runtime.seen_actl || !runtime.has_frame) {
            status = SIXEL_FALSE;
            goto end;
        }
        status = sixel_builtin_apng_flush_pending_frame(
            &state,
            &control,
            &canvas,
            &runtime,
            apng_start_frame_no,
            fstatic,
            bgcolor,
            fn_load,
            context,
            pchunk->allocator,
            &stop_decode);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (stop_decode != 0) {
            status = SIXEL_OK;
            goto end;
        }

        stop_loop = 0;
        status = sixel_builtin_apng_complete_loop_iteration(
            &state,
            &runtime,
            loop_control,
            pchunk->allocator,
            &stop_loop);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (stop_loop != 0) {
            status = SIXEL_OK;
            goto end;
        }
    }

end:
    sixel_trace_topic_message(
        "apng",
        "decode end status=%s loops=%d saw_animation=%d",
        sixel_helper_format_error(status),
        runtime.loop_no,
        runtime.saw_animation);
    sixel_allocator_free(pchunk->allocator, canvas.pixels);
    sixel_allocator_free(pchunk->allocator, canvas.backup);
    sixel_builtin_apng_release_loop_buffers(&state, pchunk->allocator);
    if (!runtime.saw_animation && status == SIXEL_FALSE) {
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
            self->cms_engine = *int_value;
        } else {
            self->cms_engine = -1;
        }
#if !defined(__PCC__)
        sixel_helper_set_loader_cms_engine(int_value != NULL ? *int_value : -1);
#endif
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_BMP_INFO40_MODE:
        int_value = (int const *)value;
        if (int_value == NULL) {
            self->bmp_info40_mode =
                SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;
            return SIXEL_OK;
        }
        if (*int_value != SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO &&
            *int_value != SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS &&
            *int_value != SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2) {
            return SIXEL_BAD_ARGUMENT;
        }
        self->bmp_info40_mode = *int_value;
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
    SIXELSTATUS status;
#if SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32)
    int lock_acquired;
#endif

    self = NULL;
    bgcolor = NULL;
    status = SIXEL_FALSE;
#if SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32)
    lock_acquired = 0;
#endif
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_builtin_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

#if SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32)
    lock_acquired = sixel_loader_builtin_lock_acquire();
#endif
    sixel_helper_set_loader_cms_engine(self->cms_engine);
    status = load_with_builtin(chunk,
                               self->fstatic,
                               self->fuse_palette,
                               self->reqcolors,
                               bgcolor,
                               self->loop_control,
                               self->start_frame_no_set,
                               self->start_frame_no,
                               self->enable_cms,
                               self->bmp_info40_mode,
                               fn_load,
                               context);
#if SIXEL_ENABLE_THREADS && defined(__PCC__) && !defined(_WIN32)
    sixel_loader_builtin_lock_release(lock_acquired);
#endif
    return status;
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
    self->cms_engine = -1;
    self->bmp_info40_mode = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;

    *ppcomponent = &self->base;
    return SIXEL_OK;
}

typedef struct sixel_builtin_load_request {
    sixel_chunk_t const *chunk;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char *bgcolor;
    int loop_control;
    int start_frame_no_set;
    int start_frame_no_override;
    int enable_cms;
    int bmp_info40_mode;
    sixel_load_image_function fn_load;
    void *callback_context;
} sixel_builtin_load_request_t;

typedef struct sixel_builtin_load_context {
    int start_frame_no;
    int resolved_start_frame_no;
    int gif_frame_count;
} sixel_builtin_load_context_t;

typedef enum sixel_builtin_decode_path {
    SIXEL_BUILTIN_DECODE_PATH_SIXEL = 0,
    SIXEL_BUILTIN_DECODE_PATH_PNM,
    SIXEL_BUILTIN_DECODE_PATH_GIF,
    SIXEL_BUILTIN_DECODE_PATH_STBI
} sixel_builtin_decode_path_t;

static sixel_builtin_decode_path_t
sixel_builtin_detect_decode_path(sixel_chunk_t const *chunk)
{
    if (chunk != NULL && chunk_is_sixel(chunk)) {
        return SIXEL_BUILTIN_DECODE_PATH_SIXEL;
    }
    if (chunk != NULL && chunk_is_pnm(chunk)) {
        return SIXEL_BUILTIN_DECODE_PATH_PNM;
    }
    if (chunk != NULL && chunk_is_gif(chunk)) {
        return SIXEL_BUILTIN_DECODE_PATH_GIF;
    }
    return SIXEL_BUILTIN_DECODE_PATH_STBI;
}

static char const *
sixel_builtin_decode_path_name(sixel_builtin_decode_path_t path)
{
    switch (path) {
    case SIXEL_BUILTIN_DECODE_PATH_SIXEL:
        return "sixel";
    case SIXEL_BUILTIN_DECODE_PATH_PNM:
        return "pnm";
    case SIXEL_BUILTIN_DECODE_PATH_GIF:
        return "gif";
    case SIXEL_BUILTIN_DECODE_PATH_STBI:
    default:
        return "stbi";
    }
}

static int
sixel_builtin_chunk_has_apng_control(sixel_chunk_t const *chunk)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    unsigned char const *p;
    size_t remain;
    uint32_t chunk_length;

    p = NULL;
    remain = 0u;
    chunk_length = 0u;
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 8u) {
        return 0;
    }
    /*
     * Start-frame parsing must only run for animated input. A lightweight
     * acTL probe keeps static PNG decode tolerant to invalid env values.
     */
    if (memcmp(chunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    p = chunk->buffer + 8;
    remain = chunk->size - 8u;
    while (remain >= 12u) {
        chunk_length = sixel_builtin_read_be32(p);
        if ((size_t)chunk_length > remain - 12u) {
            return 0;
        }
        if (memcmp(p + 4, "acTL", 4) == 0) {
            return 1;
        }
        if (memcmp(p + 4, "IEND", 4) == 0) {
            break;
        }
        p += (size_t)chunk_length + 12u;
        remain -= (size_t)chunk_length + 12u;
    }
    return 0;
}

static SIXELSTATUS
sixel_builtin_prepare_load_context(
    sixel_builtin_load_request_t const *request,
    sixel_builtin_load_context_t *load_context,
    int apply_start_frame)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (request == NULL ||
        load_context == NULL ||
        request->chunk == NULL ||
        request->fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    load_context->start_frame_no = INT_MIN;
    load_context->resolved_start_frame_no = -1;
    load_context->gif_frame_count = 0;

    if (apply_start_frame == 0) {
        return SIXEL_OK;
    }
    if (request->start_frame_no_set != 0) {
        load_context->start_frame_no = request->start_frame_no_override;
    } else {
        status = sixel_builtin_parse_animation_start_frame_no(
            &load_context->start_frame_no);
    }

    return status;
}

static SIXELSTATUS
sixel_builtin_finalize_loaded_frame(
    sixel_builtin_load_request_t const *request,
    sixel_frame_t *frame)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (request == NULL || frame == NULL || request->fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!frame->alpha_zero_is_transparent) {
        status = sixel_frame_strip_alpha(frame, request->bgcolor);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    status = request->fn_load(frame, request->callback_context);
    return status;
}

static SIXELSTATUS
sixel_builtin_chunk_size_to_int(sixel_chunk_t const *chunk, int *chunk_size)
{
    if (chunk == NULL || chunk_size == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (chunk->size > INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    *chunk_size = (int)chunk->size;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_load_gif_frames(
    sixel_builtin_load_request_t const *request,
    sixel_builtin_load_context_t *load_context)
{
    SIXELSTATUS status;
    fn_pointer fnp;
    int chunk_size;

    status = SIXEL_OK;
    fnp.fn = NULL;
    chunk_size = 0;
    if (request == NULL ||
        load_context == NULL ||
        request->chunk == NULL ||
        request->fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    fnp.fn = request->fn_load;
    status = sixel_builtin_chunk_size_to_int(request->chunk, &chunk_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (load_context->start_frame_no != INT_MIN) {
        status = sixel_builtin_count_gif_frames(request->chunk,
                                                &load_context->gif_frame_count);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        status = sixel_builtin_resolve_animation_start_frame_no(
            load_context->start_frame_no,
            load_context->gif_frame_count,
            &load_context->resolved_start_frame_no);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return load_gif(request->chunk->buffer,
                    chunk_size,
                    request->bgcolor,
                    request->reqcolors,
                    request->fuse_palette,
                    request->fstatic,
                    request->loop_control,
                    load_context->resolved_start_frame_no,
                    fnp.p,
                    request->callback_context,
                    request->chunk->allocator);
}

static SIXELSTATUS
sixel_builtin_prepare_frame_and_chunk_size(
    sixel_chunk_t const *chunk,
    sixel_frame_t **frame,
    int *chunk_size)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (chunk == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_new(frame, chunk->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_builtin_chunk_size_to_int(chunk, chunk_size);
    return status;
}

static SIXELSTATUS
sixel_builtin_try_load_indexed_png(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    unsigned char *bgcolor,
    int reqcolors,
    int enable_cms,
    int *loaded)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *palette;
    int palette_comp;
    int palette_colors;

    status = SIXEL_OK;
    pixels = NULL;
    palette = NULL;
    palette_comp = 0;
    palette_colors = 0;
    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        loaded == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *loaded = 0;

    stbi__start_mem(stb_context, chunk->buffer, chunk_size);
    pixels = stbi__png_load_palette(stb_context,
                                    &frame->width,
                                    &frame->height,
                                    &palette_comp,
                                    &palette,
                                    &palette_colors,
                                    ri);
    if (pixels == NULL || palette == NULL) {
        return SIXEL_OK;
    }
    if (palette_colors > reqcolors) {
        /*
         * Match libpng behavior: when the source palette exceeds reqcolors,
         * drop indexed output and fall back to non-indexed decoding.
         */
        sixel_allocator_free(chunk->allocator, pixels);
        stbi_free(palette);
        return SIXEL_OK;
    }

    status = convert_palette_to_rgb(&frame->palette,
                                    &palette,
                                    palette_colors,
                                    palette_comp,
                                    bgcolor,
                                    chunk->allocator);
    if (SIXEL_FAILED(status)) {
        stbi_free(palette);
        sixel_allocator_free(chunk->allocator, pixels);
        return status;
    }

    frame->ncolors = palette_colors;
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    if (enable_cms) {
        /*
         * Keep indexed PNG colorspace fallback behavior (iCCP > sRGB >
         * cHRM/gAMA) consistent with the previous inlined path.
         */
        sixel_frompng_apply_colorspace_fallback(frame->palette,
                                                palette_colors,
                                                1,
                                                chunk->buffer,
                                                chunk->size,
                                                chunk->allocator);
    }
    sixel_frame_set_pixels(frame, pixels);
    frame->loop_count = 1;
    *loaded = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_try_load_indexed_tga(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    unsigned char *bgcolor,
    int *loaded)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *palette;
    int palette_comp;
    int palette_colors;
    int palette_keycolor_index;
    int palette_zero_alpha_count;
    size_t palette_pixel_count;
    size_t palette_pixel_index;
    unsigned int palette_index;
    unsigned char palette_zero_alpha_map[SIXEL_PALETTE_MAX];

    status = SIXEL_OK;
    pixels = NULL;
    palette = NULL;
    palette_comp = 0;
    palette_colors = 0;
    palette_keycolor_index = -1;
    palette_zero_alpha_count = 0;
    palette_pixel_count = 0u;
    palette_pixel_index = 0u;
    palette_index = 0u;
    memset(palette_zero_alpha_map, 0, sizeof(palette_zero_alpha_map));
    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        loaded == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *loaded = 0;

    stbi__start_mem(stb_context, chunk->buffer, chunk_size);
    if (!stbi__tga_test(stb_context)) {
        return SIXEL_OK;
    }

    pixels = stbi__tga_load_palette(stb_context,
                                    &frame->width,
                                    &frame->height,
                                    &palette_comp,
                                    &palette,
                                    &palette_colors,
                                    ri);
    if (pixels == NULL || palette == NULL) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (palette_comp == 4 &&
        palette_colors > 0) {
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
    }

    status = convert_palette_to_rgb(&frame->palette,
                                    &palette,
                                    palette_colors,
                                    palette_comp,
                                    bgcolor,
                                    chunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    palette = NULL;

    frame->ncolors = palette_colors;
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    if (palette_keycolor_index >= 0) {
        sixel_frame_set_transparent(frame, palette_keycolor_index);
    }
    if (palette_zero_alpha_count > 1 &&
        palette_keycolor_index >= 0 &&
        frame->width > 0 &&
        frame->height > 0 &&
        (size_t)frame->width <= SIZE_MAX / (size_t)frame->height) {
        palette_pixel_count = (size_t)frame->width * (size_t)frame->height;
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
    pixels = NULL;
    frame->loop_count = 1;
    *loaded = 1;
    status = SIXEL_OK;

cleanup:
    /*
     * stbi__tga_load_palette() can return pixels == NULL after freeing the
     * temporary packed palette internally. In that case, the palette pointer
     * is stale and must not be released again here.
     */
    if (pixels == NULL) {
        palette = NULL;
    }
    if (palette != NULL) {
        stbi_free(palette);
        palette = NULL;
    }
    if (pixels != NULL) {
        sixel_allocator_free(chunk->allocator, pixels);
        pixels = NULL;
    }
    return status;
}

static SIXELSTATUS
sixel_builtin_load_png_keycolor_or_rgba(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    unsigned char *bgcolor,
    int reqcolors)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *palette;
    int depth;
    int palette_comp;
    int palette_colors;
    int palette_keycolor_index;
    int palette_zero_alpha_count;
    size_t palette_pixel_count;
    size_t palette_pixel_index;
    unsigned int palette_index;
    unsigned char palette_zero_alpha_map[SIXEL_PALETTE_MAX];

    status = SIXEL_OK;
    pixels = NULL;
    palette = NULL;
    depth = 0;
    palette_comp = 0;
    palette_colors = 0;
    palette_keycolor_index = -1;
    palette_zero_alpha_count = 0;
    palette_pixel_count = 0u;
    palette_pixel_index = 0u;
    palette_index = 0u;
    memset(palette_zero_alpha_map, 0, sizeof(palette_zero_alpha_map));
    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    stbi__start_mem(stb_context, chunk->buffer, chunk_size);
    pixels = stbi__png_load_palette(stb_context,
                                    &frame->width,
                                    &frame->height,
                                    &palette_comp,
                                    &palette,
                                    &palette_colors,
                                    ri);
    if (pixels != NULL &&
        palette != NULL &&
        palette_comp == 4 &&
        palette_colors > 0 &&
        palette_colors <= reqcolors) {
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
                                        &palette,
                                        palette_colors,
                                        palette_comp,
                                        bgcolor,
                                        chunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        palette = NULL;

        frame->ncolors = palette_colors;
        frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        if (palette_keycolor_index >= 0) {
            sixel_frame_set_transparent(frame, palette_keycolor_index);
        }
        if (palette_zero_alpha_count > 1 &&
            palette_keycolor_index >= 0 &&
            frame->width > 0 &&
            frame->height > 0 &&
            (size_t)frame->width <= SIZE_MAX / (size_t)frame->height) {
            palette_pixel_count = (size_t)frame->width * (size_t)frame->height;
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
        pixels = NULL;
        frame->loop_count = 1;
        status = SIXEL_OK;
        goto cleanup;
    }

    /*
     * stbi__png_load_palette() can return pixels == NULL after freeing the
     * temporary packed palette internally. In that case, the palette pointer
     * is stale and must not be released again on the fallback path.
     */
    if (pixels == NULL) {
        palette = NULL;
    }
    if (palette != NULL) {
        stbi_free(palette);
        palette = NULL;
    }
    if (pixels != NULL) {
        sixel_allocator_free(chunk->allocator, pixels);
        pixels = NULL;
    }
    pixels = stbi_load_from_memory(chunk->buffer,
                                   chunk_size,
                                   &frame->width,
                                   &frame->height,
                                   &depth,
                                   4);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        status = SIXEL_STBI_ERROR;
        goto cleanup;
    }

    sixel_frame_set_pixels(frame, pixels);
    pixels = NULL;
    frame->loop_count = 1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->alpha_zero_is_transparent = 1;
    status = SIXEL_OK;

cleanup:
    if (palette != NULL) {
        stbi_free(palette);
        palette = NULL;
    }
    if (pixels != NULL) {
        sixel_allocator_free(chunk->allocator, pixels);
        pixels = NULL;
    }
    return status;
}

static SIXELSTATUS
sixel_builtin_load_png_single_frame(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int fuse_palette,
    int reqcolors,
    int enable_cms,
    int png_keycolor_mode,
    unsigned char *bgcolor)
{
    SIXELSTATUS status;
    int pal_loaded;

    status = SIXEL_OK;
    pal_loaded = 0;
    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (fuse_palette && !png_keycolor_mode) {
        status = sixel_builtin_try_load_indexed_png(chunk,
                                                    chunk_size,
                                                    frame,
                                                    stb_context,
                                                    ri,
                                                    bgcolor,
                                                    reqcolors,
                                                    enable_cms,
                                                    &pal_loaded);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (pal_loaded != 0) {
            return SIXEL_OK;
        }
    }
    if (fuse_palette) {
        status = sixel_builtin_try_load_indexed_tga(chunk,
                                                    chunk_size,
                                                    frame,
                                                    stb_context,
                                                    ri,
                                                    bgcolor,
                                                    &pal_loaded);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (pal_loaded != 0) {
            return SIXEL_OK;
        }
    }
    if (png_keycolor_mode) {
        return sixel_builtin_load_png_keycolor_or_rgba(chunk,
                                                       chunk_size,
                                                       frame,
                                                       stb_context,
                                                       ri,
                                                       bgcolor,
                                                       reqcolors);
    }
    return sixel_frompng_load_nonindexed(chunk, frame, enable_cms, bgcolor);
}

static SIXELSTATUS
sixel_builtin_load_jpeg_float32(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int enable_cms,
    unsigned char **icc_profile,
    size_t *icc_profile_length)
{
    float *float_pixels;
    int depth;
    int cms_converted;

    float_pixels = NULL;
    depth = 0;
    cms_converted = 0;
    if (chunk == NULL ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        icc_profile == NULL ||
        icc_profile_length == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    float_pixels = stbi__jpeg_loadf(stb_context,
                                    &frame->width,
                                    &frame->height,
                                    &depth,
                                    3,
                                    ri);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        return SIXEL_STBI_ERROR;
    }

    sixel_frame_set_pixels(frame, (unsigned char *)float_pixels);
    frame->loop_count = 1;
    if (enable_cms) {
        if (sixel_builtin_extract_jpeg_icc(chunk->buffer,
                                           chunk->size,
                                           icc_profile,
                                           icc_profile_length,
                                           chunk->allocator)) {
            cms_converted = sixel_cms_convert_to_srgb_with_profile_bytes(
                (unsigned char *)float_pixels,
                frame->width,
                frame->height,
                SIXEL_PIXELFORMAT_RGBFLOAT32,
                *icc_profile,
                *icc_profile_length);
            if (!cms_converted) {
                loader_trace_message(
                    "builtin JPEG: embedded ICC conversion failed");
            }
        }
    }
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    return SIXEL_OK;
}

typedef SIXELSTATUS (*sixel_builtin_psd_decode_basic_fn_t)(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

typedef SIXELSTATUS (*sixel_builtin_psd_decode_cmyk_fn_t)(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

typedef struct sixel_builtin_psd_decode_basic_entry {
    int mode;
    sixel_builtin_psd_decode_basic_fn_t fn;
} sixel_builtin_psd_decode_basic_entry_t;

#if defined(__has_attribute)
# if __has_attribute(unused)
#  define SIXEL_BUILTIN_PSD_HELPER_UNUSED __attribute__((unused))
# endif
#endif

#ifndef SIXEL_BUILTIN_PSD_HELPER_UNUSED
# if defined(__GNUC__) && !defined(__PCC__)
#  define SIXEL_BUILTIN_PSD_HELPER_UNUSED __attribute__((unused))
# else
#  define SIXEL_BUILTIN_PSD_HELPER_UNUSED
# endif
#endif

static int SIXEL_BUILTIN_PSD_HELPER_UNUSED
sixel_builtin_psd_mode_is_cmyk(int mode)
{
    return mode == SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_8BIT ||
        mode == SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_16BIT ||
        mode == SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_32BIT;
}

static int SIXEL_BUILTIN_PSD_HELPER_UNUSED
sixel_builtin_psd_mode_is_lab(int mode)
{
    return mode == SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_8BIT ||
        mode == SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_16BIT ||
        mode == SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_32BIT;
}

static sixel_builtin_psd_decode_basic_fn_t SIXEL_BUILTIN_PSD_HELPER_UNUSED
sixel_builtin_psd_lookup_basic_decode_fn(int mode)
{
    static sixel_builtin_psd_decode_basic_entry_t const entries[] = {
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_BITMAP_1BIT,
            sixel_builtin_decode_psd_bitmap_1bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_INDEXED_8BIT,
            sixel_builtin_decode_psd_gray_or_indexed_8bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_16BIT,
            sixel_builtin_decode_psd_gray_or_duotone_16bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_32BIT,
            sixel_builtin_decode_psd_gray_or_duotone_32bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_8BIT,
            sixel_builtin_decode_psd_lab_8bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_16BIT,
            sixel_builtin_decode_psd_lab_16bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_32BIT,
            sixel_builtin_decode_psd_lab_32bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_8BIT,
            sixel_builtin_decode_psd_rgb_8bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_16BIT,
            sixel_builtin_decode_psd_rgb_16bit
        },
        {
            SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_32BIT,
            sixel_builtin_decode_psd_rgb_32bit
        }
    };
    size_t index;

    index = 0u;
    for (index = 0u; index < sizeof(entries) / sizeof(entries[0]); ++index) {
        if (entries[index].mode == mode) {
            return entries[index].fn;
        }
    }
    return NULL;
}

static SIXELSTATUS SIXEL_BUILTIN_PSD_HELPER_UNUSED
sixel_builtin_psd_decode_cmyk_by_mode(
    int mode,
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    sixel_builtin_psd_decode_cmyk_fn_t fn;

    fn = NULL;
    if (chunk == NULL ||
        info == NULL ||
        ppixels == NULL ||
        ptransparent_mask == NULL ||
        ptransparent_mask_size == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        ppixelformat == NULL ||
        pcms_applied == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (mode) {
    case SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_8BIT:
        fn = sixel_builtin_decode_psd_cmyk_8bit;
        break;
    case SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_16BIT:
        fn = sixel_builtin_decode_psd_cmyk_16bit;
        break;
    case SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_32BIT:
        fn = sixel_builtin_decode_psd_cmyk_32bit;
        break;
    default:
        return SIXEL_FALSE;
    }

    return fn(chunk,
              info,
              bgcolor,
              icc_profile,
              icc_profile_length,
              pcms_applied,
              ppixels,
              ptransparent_mask,
              ptransparent_mask_size,
              pwidth,
              pheight,
              ppixelformat);
}

static void
sixel_builtin_psd_trace_malformed_icc(void)
{
    loader_trace_message(
        "builtin PSD: malformed ICC resource section; "
        "skipping ICC conversion");
}

static void
sixel_builtin_psd_trace_embedded_icc_failure(void)
{
    loader_trace_message("builtin PSD: embedded ICC conversion failed");
}

static void
sixel_builtin_psd_trace_skip_icc_reason(
    int psd_icc_status,
    int psd_custom_decode_mode,
    int psd_cmyk_icc_applied)
{
    if (psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
        sixel_builtin_psd_trace_malformed_icc();
    } else if (sixel_builtin_psd_mode_is_cmyk(psd_custom_decode_mode) &&
               psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_FOUND &&
               !psd_cmyk_icc_applied) {
        sixel_builtin_psd_trace_embedded_icc_failure();
    }
}

static int
sixel_builtin_psd_apply_embedded_icc(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *icc_profile,
    size_t icc_profile_length)
{
    int cms_converted;
    sixel_cms_engine_t cms_engine;
    sixel_cms_profile_t *src_profile;
    sixel_cms_color_space_t src_colorspace;

    cms_converted = 0;
    cms_engine = sixel_cms_get_engine();
    src_profile = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
    if (pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        cms_converted = sixel_cms_convert_to_linearrgb_with_profile_bytes(
            pixels,
            width,
            height,
            pixelformat,
            icc_profile,
            icc_profile_length);
    } else if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                      icc_profile_length);
        if (src_profile == NULL) {
            if (cms_engine == SIXEL_CMS_ENGINE_NONE) {
                loader_trace_message(
                    "builtin PSD: skipping embedded ICC conversion "
                    "for CIELAB path (cms backend unsupported)");
                return 0;
            }
            sixel_builtin_psd_trace_embedded_icc_failure();
            return 0;
        }
        src_colorspace = sixel_cms_get_color_space(src_profile);
        if (src_colorspace != SIXEL_CMS_COLORSPACE_LAB) {
            /*
             * Lab pixels can only consume Lab-domain profiles.
             * Non-Lab ICC payloads are non-applicable here, not malformed.
             */
            sixel_cms_close_profile(src_profile);
            return 0;
        }
        cms_converted = sixel_cms_convert_profile_to_cielab(
            pixels,
            width,
            height,
            pixelformat,
            src_profile);
        sixel_cms_close_profile(src_profile);
        if (!cms_converted && cms_engine == SIXEL_CMS_ENGINE_NONE) {
            loader_trace_message(
                "builtin PSD: skipping embedded ICC conversion "
                "for CIELAB path (cms backend unsupported)");
            return 0;
        }
    } else {
        cms_converted = sixel_cms_convert_to_srgb_with_profile_bytes(
            pixels,
            width,
            height,
            pixelformat,
            icc_profile,
            icc_profile_length);
    }
    if (!cms_converted) {
        sixel_builtin_psd_trace_embedded_icc_failure();
    }
    return cms_converted;
}

static SIXELSTATUS
sixel_builtin_load_psd_single_frame(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    unsigned char *bgcolor,
    int enable_cms,
    unsigned char **mask,
    size_t *mask_size)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    int psd_pixelformat;
    int psd_custom_decode_mode;
    int psd_skip_icc_conversion;
    int psd_colorspace;
    int psd_cmyk_icc_applied;
    unsigned char const *psd_icc_profile;
    size_t psd_icc_profile_length;
    sixel_builtin_psd_info_t psd_info;
    int psd_validation_status;
    char psd_validation_message[128];
    int psd_icc_status;
    sixel_builtin_psd_decode_basic_fn_t basic_decode_fn;

    status = SIXEL_FALSE;
    pixels = NULL;
    psd_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    psd_custom_decode_mode = 0;
    psd_skip_icc_conversion = 0;
    psd_colorspace = SIXEL_COLORSPACE_GAMMA;
    psd_cmyk_icc_applied = 0;
    psd_icc_profile = NULL;
    psd_icc_profile_length = 0u;
    psd_validation_status = SIXEL_BUILTIN_PSD_VALIDATE_OK;
    psd_validation_message[0] = '\0';
    psd_icc_status = SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    basic_decode_fn = NULL;
    if (chunk == NULL ||
        frame == NULL ||
        mask == NULL ||
        mask_size == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *mask = NULL;
    *mask_size = 0u;
    sixel_helper_set_additional_message(NULL);
    if (!sixel_builtin_parse_psd_info(chunk, &psd_info)) {
        char const *additional_message;

        additional_message = sixel_helper_get_additional_message();
        if (additional_message == NULL || additional_message[0] == '\0') {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed section length/offset");
        }
        return SIXEL_STBI_ERROR;
    }
    psd_validation_status = sixel_builtin_validate_psd_info(
        chunk,
        &psd_info,
        &psd_custom_decode_mode,
        &psd_skip_icc_conversion,
        &psd_colorspace,
        psd_validation_message,
        sizeof(psd_validation_message));
    if (psd_validation_status != SIXEL_BUILTIN_PSD_VALIDATE_OK) {
        if (psd_validation_message[0] != '\0') {
            sixel_helper_set_additional_message(psd_validation_message);
        } else {
            sixel_helper_set_additional_message(
                "builtin PSD: validation failed");
        }
        return SIXEL_STBI_ERROR;
    }
    if (enable_cms) {
        psd_icc_status = sixel_builtin_extract_psd_icc(chunk->buffer,
                                                       chunk->size,
                                                       &psd_icc_profile,
                                                       &psd_icc_profile_length);
    }

    basic_decode_fn = sixel_builtin_psd_lookup_basic_decode_fn(
        psd_custom_decode_mode);
    if (basic_decode_fn != NULL) {
        status = basic_decode_fn(chunk,
                                 &psd_info,
                                 bgcolor,
                                 &pixels,
                                 mask,
                                 mask_size,
                                 &frame->width,
                                 &frame->height,
                                 &psd_pixelformat);
    } else {
        status = sixel_builtin_psd_decode_cmyk_by_mode(
            psd_custom_decode_mode,
            chunk,
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
            mask,
            mask_size,
            &frame->width,
            &frame->height,
            &psd_pixelformat);
        if (status == SIXEL_FALSE) {
            sixel_helper_set_additional_message(
                "builtin PSD: internal decode mode selection failed");
            return SIXEL_STBI_ERROR;
        }
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (enable_cms) {
        if (psd_skip_icc_conversion) {
            sixel_builtin_psd_trace_skip_icc_reason(psd_icc_status,
                                                    psd_custom_decode_mode,
                                                    psd_cmyk_icc_applied);
        } else {
            if (psd_icc_status == SIXEL_BUILTIN_ICC_EXTRACT_FOUND) {
                (void)sixel_builtin_psd_apply_embedded_icc(
                    pixels,
                    frame->width,
                    frame->height,
                    psd_pixelformat,
                    psd_icc_profile,
                    psd_icc_profile_length);
            } else if (psd_icc_status ==
                       SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
                sixel_builtin_psd_trace_malformed_icc();
            }
        }
    }

    sixel_frame_set_pixels(frame, pixels);
    frame->loop_count = 1;
    frame->pixelformat = psd_pixelformat;
    frame->colorspace = psd_colorspace;
    frame->transparent = -1;
    frame->transparent_mask = *mask;
    frame->transparent_mask_size = *mask_size;
    *mask = NULL;
    *mask_size = 0u;
    frame->alpha_zero_is_transparent =
        frame->transparent_mask != NULL &&
        frame->transparent_mask_size > 0u ? 1 : 0;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_apply_pic_alpha_policy(
    sixel_frame_t *frame,
    unsigned char *bgcolor)
{
    SIXELSTATUS status;
    unsigned char *transparent_mask;
    unsigned char *pixels;
    size_t pixel_count;
    size_t index;
    int has_zero_alpha;
    unsigned int alpha;
    unsigned int inv_alpha;
    unsigned int r;
    unsigned int g;
    unsigned int b;

    status = SIXEL_FALSE;
    transparent_mask = NULL;
    pixels = NULL;
    pixel_count = 0u;
    index = 0u;
    has_zero_alpha = 0;
    alpha = 0u;
    inv_alpha = 0u;
    r = 0u;
    g = 0u;
    b = 0u;
    if (frame == NULL || frame->allocator == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->width <= 0 ||
        frame->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;

    transparent_mask = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                               pixel_count);
    if (transparent_mask == NULL) {
        sixel_helper_set_additional_message(
            "builtin PIC: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    pixels = frame->pixels.u8ptr;
    for (index = 0u; index < pixel_count; ++index) {
        alpha = pixels[index * 4u + 3u];
        transparent_mask[index] = alpha == 0u ? 1u : 0u;
        if (alpha == 0u) {
            has_zero_alpha = 1;
        }

        if (alpha < 0xffu) {
            inv_alpha = 0xffu - alpha;
            r = pixels[index * 4u + 0u];
            g = pixels[index * 4u + 1u];
            b = pixels[index * 4u + 2u];
            if (bgcolor != NULL) {
                r = (r * alpha + bgcolor[0] * inv_alpha) >> 8;
                g = (g * alpha + bgcolor[1] * inv_alpha) >> 8;
                b = (b * alpha + bgcolor[2] * inv_alpha) >> 8;
            } else {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
            }
            pixels[index * 4u + 0u] = (unsigned char)r;
            pixels[index * 4u + 1u] = (unsigned char)g;
            pixels[index * 4u + 2u] = (unsigned char)b;
            if (alpha > 0u) {
                pixels[index * 4u + 3u] = 0xffu;
            }
        }
    }

    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    if (has_zero_alpha != 0) {
        frame->transparent_mask = transparent_mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        transparent_mask = NULL;
    } else {
        frame->alpha_zero_is_transparent = 0;
    }

    status = SIXEL_OK;
    sixel_allocator_free(frame->allocator, transparent_mask);
    return status;
}

static SIXELSTATUS
sixel_builtin_apply_tga_truecolor_alpha_policy(
    sixel_frame_t *frame,
    unsigned char *bgcolor)
{
    SIXELSTATUS status;
    unsigned char *transparent_mask;
    unsigned char *pixels;
    size_t pixel_count;
    size_t index;
    unsigned int alpha;
    unsigned int r;
    unsigned int g;
    unsigned int b;
    unsigned int bg_r;
    unsigned int bg_g;
    unsigned int bg_b;
    int has_zero_alpha;

    status = SIXEL_FALSE;
    transparent_mask = NULL;
    pixels = NULL;
    pixel_count = 0u;
    index = 0u;
    alpha = 0u;
    r = 0u;
    g = 0u;
    b = 0u;
    bg_r = 0u;
    bg_g = 0u;
    bg_b = 0u;
    has_zero_alpha = 0;
    if (frame == NULL || frame->allocator == NULL ||
        frame->pixels.u8ptr == NULL ||
        frame->width <= 0 ||
        frame->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)frame->width * (size_t)frame->height;
    if (bgcolor != NULL) {
        bg_r = bgcolor[0];
        bg_g = bgcolor[1];
        bg_b = bgcolor[2];
    }

    transparent_mask = (unsigned char *)sixel_allocator_malloc(
        frame->allocator,
        pixel_count);
    if (transparent_mask == NULL) {
        sixel_helper_set_additional_message(
            "builtin TGA: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    pixels = frame->pixels.u8ptr;
    for (index = 0u; index < pixel_count; ++index) {
        alpha = pixels[index * 4u + 3u];
        transparent_mask[index] = alpha == 0u ? 1u : 0u;
        if (alpha == 0u) {
            has_zero_alpha = 1;
        }
        if (alpha < 0xffu) {
            r = sixel_builtin_blend_channel_with_bg(
                pixels[index * 4u + 0u],
                alpha,
                bg_r);
            g = sixel_builtin_blend_channel_with_bg(
                pixels[index * 4u + 1u],
                alpha,
                bg_g);
            b = sixel_builtin_blend_channel_with_bg(
                pixels[index * 4u + 2u],
                alpha,
                bg_b);
        } else {
            r = pixels[index * 4u + 0u];
            g = pixels[index * 4u + 1u];
            b = pixels[index * 4u + 2u];
        }
        pixels[index * 3u + 0u] = (unsigned char)r;
        pixels[index * 3u + 1u] = (unsigned char)g;
        pixels[index * 3u + 2u] = (unsigned char)b;
    }

    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    if (has_zero_alpha != 0) {
        frame->transparent_mask = transparent_mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        transparent_mask = NULL;
    } else {
        frame->alpha_zero_is_transparent = 0;
    }
    frame->transparent = -1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

    status = SIXEL_OK;
    sixel_allocator_free(frame->allocator, transparent_mask);
    return status;
}

static int
sixel_builtin_tga_has_truecolor_alpha(
    sixel_chunk_t const *chunk,
    int chunk_size)
{
    stbi__context stb_context;
    int tga_width;
    int tga_height;
    int tga_comp;

    tga_width = 0;
    tga_height = 0;
    tga_comp = 0;
    if (chunk == NULL ||
        chunk->buffer == NULL ||
        chunk_size < 18) {
        return 0;
    }
    if (chunk->buffer[1] != 0u) {
        return 0;
    }
    if (chunk->buffer[2] != 2u &&
        chunk->buffer[2] != 10u) {
        return 0;
    }
    if (chunk->buffer[16] != 32u) {
        return 0;
    }
    stbi__start_mem(&stb_context, chunk->buffer, chunk_size);
    if (!stbi__tga_info(&stb_context, &tga_width, &tga_height, &tga_comp)) {
        return 0;
    }
    return tga_comp == 4 ? 1 : 0;
}

static SIXELSTATUS
sixel_builtin_load_nonpng_rgb8_fallback(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    unsigned char *bgcolor,
    int is_pic,
    int enable_cms,
    int bmp_info40_mode
#if HAVE_LCMS2
    ,
    int is_tiff,
    unsigned char **icc_profile,
    size_t *icc_profile_length
#endif
)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *payload_icc_profile;
    unsigned char const *bmp_icc_profile;
    unsigned char const *bmp_payload;
    size_t bmp_icc_profile_length;
    size_t bmp_payload_size;
    size_t payload_icc_profile_length;
    sixel_frombmp_probe_t bmp_probe;
    sixel_chunk_t payload_chunk;
    stbi__result_info payload_ri;
    int depth;
    int payload_depth;
    int req_comp;
    int tga_truecolor_alpha;
    int bmp_comp;
    int bmp_is_cmyk;
    int bmp_png_payload_is_16bit;
    int cms_converted;
    int cmyk_converted;
    int target_pixelformat;
    size_t pixel_count;
    size_t rgb_size;
    unsigned char *rgb_pixels;
    uint16_t *pixels16;
    int nwrite;
    char message[80];
#if HAVE_LCMS2
    uint16_t tiff_photometric;
#endif

    status = SIXEL_OK;
    pixels = NULL;
    payload_icc_profile = NULL;
    bmp_icc_profile = NULL;
    bmp_payload = NULL;
    bmp_icc_profile_length = 0u;
    bmp_payload_size = 0u;
    payload_icc_profile_length = 0u;
    memset(&bmp_probe, 0, sizeof(bmp_probe));
    memset(&payload_chunk, 0, sizeof(payload_chunk));
    payload_ri = (stbi__result_info){ 0 };
    depth = 0;
    payload_depth = 0;
    req_comp = 3;
    tga_truecolor_alpha = 0;
    bmp_comp = 0;
    bmp_is_cmyk = 0;
    bmp_png_payload_is_16bit = 0;
    cms_converted = 0;
    cmyk_converted = 0;
    target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    pixel_count = 0u;
    rgb_size = 0u;
    rgb_pixels = NULL;
    pixels16 = NULL;
    nwrite = 0;
    message[0] = '\0';
#if HAVE_LCMS2
    tiff_photometric = (uint16_t)0xffffu;
#endif
    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
#if HAVE_LCMS2
    if (icc_profile == NULL || icc_profile_length == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
#endif
    if (chunk_is_bmp(chunk)) {
        status = sixel_frombmp_probe(chunk, &bmp_probe, bmp_info40_mode);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (sixel_builtin_bmp_has_compressed_payload(bmp_probe.compression)) {
            bmp_payload = bmp_probe.payload;
            bmp_payload_size = bmp_probe.payload_size;
            if (bmp_payload == NULL ||
                bmp_payload_size == 0u ||
                bmp_payload_size > (size_t)INT_MAX) {
                sixel_helper_set_additional_message(
                    "builtin BMP: compressed payload range is invalid");
                return SIXEL_STBI_ERROR;
            }

            payload_chunk.buffer = (unsigned char *)bmp_payload;
            payload_chunk.size = bmp_payload_size;
            payload_chunk.max_size = bmp_payload_size;
            payload_chunk.allocator = chunk->allocator;

            if (bmp_probe.compression == SIXEL_FROMBMP_COMPRESSION_JPEG) {
                stbi__start_mem(stb_context,
                                payload_chunk.buffer,
                                (int)payload_chunk.size);
                status = sixel_builtin_load_jpeg_float32(
                    &payload_chunk,
                    frame,
                    stb_context,
                    &payload_ri,
                    enable_cms,
                    &payload_icc_profile,
                    &payload_icc_profile_length);
                sixel_allocator_free(chunk->allocator, payload_icc_profile);
                return status;
            }
            if (bmp_probe.compression == SIXEL_FROMBMP_COMPRESSION_PNG) {
                bmp_png_payload_is_16bit = stbi_is_16_bit_from_memory(
                    payload_chunk.buffer,
                    (int)payload_chunk.size);
                if (bmp_png_payload_is_16bit != 0 && bgcolor != NULL) {
                    /*
                     * Keep 16-bit precision for BI_PNG when explicit
                     * background composition is requested.
                     */
                    return sixel_frompng_load_nonindexed(&payload_chunk,
                                                         frame,
                                                         enable_cms,
                                                         bgcolor);
                }
                if (bmp_png_payload_is_16bit != 0 && enable_cms == 0) {
                    pixels16 = stbi_load_16_from_memory(
                        payload_chunk.buffer,
                        (int)payload_chunk.size,
                        &frame->width,
                        &frame->height,
                        &payload_depth,
                        4);
                    if (pixels16 == NULL) {
                        sixel_helper_set_additional_message(
                            stbi_failure_reason());
                        return SIXEL_STBI_ERROR;
                    }
                    status = sixel_builtin_apply_bmp_png16_no_bg_policy(
                        frame,
                        pixels16);
                    stbi_image_free(pixels16);
                    pixels16 = NULL;
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    frame->loop_count = 1;
                    return SIXEL_OK;
                }
                stbi__start_mem(stb_context,
                                payload_chunk.buffer,
                                (int)payload_chunk.size);
                pixels = stbi__load_and_postprocess_8bit(stb_context,
                                                         &frame->width,
                                                         &frame->height,
                                                         &payload_depth,
                                                         4);
                if (pixels == NULL) {
                    sixel_helper_set_additional_message(
                        stbi_failure_reason());
                    return SIXEL_STBI_ERROR;
                }
                sixel_frame_set_pixels(frame, pixels);
                frame->loop_count = 1;
                frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                frame->colorspace = SIXEL_COLORSPACE_GAMMA;
                if (enable_cms != 0) {
                    (void)
                        sixel_builtin_apply_bmp_png_colorspace_to_rgba_channels(
                            frame,
                            bmp_payload,
                            bmp_payload_size);
                }
                return sixel_builtin_apply_bmp_alpha_policy(frame, bgcolor);
            }
            sixel_helper_set_additional_message(
                "builtin BMP: unsupported compressed payload mode");
            return SIXEL_STBI_ERROR;
        }
        status = sixel_frombmp_load(chunk,
                                    &pixels,
                                    &frame->width,
                                    &frame->height,
                                    &bmp_comp,
                                    &bmp_is_cmyk,
                                    &bmp_icc_profile,
                                    &bmp_icc_profile_length,
                                    bmp_info40_mode);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bmp_is_cmyk != 0) {
            if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
                sixel_allocator_free(chunk->allocator, pixels);
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            pixel_count = (size_t)frame->width * (size_t)frame->height;
            if (pixel_count > SIZE_MAX / 3u) {
                sixel_allocator_free(chunk->allocator, pixels);
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            rgb_size = pixel_count * 3u;
            rgb_pixels = (unsigned char *)sixel_allocator_malloc(
                chunk->allocator,
                rgb_size);
            if (rgb_pixels == NULL) {
                sixel_allocator_free(chunk->allocator, pixels);
                sixel_helper_set_additional_message(
                    "builtin BMP: sixel_allocator_malloc() failed.");
                return SIXEL_BAD_ALLOCATION;
            }

            cmyk_converted = 0;
            if (enable_cms != 0 &&
                bmp_icc_profile != NULL &&
                bmp_icc_profile_length != 0u) {
                cmyk_converted = sixel_builtin_bmp_convert_cmyk8_to_rgb8_icc(
                    rgb_pixels,
                    pixels,
                    pixel_count,
                    bmp_icc_profile,
                    bmp_icc_profile_length);
            }
            if (!cmyk_converted) {
                sixel_builtin_bmp_convert_cmyk8_to_rgb8_device(
                    rgb_pixels,
                    pixels,
                    pixel_count);
            }

            sixel_allocator_free(chunk->allocator, pixels);
            pixels = rgb_pixels;
            rgb_pixels = NULL;
            sixel_frame_set_pixels(frame, pixels);
            frame->loop_count = 1;
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            if (enable_cms != 0 && cmyk_converted) {
                target_pixelformat = loader_cms_target_pixelformat();
                status = sixel_frame_set_pixelformat(frame,
                                                     target_pixelformat);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
            }
            return SIXEL_OK;
        }
        sixel_frame_set_pixels(frame, pixels);
        frame->loop_count = 1;
        if (bmp_comp == 4) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            if (enable_cms != 0) {
                if (bmp_icc_profile != NULL &&
                    bmp_icc_profile_length != 0u) {
                    (void)sixel_builtin_apply_bmp_icc_to_rgba_channels(
                        frame,
                        bmp_icc_profile,
                        bmp_icc_profile_length);
                } else if (bmp_probe.has_calibrated_rgb != 0) {
                    (void)sixel_builtin_apply_bmp_calibrated_to_rgba_channels(
                        frame,
                        &bmp_probe);
                }
            }
            return sixel_builtin_apply_bmp_alpha_policy(frame, bgcolor);
        }
        if (bmp_comp == 3) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            if (enable_cms != 0) {
                if (bmp_icc_profile != NULL &&
                    bmp_icc_profile_length != 0u) {
                    cms_converted =
                        sixel_cms_convert_to_srgb_with_profile_bytes(
                            pixels,
                            frame->width,
                            frame->height,
                            SIXEL_PIXELFORMAT_RGB888,
                            bmp_icc_profile,
                            bmp_icc_profile_length);
                } else if (bmp_probe.has_calibrated_rgb != 0) {
                    cms_converted =
                        sixel_builtin_apply_bmp_calibrated_rgb_to_rgb8(
                            pixels,
                            frame->width,
                            frame->height,
                            &bmp_probe);
                } else {
                    cms_converted = 0;
                }
                if (!cms_converted) {
                    if (bmp_icc_profile != NULL &&
                        bmp_icc_profile_length != 0u) {
                        loader_trace_message(
                            "builtin BMP: embedded ICC conversion failed");
                    }
                } else {
                    target_pixelformat = loader_cms_target_pixelformat();
                    status = sixel_frame_set_pixelformat(
                        frame,
                        target_pixelformat);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                }
            }
            return SIXEL_OK;
        }
        nwrite = snprintf(message,
                          sizeof(message),
                          "load_with_builtin() failed.\n"
                          "reason: unknown BMP pixel-format.(depth: %d)\n",
                          bmp_comp);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        return SIXEL_STBI_ERROR;
    }

    tga_truecolor_alpha = sixel_builtin_tga_has_truecolor_alpha(chunk,
                                                                 chunk_size);
    if (is_pic || tga_truecolor_alpha != 0) {
        req_comp = 4;
    }

    stbi__start_mem(stb_context, chunk->buffer, chunk_size);
    pixels = stbi__load_and_postprocess_8bit(stb_context,
                                             &frame->width,
                                             &frame->height,
                                             &depth,
                                             req_comp);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        return SIXEL_STBI_ERROR;
    }
    sixel_frame_set_pixels(frame, pixels);
    frame->loop_count = 1;
#if HAVE_LCMS2
    if (enable_cms && is_tiff) {
        if (sixel_builtin_extract_tiff_icc(chunk->buffer,
                                           chunk->size,
                                           icc_profile,
                                           icc_profile_length,
                                           &tiff_photometric,
                                           chunk->allocator)) {
            if (sixel_builtin_tiff_photometric_supports_icc(
                    tiff_photometric)) {
                if (!sixel_cms_convert_to_srgb_with_profile_bytes(
                        pixels,
                        frame->width,
                        frame->height,
                        SIXEL_PIXELFORMAT_RGB888,
                        *icc_profile,
                        *icc_profile_length)) {
                    loader_trace_message(
                        "builtin TIFF: embedded ICC conversion failed");
                }
            }
        }
    }
#endif
    if (is_pic) {
        switch (depth) {
        case 1:
        case 3:
        case 4:
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            status = sixel_builtin_apply_pic_alpha_policy(frame, bgcolor);
            break;
        default:
            nwrite = snprintf(message,
                              sizeof(message),
                              "load_with_builtin() failed.\n"
                              "reason: unknown PIC pixel-format."
                              "(depth: %d)\n",
                              depth);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            status = SIXEL_STBI_ERROR;
            break;
        }
        return status;
    }
    if (tga_truecolor_alpha != 0) {
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        status = sixel_builtin_apply_tga_truecolor_alpha_policy(frame, bgcolor);
        return status;
    }
    switch (depth) {
    case 1:
    case 3:
    case 4:
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        status = SIXEL_OK;
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
        break;
    }
    return status;
}

static SIXELSTATUS
sixel_builtin_load_nonpng_single_frame(
    sixel_chunk_t const *chunk,
    int chunk_size,
    sixel_frame_t *frame,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int fuse_palette,
    unsigned char *bgcolor,
    int enable_cms,
    int bmp_info40_mode,
    int is_jpeg,
    int is_psd,
    int is_pic,
#if HAVE_LCMS2
    int is_tiff,
#endif
    unsigned char **icc_profile,
    size_t *icc_profile_length,
    unsigned char **psd_transparent_mask,
    size_t *psd_transparent_mask_size)
{
    SIXELSTATUS status;
    unsigned char *mask;
    size_t mask_size;
    int pal_loaded;

    status = SIXEL_FALSE;
    mask = NULL;
    mask_size = 0u;
    pal_loaded = 0;

    if (chunk == NULL ||
        chunk_size <= 0 ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        icc_profile == NULL ||
        icc_profile_length == NULL ||
        psd_transparent_mask == NULL ||
        psd_transparent_mask_size == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (fuse_palette) {
        /*
         * Keep the indexed TGA fast path in the non-PNG decode flow so PAL8
         * input does not regress to RGB expansion before quantization.
         */
        status = sixel_builtin_try_load_indexed_tga(chunk,
                                                    chunk_size,
                                                    frame,
                                                    stb_context,
                                                    ri,
                                                    bgcolor,
                                                    &pal_loaded);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pal_loaded != 0) {
            status = SIXEL_OK;
            goto end;
        }
    }

    stbi__start_mem(stb_context, chunk->buffer, chunk_size);
    if (is_jpeg) {
        status = sixel_builtin_load_jpeg_float32(chunk,
                                                 frame,
                                                 stb_context,
                                                 ri,
                                                 enable_cms,
                                                 icc_profile,
                                                 icc_profile_length);
        goto end;
    }

    if (is_psd) {
        status = sixel_builtin_load_psd_single_frame(chunk,
                                                     frame,
                                                     bgcolor,
                                                     enable_cms,
                                                     &mask,
                                                     &mask_size);
        goto end;
    }

    status = sixel_builtin_load_hdr_frame(chunk, frame, enable_cms);
    if (status != SIXEL_FALSE) {
        goto end;
    }
    status = sixel_builtin_load_nonpng_rgb8_fallback(
        chunk,
        chunk_size,
        frame,
        stb_context,
        bgcolor,
        is_pic,
        enable_cms,
        bmp_info40_mode
#if HAVE_LCMS2
        ,
        is_tiff,
        icc_profile,
        icc_profile_length
#endif
    );

end:
    *psd_transparent_mask = mask;
    *psd_transparent_mask_size = mask_size;
    return status;
}

static SIXELSTATUS
sixel_builtin_load_stbi_png_path(
    sixel_builtin_load_request_t const *load_request,
    sixel_builtin_load_context_t const *load_context,
    sixel_frame_t *frame,
    int chunk_size,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int png_keycolor_mode,
    int *animation_handled)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (animation_handled != NULL) {
        *animation_handled = 0;
    }
    if (load_request == NULL ||
        load_context == NULL ||
        load_request->chunk == NULL ||
        frame == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        animation_handled == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Try APNG first for builtin PNG path. Regular PNG input returns
     * SIXEL_FALSE and then falls through to existing single-frame
     * decode logic.
     */
    status = sixel_builtin_load_apng_frames(load_request->chunk,
                                            load_request->fstatic,
                                            load_request->bgcolor,
                                            load_request->enable_cms,
                                            load_request->loop_control,
                                            load_context->start_frame_no,
                                            load_request->fn_load,
                                            load_request->callback_context);
    if (status == SIXEL_OK || status == SIXEL_INTERRUPTED) {
        *animation_handled = 1;
        return status;
    }

    status = sixel_builtin_load_png_single_frame(load_request->chunk,
                                                 chunk_size,
                                                 frame,
                                                 stb_context,
                                                 ri,
                                                 load_request->fuse_palette,
                                                 load_request->reqcolors,
                                                 load_request->enable_cms,
                                                 png_keycolor_mode,
                                                 load_request->bgcolor);
    return status;
}

static SIXELSTATUS
sixel_builtin_load_stbi_nonpng_path(
    sixel_builtin_load_request_t const *load_request,
    sixel_frame_t *frame,
    int chunk_size,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int is_jpeg,
    int is_psd,
    int is_pic,
#if HAVE_LCMS2
    int is_tiff,
#endif
    unsigned char **icc_profile,
    size_t *icc_profile_length,
    unsigned char **psd_transparent_mask,
    size_t *psd_transparent_mask_size)
{
    if (load_request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return sixel_builtin_load_nonpng_single_frame(
        load_request->chunk,
        chunk_size,
        frame,
        stb_context,
        ri,
        load_request->fuse_palette,
        load_request->bgcolor,
        load_request->enable_cms,
        load_request->bmp_info40_mode,
        is_jpeg,
        is_psd,
        is_pic,
#if HAVE_LCMS2
        is_tiff,
#endif
        icc_profile,
        icc_profile_length,
        psd_transparent_mask,
        psd_transparent_mask_size);
}

static SIXELSTATUS
sixel_builtin_load_stbi_path(
    sixel_builtin_load_request_t const *load_request,
    sixel_builtin_load_context_t const *load_context,
    sixel_frame_t **pframe,
    stbi__context *stb_context,
    stbi__result_info *ri,
    int is_png,
    int is_jpeg,
    int is_psd,
    int is_pic,
#if HAVE_LCMS2
    int is_tiff,
#endif
    unsigned char **icc_profile,
    size_t *icc_profile_length,
    unsigned char **psd_transparent_mask,
    size_t *psd_transparent_mask_size,
    int *animation_handled)
{
    SIXELSTATUS status;
    int chunk_size;
    int png_keycolor_mode;

    status = SIXEL_FALSE;
    chunk_size = 0;
    png_keycolor_mode = 0;
    if (load_request == NULL ||
        load_request->chunk == NULL ||
        load_context == NULL ||
        pframe == NULL ||
        stb_context == NULL ||
        ri == NULL ||
        icc_profile == NULL ||
        icc_profile_length == NULL ||
        psd_transparent_mask == NULL ||
        psd_transparent_mask_size == NULL ||
        animation_handled == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *animation_handled = 0;
    status = sixel_builtin_prepare_frame_and_chunk_size(load_request->chunk,
                                                        pframe,
                                                        &chunk_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    stbi_allocator = load_request->chunk->allocator;
    if (is_png) {
        png_keycolor_mode = sixel_builtin_png_keycolor_mode_enabled(
            load_request->chunk,
            load_request->bgcolor,
            load_request->enable_cms);
        return sixel_builtin_load_stbi_png_path(load_request,
                                                load_context,
                                                *pframe,
                                                chunk_size,
                                                stb_context,
                                                ri,
                                                png_keycolor_mode,
                                                animation_handled);
    }
    return sixel_builtin_load_stbi_nonpng_path(
        load_request,
        *pframe,
        chunk_size,
        stb_context,
        ri,
        is_jpeg,
        is_psd,
        is_pic,
#if HAVE_LCMS2
        is_tiff,
#endif
        icc_profile,
        icc_profile_length,
        psd_transparent_mask,
        psd_transparent_mask_size);
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
    int bmp_info40_mode,
    sixel_load_image_function fn_load,
    void *context)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    sixel_frame_t *frame;
    stbi__context stb_context;
    stbi__result_info ri;
    int chunk_size;
    int is_png;
    int is_jpeg;
    int is_psd;
    int is_pic;
    unsigned char *icc_profile;
    size_t icc_profile_length;
    unsigned char *psd_transparent_mask;
    size_t psd_transparent_mask_size;
    sixel_builtin_load_request_t load_request;
    sixel_builtin_load_context_t load_context;
    sixel_builtin_decode_path_t decode_path;
    int animation_handled;
    int apply_start_frame;
    int pnm_pixelformat;
#if HAVE_LCMS2
    int is_tiff;
#endif

    status = SIXEL_BAD_INPUT;
    pixels = NULL;
    frame = NULL;
    stb_context = (stbi__context){ 0 };
    ri = (stbi__result_info){ 0 };
    chunk_size = 0;
    is_png = 0;
    is_jpeg = 0;
    is_psd = 0;
    is_pic = 0;
    icc_profile = NULL;
    icc_profile_length = 0u;
    psd_transparent_mask = NULL;
    psd_transparent_mask_size = 0u;
    load_request.chunk = pchunk;
    load_request.fstatic = fstatic;
    load_request.fuse_palette = fuse_palette;
    load_request.reqcolors = reqcolors;
    load_request.bgcolor = bgcolor;
    load_request.loop_control = loop_control;
    load_request.start_frame_no_set = start_frame_no_set;
    load_request.start_frame_no_override = start_frame_no_override;
    load_request.enable_cms = enable_cms;
    load_request.bmp_info40_mode = bmp_info40_mode;
    load_request.fn_load = fn_load;
    load_request.callback_context = context;
    memset(&load_context, 0, sizeof(load_context));
    decode_path = SIXEL_BUILTIN_DECODE_PATH_STBI;
    animation_handled = 0;
    apply_start_frame = 0;
    pnm_pixelformat = SIXEL_PIXELFORMAT_RGB888;
#if HAVE_LCMS2
    is_tiff = 0;
#endif

    decode_path = sixel_builtin_detect_decode_path(load_request.chunk);
    is_png = chunk_is_png(pchunk);
    is_jpeg = chunk_is_jpeg(pchunk);
    is_psd = chunk_is_psd(pchunk);
    is_pic = chunk_is_pic(pchunk);
#if HAVE_LCMS2
    is_tiff = chunk_is_tiff(pchunk);
#endif
    loader_trace_message("builtin loader: decode path=%s",
                         sixel_builtin_decode_path_name(decode_path));

    if (decode_path == SIXEL_BUILTIN_DECODE_PATH_GIF) {
        apply_start_frame = 1;
    } else if (decode_path == SIXEL_BUILTIN_DECODE_PATH_STBI &&
               is_png &&
               sixel_builtin_chunk_has_apng_control(pchunk)) {
        /* Only APNG should validate animation start-frame controls. */
        apply_start_frame = 1;
    }

    status = sixel_builtin_prepare_load_context(&load_request,
                                                &load_context,
                                                apply_start_frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    switch (decode_path) {
    case SIXEL_BUILTIN_DECODE_PATH_SIXEL:
        status = sixel_builtin_prepare_frame_and_chunk_size(pchunk,
                                                            &frame,
                                                            &chunk_size);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_sixel(&pixels,
                            pchunk->buffer,
                            chunk_size,
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
        break;

    case SIXEL_BUILTIN_DECODE_PATH_PNM:
        status = sixel_builtin_prepare_frame_and_chunk_size(pchunk,
                                                            &frame,
                                                            &chunk_size);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_pnm(pchunk->buffer,
                          chunk_size,
                          frame->allocator,
                          load_request.bgcolor,
                          &pixels,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          &pnm_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_frame_set_pixelformat(frame, pnm_pixelformat);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, pixels);
            pixels = NULL;
            goto end;
        }
        /*
         * Apply pixelformat metadata before attaching decoded storage.
         * sixel_frame_set_pixelformat() may normalize existing pixels, so
         * setting the pointer first can reinterpret float data as bytes.
         */
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pnm_pixelformat)) {
            sixel_frame_set_pixels_float32(frame, (float *)pixels);
        } else {
            sixel_frame_set_pixels(frame, pixels);
        }
        break;

    case SIXEL_BUILTIN_DECODE_PATH_GIF:
        status = sixel_builtin_load_gif_frames(&load_request, &load_context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;

    case SIXEL_BUILTIN_DECODE_PATH_STBI:
    default:
        status = sixel_builtin_load_stbi_path(&load_request,
                                              &load_context,
                                              &frame,
                                              &stb_context,
                                              &ri,
                                              is_png,
                                              is_jpeg,
                                              is_psd,
                                              is_pic,
#if HAVE_LCMS2
                                              is_tiff,
#endif
                                              &icc_profile,
                                              &icc_profile_length,
                                              &psd_transparent_mask,
                                              &psd_transparent_mask_size,
                                              &animation_handled);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (animation_handled != 0) {
            goto end;
        }
    }

    status = sixel_builtin_finalize_loaded_frame(&load_request, frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (psd_transparent_mask != NULL && pchunk != NULL) {
        sixel_allocator_free(pchunk->allocator, psd_transparent_mask);
    }
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
