/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2014-2025 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_UNISTD_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_IO_H
#include <io.h>
#endif  /* HAVE_IO_H */

#include "decoder.h"
#include "decoder-parallel.h"
#include "frame-factory.h"
#include "clipboard.h"
#include "compat_stub.h"
#include "path.h"
#include "options.h"
#include "cpu.h"
#include "sixel_atomic.h"
#include "threading.h"

#if defined(HAVE_NEON) && HAVE_NEON && \
    defined(HAVE_ARM_NEON_H) && HAVE_ARM_NEON_H && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__))
# include <arm_neon.h>
# define SIXEL_KUNDITHER_USE_NEON 1
#endif

static void
decoder_clipboard_select_format(char *dest,
                                size_t dest_size,
                                char const *format,
                                char const *fallback)
{
    char const *source;
    size_t limit;

    if (dest == NULL || dest_size == 0u) {
        return;
    }

    source = fallback;
    if (format != NULL && format[0] != '\0') {
        source = format;
    }

    limit = dest_size - 1u;
    if (limit == 0u) {
        dest[0] = '\0';
        return;
    }

    (void)snprintf(dest, dest_size, "%.*s", (int)limit, source);
}


static char *
decoder_create_temp_template_with_prefix(sixel_allocator_t *allocator,
                                         char const *prefix,
                                         size_t *capacity_out)
{
    char const *tmpdir;
    size_t tmpdir_len;
    size_t prefix_len;
    size_t suffix_len;
    size_t template_len;
    char *template_path;
    int needs_separator;
    size_t maximum_tmpdir_len;

#if defined(_WIN32)
    /*
     * MinGW runtimes under Wine can reject host-side TMPDIR values
     * (for example "/home/..."). Prefer TEMP/TMP first and then
     * fall back to TMPDIR.
     */
    tmpdir = sixel_compat_getenv("TEMP");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TMP");
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TMPDIR");
    }
#else
    tmpdir = sixel_compat_getenv("TMPDIR");
#endif
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#if defined(_WIN32)
        tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }

    tmpdir_len = strlen(tmpdir);
    prefix_len = strlen(prefix);
    suffix_len = prefix_len + strlen("-XXXXXX");
    maximum_tmpdir_len = (size_t)INT_MAX;

    if (maximum_tmpdir_len <= suffix_len + 2) {
        return NULL;
    }
    if (tmpdir_len > maximum_tmpdir_len - (suffix_len + 2)) {
        return NULL;
    }

    needs_separator = 1;
    if (tmpdir_len > 0) {
        if (tmpdir[tmpdir_len - 1] == '/' || tmpdir[tmpdir_len - 1] == '\\') {
            needs_separator = 0;
        }
    }

    template_len = tmpdir_len + suffix_len + 2;
    template_path = (char *)sixel_allocator_malloc(allocator, template_len);
    if (template_path == NULL) {
        return NULL;
    }

    if (needs_separator) {
#if defined(_WIN32)
        (void)snprintf(template_path, template_len,
                       "%s\\%s-XXXXXX", tmpdir, prefix);
#else
        (void)snprintf(template_path, template_len,
                       "%s/%s-XXXXXX", tmpdir, prefix);
#endif
    } else {
        (void)snprintf(template_path, template_len,
                       "%s%s-XXXXXX", tmpdir, prefix);
    }

    if (capacity_out != NULL) {
        *capacity_out = template_len;
    }

    return template_path;
}


static SIXELSTATUS
decoder_clipboard_create_spool(sixel_allocator_t *allocator,
                               char const *prefix,
                               char **path_out)
{
    SIXELSTATUS status;
    char *template_path;
    size_t template_capacity;
    int open_flags;
    int open_mode;
    int open_attempt;
    int open_errno;
    int fd;
    char *tmpname_result;

    status = SIXEL_FALSE;
    template_path = NULL;
    template_capacity = 0u;
    open_flags = 0;
    open_mode = 0;
    fd = (-1);
    tmpname_result = NULL;

    template_path = decoder_create_temp_template_with_prefix(allocator,
                                                             prefix,
                                                             &template_capacity);
    if (template_path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to allocate spool template.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
        tmpname_result = sixel_compat_tmpnam(template_path,
                                             template_capacity);
        if (tmpname_result == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to reserve spool template.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        template_capacity = strlen(template_path) + 1u;
    }

    open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_BINARY)
    open_flags |= O_BINARY;
#endif
    /*
     * Emscripten + NODERAWFS can report false EEXIST for O_EXCL on freshly
     * generated temp paths. Keep O_EXCL on native runtimes and drop it for
     * emscripten to preserve clipboard spool creation reliability.
     */
#if defined(O_EXCL) && !defined(__EMSCRIPTEN__)
    open_flags |= O_EXCL;
#endif
    open_mode = S_IRUSR | S_IWUSR;
    open_errno = 0;
    open_attempt = 0;
    for (open_attempt = 0; open_attempt < 4; ++open_attempt) {
        fd = sixel_compat_open(template_path, open_flags, open_mode);
        if (fd >= 0) {
            break;
        }
        open_errno = errno;
        if (open_errno != EEXIST) {
            break;
        }
        /*
         * Emscripten mktemp implementations can return reused names.
         * Regenerate the path and retry when the generated file already exists.
         */
        if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
            tmpname_result = sixel_compat_tmpnam(template_path,
                                                 template_capacity);
            if (tmpname_result == NULL) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to reserve spool template.");
                status = SIXEL_LIBC_ERROR;
                goto end;
            }
            template_capacity = strlen(template_path) + 1u;
        }
    }
    if (fd < 0) {
        if (open_errno != 0) {
            errno = open_errno;
        }
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file.");
        status = SIXEL_LIBC_ERROR;
        goto end;
    }

    *path_out = template_path;
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
        fd = (-1);
    }

    template_path = NULL;
    status = SIXEL_OK;

end:
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
    }
    if (template_path != NULL) {
        sixel_allocator_free(allocator, template_path);
    }

    return status;
}


static SIXELSTATUS
decoder_clipboard_read_file(char const *path,
                            unsigned char **data,
                            size_t *size)
{
    FILE *stream;
    long seek_result;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: read buffer pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    if (path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: spool path is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file for read.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_END);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to seek spool file.");
        return SIXEL_LIBC_ERROR;
    }

    file_size = ftell(stream);
    if (file_size < 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to determine spool size.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_SET);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to rewind spool file.");
        return SIXEL_LIBC_ERROR;
    }

    if (file_size == 0) {
        buffer = NULL;
        read_size = 0u;
    } else {
        buffer = (unsigned char *)malloc((size_t)file_size);
        if (buffer == NULL) {
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: malloc() failed for spool payload.");
            return SIXEL_BAD_ALLOCATION;
        }
        read_size = fread(buffer, 1u, (size_t)file_size, stream);
        if (read_size != (size_t)file_size) {
            free(buffer);
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: failed to read spool payload.");
            return SIXEL_LIBC_ERROR;
        }
    }

    if (fclose(stream) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        sixel_helper_set_additional_message(
            "clipboard: failed to close spool file after read.");
        return SIXEL_LIBC_ERROR;
    }

    *data = buffer;
    *size = read_size;

    return SIXEL_OK;
}


/* original version of strdup(3) with allocator object */
static char *
strdup_with_allocator(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;

    if (s == NULL || allocator == NULL) {
        return NULL;
    }
    p = (char *)sixel_allocator_malloc(allocator, (size_t)(strlen(s) + 1));
    if (p) {
        (void)sixel_compat_strcpy(p, strlen(s) + 1, s);
    }
    return p;
}


/* create decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_new(
    sixel_decoder_t    /* out */ **ppdecoder,  /* decoder object to be created */
    sixel_allocator_t  /* in */  *allocator)   /* allocator, null if you use
                                                  default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppdecoder = sixel_allocator_malloc(allocator, sizeof(sixel_decoder_t));
    if (*ppdecoder == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_decoder_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppdecoder)->ref          = 1U;
    (*ppdecoder)->output       = strdup_with_allocator("-", allocator);
    (*ppdecoder)->input        = strdup_with_allocator("-", allocator);
    (*ppdecoder)->allocator    = allocator;
    (*ppdecoder)->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    (*ppdecoder)->dequantize_similarity_bias = 100;
    (*ppdecoder)->dequantize_edge_strength = 0;
    (*ppdecoder)->thumbnail_size = 0;
    (*ppdecoder)->direct_color = 0;
    (*ppdecoder)->clipboard_input_active = 0;
    (*ppdecoder)->clipboard_output_active = 0;
    (*ppdecoder)->clipboard_input_format[0] = '\0';
    (*ppdecoder)->clipboard_output_format[0] = '\0';

    if ((*ppdecoder)->output == NULL || (*ppdecoder)->input == NULL) {
        sixel_decoder_unref(*ppdecoder);
        *ppdecoder = NULL;
        sixel_helper_set_additional_message(
            "sixel_decoder_new: strdup_with_allocator() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* deprecated version of sixel_decoder_new() */
SIXELAPI /* deprecated */ sixel_decoder_t *
sixel_decoder_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_decoder_t *decoder = NULL;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return decoder;
}


/* destroy a decoder object */
static void
sixel_decoder_destroy(sixel_decoder_t *decoder)
{
    sixel_allocator_t *allocator;

    if (decoder) {
        allocator = decoder->allocator;
        sixel_allocator_free(allocator, decoder->input);
        sixel_allocator_free(allocator, decoder->output);
        sixel_allocator_free(allocator, decoder);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of decoder object (thread-safe) */
SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder)
{
    if (decoder == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&decoder->ref, 1U);
}


/* decrease reference count of decoder object (thread-safe) */
SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder)
{
    unsigned int previous;

    if (decoder == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&decoder->ref, 1U);
    if (previous == 1U) {
        sixel_decoder_destroy(decoder);
    }
}


typedef struct sixel_similarity {
    const unsigned char *palette;
    int ncolors;
    int stride;
    signed char *cache;
    int bias;
#if defined(SIXEL_KUNDITHER_USE_NEON)
    int simd_level;
#endif
} sixel_similarity_t;

typedef struct sixel_kundither_filter_context {
    unsigned char const *indexed_pixels;
    int width;
    int height;
    unsigned char const *palette;
    int ncolors;
    sixel_similarity_t *similarity;
    unsigned char *rgb;
    int y_start;
    int y_end;
} sixel_kundither_filter_context_t;

static const int g_kundither_neighbor_offsets[8][4] = {
    {-1, -1,  10, 16}, {0, -1, 16, 16}, {1, -1,   6, 16},
    {-1,  0,  11, 16},                  {1,  0,  11, 16},
    {-1,  1,   6, 16}, {0,  1, 16, 16}, {1,  1,  10, 16}
};

static SIXELSTATUS
sixel_similarity_init(sixel_similarity_t *similarity,
                      const unsigned char *palette,
                      int ncolors,
                      int bias,
                      sixel_allocator_t *allocator)
{
    size_t cache_size;
    int i;

    if (bias < 1) {
        bias = 1;
    }

    similarity->palette = palette;
    similarity->ncolors = ncolors;
    similarity->stride = ncolors;
    similarity->bias = bias;
#if defined(SIXEL_KUNDITHER_USE_NEON)
    similarity->simd_level = SIXEL_SIMD_LEVEL_SCALAR;
#endif

    cache_size = (size_t)ncolors * (size_t)ncolors;
    if (cache_size == 0) {
        similarity->cache = NULL;
        return SIXEL_OK;
    }

    similarity->cache = (signed char *)sixel_allocator_malloc(
        allocator,
        cache_size);
    if (similarity->cache == NULL) {
        sixel_helper_set_additional_message(
            "sixel_similarity_init: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memset(similarity->cache, -1, cache_size);
    for (i = 0; i < ncolors; ++i) {
        similarity->cache[i * similarity->stride + i] = 7;
    }

    return SIXEL_OK;
}

static void
sixel_similarity_destroy(sixel_similarity_t *similarity,
                         sixel_allocator_t *allocator)
{
    if (similarity->cache != NULL) {
        sixel_allocator_free(allocator, similarity->cache);
        similarity->cache = NULL;
    }
}

static inline unsigned int
sixel_similarity_diff(const unsigned char *a, const unsigned char *b)
{
    int dr = (int)a[0] - (int)b[0];
    int dg = (int)a[1] - (int)b[1];
    int db = (int)a[2] - (int)b[2];
    return (unsigned int)(dr * dr + dg * dg + db * db);
}

static inline unsigned int
sixel_similarity_min_diff_scalar(const unsigned char *palette,
                                 int ncolors,
                                 int index1,
                                 int index2,
                                 const unsigned char *avg_color)
{
    unsigned int min_diff;
    const unsigned char *pk;
    unsigned int diff;
    int i;

    min_diff = UINT_MAX;
    for (i = 0; i < ncolors; ++i) {
        if (i == index1 || i == index2) {
            continue;
        }
        pk = palette + i * 3;
        diff = sixel_similarity_diff(avg_color, pk);
        if (diff < min_diff) {
            min_diff = diff;
        }
    }

    return min_diff;
}

#if defined(SIXEL_KUNDITHER_USE_NEON)
static inline unsigned int
sixel_similarity_min_diff_neon(const unsigned char *palette,
                               int ncolors,
                               int index1,
                               int index2,
                               const unsigned char *avg_color)
{
    uint16x8_t avg_r_vec;
    uint16x8_t avg_g_vec;
    uint16x8_t avg_b_vec;
    uint32x4_t min_vec;
    uint8x8x3_t colors;
    uint16x8_t r_u16;
    uint16x8_t g_u16;
    uint16x8_t b_u16;
    int16x8_t dr;
    int16x8_t dg;
    int16x8_t db;
    int32x4_t r2_lo;
    int32x4_t r2_hi;
    int32x4_t g2_lo;
    int32x4_t g2_hi;
    int32x4_t b2_lo;
    int32x4_t b2_hi;
    uint32x4_t sum_lo;
    uint32x4_t sum_hi;
    uint32_t lanes[4];
    unsigned int min_diff;
    const unsigned char *pk;
    unsigned int diff;
    int i;
    int k;

    min_diff = UINT_MAX;
    avg_r_vec = vdupq_n_u16((uint16_t)avg_color[0]);
    avg_g_vec = vdupq_n_u16((uint16_t)avg_color[1]);
    avg_b_vec = vdupq_n_u16((uint16_t)avg_color[2]);
    min_vec = vdupq_n_u32(UINT_MAX);
    i = 0;

    /*
     * vld3_u8 maps the interleaved RGB palette to three byte vectors.  Chunks
     * containing either endpoint fall back to scalar so the historical
     * "ignore index1/index2" rule stays byte-for-byte identical.
     */
    for (; i + 8 <= ncolors; i += 8) {
        if ((index1 >= i && index1 < i + 8) ||
            (index2 >= i && index2 < i + 8)) {
            for (k = i; k < i + 8; ++k) {
                if (k == index1 || k == index2) {
                    continue;
                }
                pk = palette + k * 3;
                diff = sixel_similarity_diff(avg_color, pk);
                if (diff < min_diff) {
                    min_diff = diff;
                }
            }
            continue;
        }

        colors = vld3_u8(palette + i * 3);
        r_u16 = vmovl_u8(colors.val[0]);
        g_u16 = vmovl_u8(colors.val[1]);
        b_u16 = vmovl_u8(colors.val[2]);
        dr = vreinterpretq_s16_u16(vsubq_u16(r_u16, avg_r_vec));
        dg = vreinterpretq_s16_u16(vsubq_u16(g_u16, avg_g_vec));
        db = vreinterpretq_s16_u16(vsubq_u16(b_u16, avg_b_vec));

        r2_lo = vmull_s16(vget_low_s16(dr), vget_low_s16(dr));
        r2_hi = vmull_s16(vget_high_s16(dr), vget_high_s16(dr));
        g2_lo = vmull_s16(vget_low_s16(dg), vget_low_s16(dg));
        g2_hi = vmull_s16(vget_high_s16(dg), vget_high_s16(dg));
        b2_lo = vmull_s16(vget_low_s16(db), vget_low_s16(db));
        b2_hi = vmull_s16(vget_high_s16(db), vget_high_s16(db));

        sum_lo = vreinterpretq_u32_s32(vaddq_s32(
            vaddq_s32(r2_lo, g2_lo),
            b2_lo));
        sum_hi = vreinterpretq_u32_s32(vaddq_s32(
            vaddq_s32(r2_hi, g2_hi),
            b2_hi));
        min_vec = vminq_u32(min_vec, sum_lo);
        min_vec = vminq_u32(min_vec, sum_hi);
    }

    vst1q_u32(lanes, min_vec);
    if (lanes[0] < min_diff) {
        min_diff = lanes[0];
    }
    if (lanes[1] < min_diff) {
        min_diff = lanes[1];
    }
    if (lanes[2] < min_diff) {
        min_diff = lanes[2];
    }
    if (lanes[3] < min_diff) {
        min_diff = lanes[3];
    }

    for (; i < ncolors; ++i) {
        if (i == index1 || i == index2) {
            continue;
        }
        pk = palette + i * 3;
        diff = sixel_similarity_diff(avg_color, pk);
        if (diff < min_diff) {
            min_diff = diff;
        }
    }

    return min_diff;
}
#endif

static unsigned int
sixel_similarity_compare(sixel_similarity_t *similarity,
                         int index1,
                         int index2,
                         int numerator,
                         int denominator)
{
    int min_index;
    int max_index;
    size_t cache_pos;
    signed char cached;
    const unsigned char *palette;
    const unsigned char *p1;
    const unsigned char *p2;
    unsigned char avg_color[3];
    unsigned int distance;
    unsigned int base_distance;
    unsigned long long scaled_distance;
    int bias;
    unsigned int min_diff;
    unsigned int result;

    if (similarity->cache == NULL) {
        return 0;
    }

    if (index1 < 0 || index1 >= similarity->ncolors ||
        index2 < 0 || index2 >= similarity->ncolors) {
        return 0;
    }

    if (index1 <= index2) {
        min_index = index1;
        max_index = index2;
    } else {
        min_index = index2;
        max_index = index1;
    }

    cache_pos = (size_t)min_index * (size_t)similarity->stride
              + (size_t)max_index;
    cached = similarity->cache[cache_pos];
    if (cached >= 0) {
        return (unsigned int)cached;
    }

    palette = similarity->palette;
    p1 = palette + index1 * 3;
    p2 = palette + index2 * 3;

#if 1
   /*    original: n = (p1 + p2) / 2
    */
    avg_color[0] = (unsigned char)(((unsigned int)p1[0]
                                    + (unsigned int)p2[0]) >> 1);
    avg_color[1] = (unsigned char)(((unsigned int)p1[1]
                                    + (unsigned int)p2[1]) >> 1);
    avg_color[2] = (unsigned char)(((unsigned int)p1[2]
                                    + (unsigned int)p2[2]) >> 1);
    (void) numerator;
    (void) denominator;
#else
   /*
    *    diffuse(pos_a, n1) -> p1
    *    diffuse(pos_b, n2) -> p2
    *
    *    when n1 == n2 == n:
    *
    *    p2 = n + (n - p1) * numerator / denominator
    * => p2 * denominator = n * denominator + (n - p1) * numerator
    * => p2 * denominator = n * denominator + n * numerator - p1 * numerator
    * => n * (denominator + numerator) = p1 * numerator + p2 * denominator
    * => n = (p1 * numerator + p2 * denominator) / (denominator + numerator)
    *
    */
    avg_color[0] = (p1[0] * numerator + p2[0] * denominator + (numerator + denominator + 0.5) / 2)
                 / (numerator + denominator);
    avg_color[1] = (p1[1] * numerator + p2[1] * denominator + (numerator + denominator + 0.5) / 2)
                 / (numerator + denominator);
    avg_color[2] = (p1[2] * numerator + p2[2] * denominator + (numerator + denominator + 0.5) / 2)
                 / (numerator + denominator);
#endif

    distance = sixel_similarity_diff(avg_color, p1);
    bias = similarity->bias;
    if (bias < 1) {
        bias = 1;
    }
    scaled_distance = (unsigned long long)distance
                    * (unsigned long long)bias
                    + 50ULL;
    base_distance = (unsigned int)(scaled_distance / 100ULL);
    if (base_distance == 0U) {
        base_distance = 1U;
    }

#if defined(SIXEL_KUNDITHER_USE_NEON)
    if (similarity->simd_level >= SIXEL_SIMD_LEVEL_NEON) {
        min_diff = sixel_similarity_min_diff_neon(
            palette,
            similarity->ncolors,
            index1,
            index2,
            avg_color);
    } else {
        min_diff = sixel_similarity_min_diff_scalar(
            palette,
            similarity->ncolors,
            index1,
            index2,
            avg_color);
    }
#else
    min_diff = sixel_similarity_min_diff_scalar(
        palette,
        similarity->ncolors,
        index1,
        index2,
        avg_color);
#endif

    if (min_diff == UINT_MAX) {
        min_diff = base_distance * 2U;
    }

    if (min_diff >= base_distance * 2U) {
        result = 5U;
    } else if (min_diff >= base_distance) {
        result = 8U;
    } else if ((unsigned long long)min_diff * 6ULL
               >= (unsigned long long)base_distance * 5ULL) {
        result = 7U;
    } else if ((unsigned long long)min_diff * 4ULL
               >= (unsigned long long)base_distance * 3ULL) {
        result = 7U;
    } else if ((unsigned long long)min_diff * 3ULL
               >= (unsigned long long)base_distance * 2ULL) {
        result = 5U;
    } else if ((unsigned long long)min_diff * 5ULL
               >= (unsigned long long)base_distance * 3ULL) {
        result = 7U;
    } else if ((unsigned long long)min_diff * 2ULL
               >= (unsigned long long)base_distance * 1ULL) {
        result = 4U;
    } else if ((unsigned long long)min_diff * 3ULL
               >= (unsigned long long)base_distance * 1ULL) {
        result = 2U;
    } else {
        result = 0U;
    }

    similarity->cache[cache_pos] = (signed char)result;

    return result;
}

static void
sixel_similarity_enable_simd(sixel_similarity_t *similarity)
{
#if defined(SIXEL_KUNDITHER_USE_NEON)
    if (similarity != NULL && similarity->ncolors >= 80) {
        /*
         * NEON only amortizes in the precomputed high-color path.  Lazy scalar
         * decoding is dominated by cache-hit lookups, and 32-64 color images
         * benchmark better with the compiler's scalar loop on Apple Silicon.
         */
        similarity->simd_level = sixel_cpu_simd_level();
    }
#else
    (void)similarity;
#endif
}

static void
sixel_similarity_prepare_image_order(sixel_similarity_t *similarity,
                                     unsigned char const *indexed_pixels,
                                     int width,
                                     int height,
                                     int ncolors)
{
    size_t prepared_pairs;
    size_t target_pairs;
    size_t cache_pos;
    int palette_index;
    int neighbor_index;
    int min_index;
    int max_index;
    int neighbor;
    int numerator;
    int denominator;
    int nx;
    int ny;
    int x;
    int y;

    if (similarity == NULL || similarity->cache == NULL) {
        return;
    }

    /*
     * The similarity cache is indexed by an unordered palette pair, but the
     * midpoint rounding in sixel_similarity_compare() is historically order
     * sensitive.  Prepare cache entries by walking the image in scalar scan
     * order so parallel workers read the same first-seen orientation.
     */
    prepared_pairs = 0u;
    target_pairs = ((size_t)ncolors * (size_t)(ncolors - 1)) / 2u;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            palette_index = indexed_pixels[y * width + x];
            if (palette_index < 0 || palette_index >= ncolors) {
                palette_index = 0;
            }

            for (neighbor = 0; neighbor < 8; ++neighbor) {
                nx = x + g_kundither_neighbor_offsets[neighbor][0];
                ny = y + g_kundither_neighbor_offsets[neighbor][1];
                numerator = g_kundither_neighbor_offsets[neighbor][2];
                denominator = g_kundither_neighbor_offsets[neighbor][3];

                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    continue;
                }

                neighbor_index = indexed_pixels[ny * width + nx];
                if (neighbor_index < 0 || neighbor_index >= ncolors) {
                    continue;
                }
                if (palette_index == neighbor_index) {
                    continue;
                }

                if (palette_index <= neighbor_index) {
                    min_index = palette_index;
                    max_index = neighbor_index;
                } else {
                    min_index = neighbor_index;
                    max_index = palette_index;
                }

                cache_pos = (size_t)min_index * (size_t)similarity->stride
                          + (size_t)max_index;
                if (similarity->cache[cache_pos] >= 0) {
                    continue;
                }

                (void)sixel_similarity_compare(
                    similarity,
                    palette_index,
                    neighbor_index,
                    numerator,
                    denominator);
                prepared_pairs += 1u;
                if (prepared_pairs >= target_pairs) {
                    return;
                }
            }
        }
    }
}

static inline unsigned int
sixel_similarity_cached_compare(sixel_similarity_t *similarity,
                                int index1,
                                int index2,
                                int numerator,
                                int denominator)
{
    int min_index;
    int max_index;
    size_t cache_pos;
    signed char cached;

    if (similarity == NULL || similarity->cache == NULL) {
        return 0U;
    }
    if (index1 < 0 || index1 >= similarity->ncolors ||
        index2 < 0 || index2 >= similarity->ncolors) {
        return 0U;
    }

    if (index1 <= index2) {
        min_index = index1;
        max_index = index2;
    } else {
        min_index = index2;
        max_index = index1;
    }

    cache_pos = (size_t)min_index * (size_t)similarity->stride
              + (size_t)max_index;
    cached = similarity->cache[cache_pos];
    if (cached >= 0) {
        return (unsigned int)cached;
    }

    return sixel_similarity_compare(
        similarity,
        index1,
        index2,
        numerator,
        denominator);
}

static inline int
sixel_clamp(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static inline int
sixel_get_gray(const int *gray, int width, int height, int x, int y)
{
    int cx = sixel_clamp(x, 0, width - 1);
    int cy = sixel_clamp(y, 0, height - 1);
    return gray[cy * width + cx];
}

static unsigned short
sixel_prewitt_value(const int *gray, int width, int height, int x, int y)
{
    int top_prev = sixel_get_gray(gray, width, height, x - 1, y - 1);
    int top_curr = sixel_get_gray(gray, width, height, x, y - 1);
    int top_next = sixel_get_gray(gray, width, height, x + 1, y - 1);
    int mid_prev = sixel_get_gray(gray, width, height, x - 1, y);
    int mid_next = sixel_get_gray(gray, width, height, x + 1, y);
    int bot_prev = sixel_get_gray(gray, width, height, x - 1, y + 1);
    int bot_curr = sixel_get_gray(gray, width, height, x, y + 1);
    int bot_next = sixel_get_gray(gray, width, height, x + 1, y + 1);
    long gx = (long)top_next - (long)top_prev +
              (long)mid_next - (long)mid_prev +
              (long)bot_next - (long)bot_prev;
    long gy = (long)bot_prev + (long)bot_curr + (long)bot_next -
              (long)top_prev - (long)top_curr - (long)top_next;
    unsigned long long ux;
    unsigned long long uy;
    unsigned long long magnitude;

    /*
     * gx and gy are signed Prewitt gradients. Convert their absolute values
     * before squaring so unsigned-overflow sanitizers do not see the modulo
     * product that comes from casting a negative long directly to unsigned.
     */
    if (gx < 0L) {
        ux = (unsigned long long)(-gx);
    } else {
        ux = (unsigned long long)gx;
    }
    if (gy < 0L) {
        uy = (unsigned long long)(-gy);
    } else {
        uy = (unsigned long long)gy;
    }

    magnitude = ux * ux + uy * uy;
    magnitude /= 256ULL;
    if (magnitude > 65535ULL) {
        magnitude = 65535ULL;
    }
    return (unsigned short)magnitude;
}

static unsigned short
sixel_scale_threshold(unsigned short base, int percent)
{
    unsigned long long numerator;
    unsigned long long scaled;

    if (percent <= 0) {
        percent = 1;
    }

    numerator = (unsigned long long)base * 100ULL
              + (unsigned long long)percent / 2ULL;
    scaled = numerator / (unsigned long long)percent;
    if (scaled == 0ULL) {
        scaled = 1ULL;
    }
    if (scaled > USHRT_MAX) {
        scaled = USHRT_MAX;
    }

    return (unsigned short)scaled;
}

static inline void
sixel_kundither_filter_noedge_range(unsigned char const *indexed_pixels,
                                    int width,
                                    int height,
                                    unsigned char const *palette,
                                    int ncolors,
                                    sixel_similarity_t *similarity,
                                    unsigned char *rgb,
                                    int y_start,
                                    int y_end)
{
    const unsigned char *color;
    size_t out_index;
    int palette_index;
    unsigned int total_weight;
    unsigned int accum_r;
    unsigned int accum_g;
    unsigned int accum_b;
    int neighbor;
    int nx;
    int ny;
    int numerator;
    int denominator;
    unsigned int weight;
    const unsigned char *neighbor_color;
    int neighbor_index;
    int x;
    int y;

    /*
     * This path is intentionally independent per output pixel.  The edge
     * preserving mode below has historical scan-order behaviour, but the
     * common no-edge filter only depends on the indexed source and palette.
     */
    for (y = y_start; y < y_end; ++y) {
        for (x = 0; x < width; ++x) {
            palette_index = indexed_pixels[y * width + x];
            if (palette_index < 0 || palette_index >= ncolors) {
                palette_index = 0;
            }

            color = palette + palette_index * 3;
            accum_r = (unsigned int)color[0] * 8U;
            accum_g = (unsigned int)color[1] * 8U;
            accum_b = (unsigned int)color[2] * 8U;
            total_weight = 8U;

            for (neighbor = 0; neighbor < 8; ++neighbor) {
                nx = x + g_kundither_neighbor_offsets[neighbor][0];
                ny = y + g_kundither_neighbor_offsets[neighbor][1];
                numerator = g_kundither_neighbor_offsets[neighbor][2];
                denominator = g_kundither_neighbor_offsets[neighbor][3];

                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    continue;
                }

                neighbor_index = indexed_pixels[ny * width + nx];
                if (neighbor_index < 0 || neighbor_index >= ncolors) {
                    continue;
                }

                weight = sixel_similarity_cached_compare(
                    similarity,
                    palette_index,
                    neighbor_index,
                    numerator,
                    denominator);
                if (weight == 0U) {
                    continue;
                }

                neighbor_color = palette + neighbor_index * 3;
                accum_r += (unsigned int)neighbor_color[0] * weight;
                accum_g += (unsigned int)neighbor_color[1] * weight;
                accum_b += (unsigned int)neighbor_color[2] * weight;
                total_weight += weight;
            }

            out_index = (size_t)(y * width + x) * 3U;
            rgb[out_index + 0] = (unsigned char)(accum_r / total_weight);
            rgb[out_index + 1] = (unsigned char)(accum_g / total_weight);
            rgb[out_index + 2] = (unsigned char)(accum_b / total_weight);
        }
    }
}

#if SIXEL_ENABLE_THREADS
static int
sixel_kundither_filter_noedge_worker(void *arg)
{
    sixel_kundither_filter_context_t *context;

    context = (sixel_kundither_filter_context_t *)arg;
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_kundither_filter_noedge_range(
        context->indexed_pixels,
        context->width,
        context->height,
        context->palette,
        context->ncolors,
        context->similarity,
        context->rgb,
        context->y_start,
        context->y_end);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kundither_filter_noedge_parallel(unsigned char const *indexed_pixels,
                                       int width,
                                       int height,
                                       unsigned char const *palette,
                                       int ncolors,
                                       sixel_similarity_t *similarity,
                                       unsigned char *rgb,
                                       int threads)
{
    sixel_thread_t *workers;
    sixel_kundither_filter_context_t *contexts;
    size_t num_pixels;
    int rows_per_thread;
    int y_start;
    int y_end;
    int created;
    int i;
    int status;
    int failed;

    workers = NULL;
    contexts = NULL;
    num_pixels = (size_t)width * (size_t)height;
    rows_per_thread = 0;
    y_start = 0;
    y_end = 0;
    created = 0;
    status = SIXEL_FALSE;
    failed = 0;

    if (threads < 2 || num_pixels < 65536U) {
        return SIXEL_FALSE;
    }
    if (threads > height) {
        threads = height;
    }
    if (threads < 2) {
        return SIXEL_FALSE;
    }

    sixel_similarity_enable_simd(similarity);
    sixel_similarity_prepare_image_order(
        similarity,
        indexed_pixels,
        width,
        height,
        ncolors);

    workers = (sixel_thread_t *)calloc((size_t)threads,
                                       sizeof(sixel_thread_t));
    contexts = (sixel_kundither_filter_context_t *)calloc(
        (size_t)threads,
        sizeof(sixel_kundither_filter_context_t));
    if (workers == NULL || contexts == NULL) {
        free(workers);
        free(contexts);
        return SIXEL_FALSE;
    }

    rows_per_thread = (height + threads - 1) / threads;
    for (i = 0; i < threads; ++i) {
        y_start = i * rows_per_thread;
        y_end = y_start + rows_per_thread;
        if (y_start >= height) {
            break;
        }
        if (y_end > height) {
            y_end = height;
        }

        contexts[i].indexed_pixels = indexed_pixels;
        contexts[i].width = width;
        contexts[i].height = height;
        contexts[i].palette = palette;
        contexts[i].ncolors = ncolors;
        contexts[i].similarity = similarity;
        contexts[i].rgb = rgb;
        contexts[i].y_start = y_start;
        contexts[i].y_end = y_end;

        status = sixel_thread_create(
            &workers[i],
            sixel_kundither_filter_noedge_worker,
            &contexts[i]);
        if (SIXEL_FAILED(status)) {
            failed = 1;
            break;
        }
        created += 1;
    }

    for (i = 0; i < created; ++i) {
        sixel_thread_join(&workers[i]);
    }

    free(workers);
    free(contexts);

    if (failed || created < 2) {
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}
#endif  /* SIXEL_ENABLE_THREADS */

static SIXELSTATUS
sixel_dequantize_k_undither_scalar(unsigned char *indexed_pixels,
                                   int width,
                                   int height,
                                   unsigned char *palette,
                                   int ncolors,
                                   int similarity_bias,
                                   int edge_strength,
                                   sixel_allocator_t *allocator,
                                   unsigned char **output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *rgb = NULL;
    int *gray = NULL;
    unsigned short *prewitt = NULL;
    sixel_similarity_t similarity;
    size_t num_pixels;
    int x;
    int y;
    unsigned short strong_threshold;
    unsigned short detail_threshold;
    static const int neighbor_offsets[8][4] = {
        {-1, -1,  10, 16}, {0, -1, 16, 16}, {1, -1,   6, 16},
        {-1,  0,  11, 16},                  {1,  0,  11, 16},
        {-1,  1,   6, 16}, {0,  1, 16, 16}, {1,  1,  10, 16}
    };
    const unsigned char *color;
    size_t out_index;
    int palette_index;
    unsigned int center_weight;
    unsigned int total_weight = 0;
    unsigned int accum_r;
    unsigned int accum_g;
    unsigned int accum_b;
    unsigned short gradient;
    int neighbor;
    int nx;
    int ny;
    int numerator;
    int denominator;
    unsigned int weight;
    const unsigned char *neighbor_color;
    int neighbor_index;

    if (width <= 0 || height <= 0 || palette == NULL || ncolors <= 0) {
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)width > ((size_t)-1 / (size_t)height)) {
        return SIXEL_BAD_ALLOCATION;
    }

    num_pixels = (size_t)width * (size_t)height;

    memset(&similarity, 0, sizeof(sixel_similarity_t));

    strong_threshold = sixel_scale_threshold(256U, edge_strength);
    detail_threshold = sixel_scale_threshold(160U, edge_strength);
    if (strong_threshold < detail_threshold) {
        strong_threshold = detail_threshold;
    }

    /*
     * Build RGB and luminance buffers so we can reuse the similarity cache
     * and gradient analysis across the reconstructed image.
     */
    rgb = (unsigned char *)sixel_allocator_malloc(
        allocator,
        num_pixels * 3);
    if (rgb == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_k_undither: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    gray = (int *)sixel_allocator_malloc(
        allocator,
        num_pixels * sizeof(int));
    if (gray == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_k_undither: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    prewitt = (unsigned short *)sixel_allocator_malloc(
        allocator,
        num_pixels * sizeof(unsigned short));
    if (prewitt == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_k_undither: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Pre-compute palette distance heuristics so each neighbour lookup reuses
     * the k_undither-style similarity table.
     */
    status = sixel_similarity_init(
        &similarity,
        palette,
        ncolors,
        similarity_bias,
        allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            palette_index = indexed_pixels[y * width + x];
            if (palette_index < 0 || palette_index >= ncolors) {
                palette_index = 0;
            }

            color = palette + palette_index * 3;
            out_index = (size_t)(y * width + x) * 3;
            rgb[out_index + 0] = color[0];
            rgb[out_index + 1] = color[1];
            rgb[out_index + 2] = color[2];

            if (edge_strength > 0) {
                gray[y * width + x] = (int)color[0]
                                    + (int)color[1] * 2
                                    + (int)color[2];
            }
        }
    }

    if (edge_strength > 0) {
        /*
         * Prewitt samples neighbouring pixels, including rows and columns that
         * appear later in scan order.  Fill the whole luminance buffer before
         * computing gradients so the edge decision is deterministic under
         * sanitizer and threaded builds.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                prewitt[y * width + x] = sixel_prewitt_value(
                    gray,
                    width,
                    height,
                    x,
                    y);
            }
        }
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            palette_index = indexed_pixels[y * width + x];
            if (palette_index < 0 || palette_index >= ncolors) {
                palette_index = 0;
            }

            out_index = (size_t)(y * width + x) * 3;
            if (edge_strength > 0) {
                gradient = prewitt[y * width + x];
                if (gradient > strong_threshold) {
                    continue;
                }

                if (gradient > detail_threshold) {
                    center_weight = 24U;
                } else {
                    center_weight = 8U;
                }
            } else {
                center_weight = 8U;
            }

            accum_r = (unsigned int)rgb[out_index + 0] * center_weight;
            accum_g = (unsigned int)rgb[out_index + 1] * center_weight;
            accum_b = (unsigned int)rgb[out_index + 2] * center_weight;
            total_weight = center_weight;

            /*
             * Blend neighbours that stay within the palette similarity
             * threshold so Floyd-Steinberg noise is averaged away without
             * bleeding across pronounced edges.
             */
            for (neighbor = 0; neighbor < 8; ++neighbor) {
                nx = x + neighbor_offsets[neighbor][0];
                ny = y + neighbor_offsets[neighbor][1];
                numerator = neighbor_offsets[neighbor][2];
                denominator = neighbor_offsets[neighbor][3];

                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    continue;
                }

                neighbor_index = indexed_pixels[ny * width + nx];
                if (neighbor_index < 0 || neighbor_index >= ncolors) {
                    continue;
                }

                if (numerator) {
                    weight = sixel_similarity_cached_compare(
                        &similarity,
                        palette_index,
                        neighbor_index,
                        numerator,
                        denominator);
                    if (weight == 0) {
                        continue;
                    }

                    neighbor_color = palette + neighbor_index * 3;
                    accum_r += (unsigned int)neighbor_color[0] * weight;
                    accum_g += (unsigned int)neighbor_color[1] * weight;
                    accum_b += (unsigned int)neighbor_color[2] * weight;
                    total_weight += weight;
                }
            }

            if (total_weight > 0U) {
                rgb[out_index + 0] = (unsigned char)(accum_r / total_weight);
                rgb[out_index + 1] = (unsigned char)(accum_g / total_weight);
                rgb[out_index + 2] = (unsigned char)(accum_b / total_weight);
            }
        }
    }


    *output = rgb;
    rgb = NULL;
    status = SIXEL_OK;

end:
    sixel_similarity_destroy(&similarity, allocator);
    sixel_allocator_free(allocator, rgb);
    sixel_allocator_free(allocator, gray);
    sixel_allocator_free(allocator, prewitt);
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither(unsigned char *indexed_pixels,
                            int width,
                            int height,
                            unsigned char *palette,
                            int ncolors,
                            int similarity_bias,
                            int edge_strength,
                            sixel_allocator_t *allocator,
                            unsigned char **output)
{
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
    sixel_similarity_t similarity;
    unsigned char *rgb;
    size_t num_pixels;
    SIXELSTATUS parallel_status;
    int parallel_threads;

    memset(&similarity, 0, sizeof(sixel_similarity_t));
    rgb = NULL;
    parallel_threads = 1;

    /*
     * The no-edge filter reads only the source index image and palette, so
     * row bands are independent.  Edge-preserving mode intentionally keeps
     * the historical scalar scan order because its gradient buffer is filled
     * as the loop advances.
     */
    if (edge_strength <= 0 &&
        width > 0 &&
        height > 0 &&
        palette != NULL &&
        ncolors > 0 &&
        (size_t)width <= ((size_t)-1 / (size_t)height)) {
        num_pixels = (size_t)width * (size_t)height;
        parallel_threads = sixel_threads_resolve();
        if (parallel_threads >= 2 && num_pixels >= 65536U) {
            rgb = (unsigned char *)sixel_allocator_malloc(
                allocator,
                num_pixels * 3);
            if (rgb == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_dequantize_k_undither: "
                    "sixel_allocator_malloc() failed.");
                return SIXEL_BAD_ALLOCATION;
            }

            status = sixel_similarity_init(
                &similarity,
                palette,
                ncolors,
                similarity_bias,
                allocator);
            if (SIXEL_FAILED(status)) {
                sixel_allocator_free(allocator, rgb);
                return status;
            }

            parallel_status = sixel_kundither_filter_noedge_parallel(
                indexed_pixels,
                width,
                height,
                palette,
                ncolors,
                &similarity,
                rgb,
                parallel_threads);
            if (parallel_status == SIXEL_OK) {
                sixel_similarity_destroy(&similarity, allocator);
                *output = rgb;
                return SIXEL_OK;
            }

            sixel_similarity_destroy(&similarity, allocator);
            sixel_allocator_free(allocator, rgb);
        }
    }
#endif

    return sixel_dequantize_k_undither_scalar(
        indexed_pixels,
        width,
        height,
        palette,
        ncolors,
        similarity_bias,
        edge_strength,
        allocator,
        output);
}
/*
 * The dequantizer accepts a method supplied by the shared option helper. The
 * decoder keeps a parallel lookup table that translates the matched index
 * into the execution constant.
 */
static int const g_decoder_dequant_methods[] = {
    SIXEL_DEQUANTIZE_NONE,
    SIXEL_DEQUANTIZE_K_UNDITHER
};

static sixel_option_choice_t const g_decoder_dequant_choices[] = {
    { "none", 0 },
    { "k_undither", 1 }
};

/* set an option flag to decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_setopt(
    sixel_decoder_t /* in */ *decoder,
    int             /* in */ arg,
    char const      /* in */ *value
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int path_flags;
    int path_check;
    char const *payload = NULL;
    sixel_clipboard_spec_t clipboard_spec;
    int match_index;
    sixel_option_choice_result_t match_result;
    char match_detail[128];
    char match_message[256];
    char const *filename = NULL;
    size_t libc_buffer_size;
    char *libc_buffer;
    char const *libc_path;
    long bias;
    long parsed_value;
    char *endptr;

    sixel_decoder_ref(decoder);
    path_flags = 0u;
    path_check = 0;
    libc_buffer_size = 0u;
    libc_buffer = NULL;
    libc_path = NULL;

    switch(arg) {
    case SIXEL_OPTFLAG_INPUT:  /* i */
        path_flags = SIXEL_OPTION_PATH_ALLOW_STDIN |
            SIXEL_OPTION_PATH_ALLOW_CLIPBOARD |
            SIXEL_OPTION_PATH_ALLOW_REMOTE;
        if (value != NULL) {
            path_check = sixel_option_validate_filesystem_path(
                value,
                value,
                path_flags);
            if (path_check != 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        decoder->clipboard_input_active = 0;
        decoder->clipboard_input_format[0] = '\0';
        if (value != NULL) {
            clipboard_spec.is_clipboard = 0;
            clipboard_spec.format[0] = '\0';
            if (sixel_clipboard_parse_spec(value, &clipboard_spec)
                    && clipboard_spec.is_clipboard) {
                decoder_clipboard_select_format(
                    decoder->clipboard_input_format,
                    sizeof(decoder->clipboard_input_format),
                    clipboard_spec.format,
                    "sixel");
                decoder->clipboard_input_active = 1;
            }
        }
        free(decoder->input);
        decoder->input = strdup_with_allocator(value, decoder->allocator);
        if (decoder->input == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_OUTPUT:  /* o */
        decoder->clipboard_output_active = 0;
        decoder->clipboard_output_format[0] = '\0';

        payload = value;
        if (strncmp(value, "png:", 4) == 0) {
            payload = value + 4;
            if (payload[0] == '\0') {
                sixel_helper_set_additional_message(
                    "missing target after the \"png:\" prefix.");
                return SIXEL_BAD_ARGUMENT;
            }
            libc_buffer_size = sixel_path_to_libc_buffer_size(payload);
            if (libc_buffer_size > 0u) {
                libc_buffer = (char *)malloc(libc_buffer_size);
                if (libc_buffer == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_decoder_setopt: malloc() failed for png path "
                        "buffer.");
                    return SIXEL_BAD_ALLOCATION;
                }
                libc_path = sixel_path_to_libc(payload,
                                               libc_buffer,
                                               libc_buffer_size);
                if (libc_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_decoder_setopt: invalid png output path.");
                    free(libc_buffer);
                    return SIXEL_BAD_ARGUMENT;
                }
                filename = libc_path;
            } else {
                filename = payload;
            }
        } else {
            filename = value;
        }

        if (filename != NULL) {
            clipboard_spec.is_clipboard = 0;
            clipboard_spec.format[0] = '\0';
            if (sixel_clipboard_parse_spec(filename, &clipboard_spec)
                    && clipboard_spec.is_clipboard) {
                decoder_clipboard_select_format(
                    decoder->clipboard_output_format,
                    sizeof(decoder->clipboard_output_format),
                    clipboard_spec.format,
                    "png");
                decoder->clipboard_output_active = 1;
            }
        }
        free(decoder->output);
        decoder->output = strdup_with_allocator(filename, decoder->allocator);
        if (libc_buffer != NULL) {
            free(libc_buffer);
            libc_buffer = NULL;
        }
        if (decoder->output == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DEQUANTIZE:  /* d */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: -d/--dequantize requires an argument.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        match_index = 0;
        memset(match_detail, 0, sizeof(match_detail));
        memset(match_message, 0, sizeof(match_message));

        match_result = sixel_option_match_choice(
            value,
            g_decoder_dequant_choices,
            sizeof(g_decoder_dequant_choices) /
            sizeof(g_decoder_dequant_choices[0]),
            &match_index,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            decoder->dequantize_method =
                g_decoder_dequant_methods[match_index];
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(
                    value,
                    match_detail,
                    match_message,
                    sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "unsupported dequantize method.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;

    case SIXEL_OPTFLAG_SIMILARITY:  /* S */
        errno = 0;
        bias = strtol(value, &endptr, 10);
        if (endptr == value || endptr[0] != '\0' ||
            errno == ERANGE || bias < 0 || bias > 1000) {
            sixel_helper_set_additional_message(
                "similarity must be an integer between 0 and 1000.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }

        decoder->dequantize_similarity_bias = (int)bias;
        break;

    case SIXEL_OPTFLAG_SIZE:  /* s */
        parsed_value = 0L;
        endptr = NULL;
        errno = 0;
        parsed_value = strtol(value, &endptr, 10);
        if (endptr == value || *endptr != '\0' || errno == ERANGE ||
            parsed_value < 1L || parsed_value > (long)INT_MAX) {
            sixel_helper_set_additional_message(
                "size must be greater than zero.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        decoder->thumbnail_size = (int)parsed_value;
        break;

    case SIXEL_OPTFLAG_EDGE:  /* e */
        parsed_value = 0L;
        endptr = NULL;
        errno = 0;
        parsed_value = strtol(value, &endptr, 10);
        if (endptr == value || *endptr != '\0' || errno == ERANGE ||
            parsed_value < 0L || parsed_value > 1000L) {
            sixel_helper_set_additional_message(
                "edge bias must be between 1 and 1000.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        decoder->dequantize_edge_strength = (int)parsed_value;
        break;

    case SIXEL_OPTFLAG_DIRECT:  /* D */
        decoder->direct_color = 1;
        break;

    case SIXEL_OPTFLAG_THREADS:  /* = */
        status = sixel_decoder_parallel_override_threads(value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;

    case '?':
    default:
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_decoder_unref(decoder);

    return status;
}


static SIXELSTATUS
sixel_decoder_promote_rgb888_to_rgba8888(unsigned char **out_pixels,
                                         unsigned char const *rgb_pixels,
                                         int width,
                                         int height,
                                         sixel_allocator_t *allocator)
{
    unsigned char *rgba_pixels = NULL;
    size_t num_pixels;
    size_t pixel_index;
    size_t rgb_index;
    size_t rgba_index;

    if (out_pixels == NULL || rgb_pixels == NULL ||
            width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    num_pixels = (size_t)width * (size_t)height;
    if (num_pixels > ((size_t)-1 / 4u)) {
        return SIXEL_BAD_ALLOCATION;
    }

    rgba_pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        num_pixels * 4u);
    if (rgba_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decoder_promote_rgb888_to_rgba8888: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    /*
     * k_undither is an RGB-domain neighbour filter.  Its current cache is
     * keyed by palette index, so keep that palette-aware pass and only add
     * opaque alpha here to preserve --direct's RGBA output contract.
     */
    for (pixel_index = 0u; pixel_index < num_pixels; ++pixel_index) {
        rgb_index = pixel_index * 3u;
        rgba_index = pixel_index * 4u;
        rgba_pixels[rgba_index + 0u] = rgb_pixels[rgb_index + 0u];
        rgba_pixels[rgba_index + 1u] = rgb_pixels[rgb_index + 1u];
        rgba_pixels[rgba_index + 2u] = rgb_pixels[rgb_index + 2u];
        rgba_pixels[rgba_index + 3u] = 0xffu;
    }

    *out_pixels = rgba_pixels;
    return SIXEL_OK;
}

/* load source data from stdin or the file specified with
   SIXEL_OPTFLAG_INPUT flag, and decode it */
SIXELAPI SIXELSTATUS
sixel_decoder_decode(
    sixel_decoder_t /* in */ *decoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *raw_data = NULL;
    int sx;
    int sy;
    int raw_len;
    int max;
    int n;
    FILE *input_fp = NULL;
    char message[2048];
    unsigned char *indexed_pixels = NULL;
    unsigned char *palette = NULL;
    unsigned char *rgb_pixels = NULL;
    unsigned char *direct_pixels = NULL;
    unsigned char *output_pixels;
    unsigned char *output_palette;
    int output_pixelformat;
    int ncolors;
    sixel_frame_t *frame;
    int new_width;
    int new_height;
    double scaled_width;
    double scaled_height;
    int max_dimension;
    int thumbnail_size;
    int frame_ncolors;
    unsigned char *clipboard_blob;
    size_t clipboard_blob_size;
    SIXELSTATUS clipboard_status;
    char *clipboard_output_path;
    unsigned char *clipboard_output_data;
    size_t clipboard_output_size;
    SIXELSTATUS clipboard_output_status;
    sixel_timeline_logger_t *logger;
    int logger_prepared;

    sixel_decoder_ref(decoder);

    frame = NULL;
    new_width = 0;
    new_height = 0;
    scaled_width = 0.0;
    scaled_height = 0.0;
    max_dimension = 0;
    thumbnail_size = decoder->thumbnail_size;
    frame_ncolors = -1;
    clipboard_blob = NULL;
    clipboard_blob_size = 0u;
    clipboard_status = SIXEL_OK;
    clipboard_output_path = NULL;
    clipboard_output_data = NULL;
    clipboard_output_size = 0u;
    clipboard_output_status = SIXEL_OK;
    input_fp = NULL;
    logger = NULL;
    (void)sixel_timeline_logger_prepare_env(decoder->allocator, &logger);
    logger_prepared = logger != NULL;

    raw_len = 0;
    max = 0;
    if (decoder->clipboard_input_active) {
        clipboard_status = sixel_clipboard_read(
            decoder->clipboard_input_format,
            &clipboard_blob,
            &clipboard_blob_size,
            decoder->allocator);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        max = (int)((clipboard_blob_size > 0u)
                    ? clipboard_blob_size
                    : 1u);
        raw_data = (unsigned char *)sixel_allocator_malloc(
            decoder->allocator,
            (size_t)max);
        if (raw_data == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_decode: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (clipboard_blob_size > 0u && clipboard_blob != NULL) {
            memcpy(raw_data, clipboard_blob, clipboard_blob_size);
        }
        raw_len = (int)clipboard_blob_size;
        if (clipboard_blob != NULL) {
            free(clipboard_blob);
            clipboard_blob = NULL;
        }
    } else {
        if (strcmp(decoder->input, "-") == 0) {
            /* for windows */
#if defined(O_BINARY)
            (void)sixel_compat_set_binary(STDIN_FILENO);
#endif  /* defined(O_BINARY) */
            input_fp = stdin;
        } else {
            input_fp = sixel_compat_fopen(decoder->input, "rb");
            if (! input_fp) {
                (void)snprintf(
                    message,
                    sizeof(message) - 1,
                    "sixel_decoder_decode: failed to open input file: %s.",
                    decoder->input);
                sixel_helper_set_additional_message(message);
                status = (SIXEL_LIBC_ERROR | (errno & 0xff));
                goto end;
            }
        }

        raw_len = 0;
        max = 64 * 1024;

        raw_data = (unsigned char *)sixel_allocator_malloc(
            decoder->allocator,
            (size_t)max);
        if (raw_data == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_decode: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        for (;;) {
            if ((max - raw_len) < 4096) {
                max *= 2;
                raw_data = (unsigned char *)sixel_allocator_realloc(
                    decoder->allocator,
                    raw_data,
                    (size_t)max);
                if (raw_data == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_decoder_decode: sixel_allocator_realloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
            }
            if ((n = (int)fread(raw_data + raw_len, 1, 4096, input_fp)) <= 0) {
                break;
            }
            raw_len += n;
        }

        if (input_fp != NULL && input_fp != stdin) {
            fclose(input_fp);
        }
    }

    ncolors = 0;

    if (decoder->direct_color != 0 &&
            decoder->dequantize_method == SIXEL_DEQUANTIZE_NONE) {
        status = sixel_decode_direct(
            raw_data,
            raw_len,
            &direct_pixels,
            &sx,
            &sy,
            decoder->allocator);
    } else {
        status = sixel_decode_raw(
            raw_data,
            raw_len,
            &indexed_pixels,
            &sx,
            &sy,
            &palette,
            &ncolors,
            decoder->allocator);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (sx > SIXEL_WIDTH_LIMIT || sy > SIXEL_HEIGHT_LIMIT) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (decoder->direct_color != 0 &&
            decoder->dequantize_method == SIXEL_DEQUANTIZE_NONE) {
        output_pixels = direct_pixels;
        output_palette = NULL;
        output_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame_ncolors = 0;
    } else {
        output_pixels = indexed_pixels;
        output_palette = palette;
        output_pixelformat = SIXEL_PIXELFORMAT_PAL8;

        if (decoder->dequantize_method == SIXEL_DEQUANTIZE_K_UNDITHER) {
            if (logger_prepared) {
                sixel_timeline_logger_logf(logger,
                                  "decoder",
                                  "undither",
                                  "start",
                                  0);
            }
            status = sixel_dequantize_k_undither(
                indexed_pixels,
                sx,
                sy,
                palette,
                ncolors,
                decoder->dequantize_similarity_bias,
                decoder->dequantize_edge_strength,
                decoder->allocator,
                &rgb_pixels);
            if (SIXEL_FAILED(status)) {
                if (logger_prepared) {
                    sixel_timeline_logger_logf(
                        logger,
                        "decoder",
                        "undither",
                        "abort",
                        0);
                }
                goto end;
            }
            if (decoder->direct_color != 0) {
                status = sixel_decoder_promote_rgb888_to_rgba8888(
                    &direct_pixels,
                    rgb_pixels,
                    sx,
                    sy,
                    decoder->allocator);
                if (SIXEL_FAILED(status)) {
                    if (logger_prepared) {
                        sixel_timeline_logger_logf(
                            logger,
                            "decoder",
                            "undither",
                            "abort",
                            0);
                    }
                    goto end;
                }
            }
            if (logger_prepared) {
                sixel_timeline_logger_logf(logger,
                                  "decoder",
                                  "undither",
                                  "finish",
                                  0);
            }
            if (decoder->direct_color != 0) {
                output_pixels = direct_pixels;
                output_palette = NULL;
                output_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            } else {
                output_pixels = rgb_pixels;
                output_palette = NULL;
                output_pixelformat = SIXEL_PIXELFORMAT_RGB888;
            }
        }

        if (output_pixelformat == SIXEL_PIXELFORMAT_PAL8) {
            frame_ncolors = ncolors;
        } else {
            frame_ncolors = 0;
        }
    }

    if (thumbnail_size > 0) {
        /*
         * When the caller requests a thumbnail, compute the new geometry
         * while preserving the original aspect ratio. We only allocate a
         * frame when the dimensions actually change, so the fast path for
         * matching sizes still avoids any additional allocations.
         */
        max_dimension = sx;
        if (sy > max_dimension) {
            max_dimension = sy;
        }
        if (max_dimension > 0) {
            if (sx >= sy) {
                new_width = thumbnail_size;
                scaled_height = (double)sy * (double)thumbnail_size /
                    (double)sx;
                new_height = (int)(scaled_height + 0.5);
            } else {
                new_height = thumbnail_size;
                scaled_width = (double)sx * (double)thumbnail_size /
                    (double)sy;
                new_width = (int)(scaled_width + 0.5);
            }
            if (new_width < 1) {
                new_width = 1;
            }
            if (new_height < 1) {
                new_height = 1;
            }
            if (new_width != sx || new_height != sy) {
                /*
                 * Wrap the decoded pixels in a frame so we can reuse the
                 * central scaling helper. Ownership transfers to the frame,
                 * which keeps the lifetime rules identical on both paths.
                 */
                status = sixel_frame_create_from_factory(
                    &frame,
                    decoder->allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                status = sixel_frame_init(
                    frame,
                    output_pixels,
                    sx,
                    sy,
                    output_pixelformat,
                    output_palette,
                    frame_ncolors);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                if (output_pixels == indexed_pixels) {
                    indexed_pixels = NULL;
                }
                if (output_pixels == rgb_pixels) {
                    rgb_pixels = NULL;
                }
                if (output_pixels == direct_pixels) {
                    direct_pixels = NULL;
                }
                if (output_palette == palette) {
                    palette = NULL;
                }
                status = sixel_frame_resize(
                    frame,
                    new_width,
                    new_height,
                    SIXEL_RES_BILINEAR);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                /*
                 * The resized frame already exposes a tightly packed RGB
                 * buffer, so write the updated dimensions and references
                 * back to the main encoder path.
                 */
                sx = sixel_frame_get_width(frame);
                sy = sixel_frame_get_height(frame);
                output_pixels = sixel_frame_get_pixels(frame);
                output_palette = NULL;
                output_pixelformat = sixel_frame_get_pixelformat(frame);
            }
        }
    }

    if (decoder->clipboard_output_active) {
        clipboard_output_status = decoder_clipboard_create_spool(
            decoder->allocator,
            "clipboard-out",
            &clipboard_output_path);
        if (SIXEL_FAILED(clipboard_output_status)) {
            status = clipboard_output_status;
            goto end;
        }
    }

    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "start",
                          0);
    }
    status = sixel_helper_write_image_file(
        output_pixels,
        sx,
        sy,
        output_palette,
        output_pixelformat,
        decoder->clipboard_output_active
            ? clipboard_output_path
            : decoder->output,
        SIXEL_FORMAT_PNG,
        decoder->allocator);
    if (SIXEL_FAILED(status)) {
        if (logger_prepared) {
            sixel_timeline_logger_logf(logger,
                              "io",
                              "png",
                              "abort",
                              0);
        }
        goto end;
    }
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "finish",
                          0);
    }

    if (decoder->clipboard_output_active) {
        clipboard_output_status = decoder_clipboard_read_file(
            clipboard_output_path,
            &clipboard_output_data,
            &clipboard_output_size);
        if (SIXEL_SUCCEEDED(clipboard_output_status)) {
            clipboard_output_status = sixel_clipboard_write(
                decoder->clipboard_output_format,
                clipboard_output_data,
                clipboard_output_size);
        }
        if (clipboard_output_data != NULL) {
            free(clipboard_output_data);
            clipboard_output_data = NULL;
        }
        if (SIXEL_FAILED(clipboard_output_status)) {
            status = clipboard_output_status;
            goto end;
        }
    }

end:
    sixel_frame_unref(frame);
    sixel_allocator_free(decoder->allocator, raw_data);
    sixel_allocator_free(decoder->allocator, indexed_pixels);
    sixel_allocator_free(decoder->allocator, palette);
    sixel_allocator_free(decoder->allocator, direct_pixels);
    sixel_allocator_free(decoder->allocator, rgb_pixels);
    if (clipboard_blob != NULL) {
        free(clipboard_blob);
    }
    if (clipboard_output_path != NULL) {
        (void)sixel_compat_unlink(clipboard_output_path);
        sixel_allocator_free(decoder->allocator, clipboard_output_path);
    }

    sixel_decoder_unref(decoder);
    if (logger_prepared) {
        sixel_timeline_logger_unref(logger);
    }

    return status;
}


/* Exercise legacy constructor and refcounting for the decoder. */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
