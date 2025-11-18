/*
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

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
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
#include "clipboard.h"
#include "compat_stub.h"
#include "options.h"

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

    tmpdir = sixel_compat_getenv("TMPDIR");
#if defined(_WIN32)
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TEMP");
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TMP");
    }
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
#if defined(HAVE_TMPNAM)

# if defined(__clang__)
#  if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdeprecated-declarations"
#  endif
# endif
        /* Fall back to tmpnam() when mktemp variants are unavailable. */
        tmpname_result = tmpnam(template_path);
# if defined(__clang__)
#  if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#   pragma clang diagnostic pop
#  endif
# endif

#endif  /* HAVE_TMPNAM */
        if (tmpname_result == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to reserve spool template.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        template_capacity = strlen(template_path) + 1u;
    }

    open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_EXCL)
    open_flags |= O_EXCL;
#endif
    open_mode = S_IRUSR | S_IWUSR;
    fd = sixel_compat_open(template_path, open_flags, open_mode);
    if (fd < 0) {
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


static float sixel_srgb_to_linear_lut[256];
static unsigned char sixel_linear_to_srgb_lut[256];
static int sixel_color_lut_initialized = 0;

static const float sixel_gaussian3x3_kernel[9] = {
    0.0625f, 0.1250f, 0.0625f,
    0.1250f, 0.2500f, 0.1250f,
    0.0625f, 0.1250f, 0.0625f
};

static const float sixel_weak_sharpen3x3_kernel[9] = {
    -0.0625f, -0.0625f, -0.0625f,
    -0.0625f,  1.5000f, -0.0625f,
    -0.0625f, -0.0625f, -0.0625f
};

static const float sixel_sobel_gx_kernel[9] = {
    -1.0f, 0.0f, 1.0f,
    -2.0f, 0.0f, 2.0f,
    -1.0f, 0.0f, 1.0f
};

static const float sixel_sobel_gy_kernel[9] = {
    -1.0f, -2.0f, -1.0f,
     0.0f,  0.0f,  0.0f,
     1.0f,  2.0f,  1.0f
};

static void
sixel_color_lut_init(void)
{
    int i;
    float srgb;
    float linear;

    if (sixel_color_lut_initialized) {
        return;
    }

    for (i = 0; i < 256; ++i) {
        srgb = (float)i / 255.0f;
        if (srgb <= 0.04045f) {
            linear = srgb / 12.92f;
        } else {
            linear = powf((srgb + 0.055f) / 1.055f, 2.4f);
        }
        sixel_srgb_to_linear_lut[i] = linear * 255.0f;
    }

    for (i = 0; i < 256; ++i) {
        linear = (float)i / 255.0f;
        if (linear <= 0.0031308f) {
            srgb = linear * 12.92f;
        } else {
            srgb = 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
        }
        srgb *= 255.0f;
        if (srgb < 0.0f) {
            srgb = 0.0f;
        } else if (srgb > 255.0f) {
            srgb = 255.0f;
        }
        sixel_linear_to_srgb_lut[i] = (unsigned char)(srgb + 0.5f);
    }

    sixel_color_lut_initialized = 1;
}

static void
sixel_convolve3x3(const float *kernel,
                  float *dst,
                  const float *src,
                  int width,
                  int height)
{
    int x;
    int y;
    int kx;
    int ky;
    int sx;
    int sy;
    int idx;
    int src_index;
    int kernel_index;
    float sum;
    float weight;

    if (kernel == NULL || dst == NULL || src == NULL) {
        return;
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            sum = 0.0f;
            for (ky = -1; ky <= 1; ++ky) {
                sy = y + ky;
                if (sy < 0) {
                    sy = 0;
                } else if (sy >= height) {
                    sy = height - 1;
                }
                for (kx = -1; kx <= 1; ++kx) {
                    sx = x + kx;
                    if (sx < 0) {
                        sx = 0;
                    } else if (sx >= width) {
                        sx = width - 1;
                    }
                    kernel_index = (ky + 1) * 3 + (kx + 1);
                    weight = kernel[kernel_index];
                    src_index = sy * width + sx;
                    sum += src[src_index] * weight;
                }
            }
            idx = y * width + x;
            dst[idx] = sum;
        }
    }
}

static void
sixel_apply_relu(float *buffer, size_t count)
{
    size_t i;

    if (buffer == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        if (buffer[i] < 0.0f) {
            buffer[i] = 0.0f;
        }
    }
}

static unsigned char
sixel_linear_to_srgb_value(float value)
{
    int index;

    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 255.0f) {
        value = 255.0f;
    }

    index = (int)(value + 0.5f);
    if (index < 0) {
        index = 0;
    } else if (index > 255) {
        index = 255;
    }

    return sixel_linear_to_srgb_lut[index];
}

static void
sixel_post_undither_refine(unsigned char *rgb,
                           int width,
                           int height,
                           const float *mask)
{
    size_t num_pixels;
    float *Y;
    float *Cb;
    float *Cr;
    float *work0;
    float *work1;
    float *gate;
    float *gradient;
    int x;
    int y;
    int kx;
    int ky;
    int sx;
    int sy;
    int idx;
    int src_index;
    int kernel_index;
    size_t i;
    size_t base;
    float sigma_r;
    float beta;
    float alpha1;
    float alpha2;
    float smooth_gate_scale;
    float inv_sigma_r2;
    float center;
    float neighbor;
    float sum;
    float weight;
    float weight_sum;
    float diff;
    float range_weight;
    float gate_value;
    float gaussian_weight;
    float max_grad;
    float scale;
    float gx;
    float gy;
    float value;
    float y_value;
    float cb_value;
    float cr_value;
    float r_lin;
    float g_lin;
    float b_lin;
    float magnitude;

    if (rgb == NULL) {
        return;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    sixel_color_lut_init();

    num_pixels = (size_t)width * (size_t)height;
    if (num_pixels == 0) {
        return;
    }

    Y = NULL;
    Cb = NULL;
    Cr = NULL;
    work0 = NULL;
    work1 = NULL;
    gate = NULL;
    gradient = NULL;

    sigma_r = 10.0f;
    beta = 0.25f;
    alpha1 = 0.60f;
    alpha2 = 0.40f;
    inv_sigma_r2 = 1.0f / (2.0f * sigma_r * sigma_r);
    smooth_gate_scale = 0.96f;

    Y = (float *)malloc(num_pixels * sizeof(float));
    Cb = (float *)malloc(num_pixels * sizeof(float));
    Cr = (float *)malloc(num_pixels * sizeof(float));
    work0 = (float *)malloc(num_pixels * sizeof(float));
    work1 = (float *)malloc(num_pixels * sizeof(float));
    gate = (float *)malloc(num_pixels * sizeof(float));

    if (Y == NULL || Cb == NULL || Cr == NULL ||
        work0 == NULL || work1 == NULL || gate == NULL) {
        goto cleanup;
    }

    for (i = 0; i < num_pixels; ++i) {
        base = i * 3;
        r_lin = sixel_srgb_to_linear_lut[rgb[base + 0]];
        g_lin = sixel_srgb_to_linear_lut[rgb[base + 1]];
        b_lin = sixel_srgb_to_linear_lut[rgb[base + 2]];

        y_value = 0.2990f * r_lin + 0.5870f * g_lin + 0.1140f * b_lin;
        cb_value = (b_lin - y_value) * 0.564383f;
        cr_value = (r_lin - y_value) * 0.713272f;

        Y[i] = y_value;
        Cb[i] = cb_value;
        Cr[i] = cr_value;
    }

    if (mask != NULL) {
        for (i = 0; i < num_pixels; ++i) {
            value = mask[i];
            if (value < 0.0f) {
                value = 0.0f;
            } else if (value > 1.0f) {
                value = 1.0f;
            }
            gate[i] = 1.0f - value;
            if (gate[i] < 0.0f) {
                gate[i] = 0.0f;
            }
        }
    } else {
        gradient = (float *)malloc(num_pixels * sizeof(float));
        if (gradient == NULL) {
            goto cleanup;
        }

        max_grad = 0.0f;
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                gx = 0.0f;
                gy = 0.0f;
                for (ky = -1; ky <= 1; ++ky) {
                    sy = y + ky;
                    if (sy < 0) {
                        sy = 0;
                    } else if (sy >= height) {
                        sy = height - 1;
                    }
                    for (kx = -1; kx <= 1; ++kx) {
                        sx = x + kx;
                        if (sx < 0) {
                            sx = 0;
                        } else if (sx >= width) {
                            sx = width - 1;
                        }
                        kernel_index = (ky + 1) * 3 + (kx + 1);
                        src_index = sy * width + sx;
                        neighbor = Y[src_index];
                        gx += neighbor *
                              sixel_sobel_gx_kernel[kernel_index];
                        gy += neighbor *
                              sixel_sobel_gy_kernel[kernel_index];
                    }
                }
                idx = y * width + x;
                magnitude = sqrtf(gx * gx + gy * gy);
                gradient[idx] = magnitude;
                if (magnitude > max_grad) {
                    max_grad = magnitude;
                }
            }
        }

        if (max_grad <= 0.0f) {
            max_grad = 1.0f;
        }
        scale = 1.0f / max_grad;

        for (i = 0; i < num_pixels; ++i) {
            value = gradient[i] * scale;
            if (value < 0.0f) {
                value = 0.0f;
            } else if (value > 1.0f) {
                value = 1.0f;
            }
            gate[i] = 1.0f - value;
            if (gate[i] < 0.0f) {
                gate[i] = 0.0f;
            }
        }
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = y * width + x;
            center = Y[idx];
            gate_value = gate[idx];
            sum = 0.0f;
            weight_sum = 0.0f;
            for (ky = -1; ky <= 1; ++ky) {
                sy = y + ky;
                if (sy < 0) {
                    sy = 0;
                } else if (sy >= height) {
                    sy = height - 1;
                }
                for (kx = -1; kx <= 1; ++kx) {
                    sx = x + kx;
                    if (sx < 0) {
                        sx = 0;
                    } else if (sx >= width) {
                        sx = width - 1;
                    }
                    kernel_index = (ky + 1) * 3 + (kx + 1);
                    gaussian_weight =
                        sixel_gaussian3x3_kernel[kernel_index];
                    src_index = sy * width + sx;
                    neighbor = Y[src_index];
                    if (kx == 0 && ky == 0) {
                        weight = gaussian_weight;
                    } else {
                        diff = neighbor - center;
                        range_weight = expf(-(diff * diff) * inv_sigma_r2);
                        weight = gaussian_weight * gate_value * range_weight;
                    }
                    sum += neighbor * weight;
                    weight_sum += weight;
                }
            }
            if (weight_sum <= 0.0f) {
                work0[idx] = center;
            } else {
                work0[idx] = sum / weight_sum;
            }
        }
    }

    for (i = 0; i < num_pixels; ++i) {
        center = Y[i];
        value = work0[i];
        Y[i] = (1.0f - beta) * center + beta * value;
    }

    sixel_convolve3x3(sixel_gaussian3x3_kernel,
                      work0,
                      Y,
                      width,
                      height);
    for (i = 0; i < num_pixels; ++i) {
        gate_value = gate[i] * smooth_gate_scale;
        center = Y[i];
        value = work0[i];
        work0[i] = gate_value * value
                 + (1.0f - gate_value) * center;
    }
    sixel_apply_relu(work0, num_pixels);
    sixel_convolve3x3(sixel_weak_sharpen3x3_kernel,
                      work1,
                      work0,
                      width,
                      height);

    for (i = 0; i < num_pixels; ++i) {
        center = Y[i];
        value = work1[i];
        Y[i] = center + alpha1 * (value - center);
    }

    sixel_convolve3x3(sixel_gaussian3x3_kernel,
                      work0,
                      Y,
                      width,
                      height);
    for (i = 0; i < num_pixels; ++i) {
        gate_value = gate[i] * smooth_gate_scale;
        center = Y[i];
        value = work0[i];
        work0[i] = gate_value * value
                 + (1.0f - gate_value) * center;
    }
    sixel_apply_relu(work0, num_pixels);
    sixel_convolve3x3(sixel_weak_sharpen3x3_kernel,
                      work1,
                      work0,
                      width,
                      height);

    for (i = 0; i < num_pixels; ++i) {
        center = Y[i];
        value = work1[i];
        Y[i] = center + alpha2 * (value - center);
    }

    for (i = 0; i < num_pixels; ++i) {
        base = i * 3;
        y_value = Y[i];
        cb_value = Cb[i];
        cr_value = Cr[i];

        r_lin = y_value + 1.402000f * cr_value;
        b_lin = y_value + 1.772000f * cb_value;
        g_lin = y_value - 0.344136f * cb_value - 0.714136f * cr_value;

        rgb[base + 0] = sixel_linear_to_srgb_value(r_lin);
        rgb[base + 1] = sixel_linear_to_srgb_value(g_lin);
        rgb[base + 2] = sixel_linear_to_srgb_value(b_lin);
    }

cleanup:
    free(Y);
    free(Cb);
    free(Cr);
    free(work0);
    free(work1);
    free(gate);
    free(gradient);
}


/* original version of strdup(3) with allocator object */
static char *
strdup_with_allocator(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;

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

    (*ppdecoder)->ref          = 1;
    (*ppdecoder)->output       = strdup_with_allocator("-", allocator);
    (*ppdecoder)->input        = strdup_with_allocator("-", allocator);
    (*ppdecoder)->allocator    = allocator;
    (*ppdecoder)->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    (*ppdecoder)->dequantize_similarity_bias = 100;
    (*ppdecoder)->dequantize_edge_strength = 0;
    (*ppdecoder)->dequantize_refine = 1;
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


/* increase reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    ++decoder->ref;
}


/* decrease reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    if (decoder != NULL && --decoder->ref == 0) {
        sixel_decoder_destroy(decoder);
    }
}


typedef struct sixel_similarity {
    const unsigned char *palette;
    int ncolors;
    int stride;
    signed char *cache;
    int bias;
} sixel_similarity_t;

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
    unsigned int min_diff = UINT_MAX;
    int i;
    unsigned int result;
    const unsigned char *pk;
    unsigned int diff;

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

    for (i = 0; i < similarity->ncolors; ++i) {
        if (i == index1 || i == index2) {
            continue;
        }
        pk = palette + i * 3;
        diff = sixel_similarity_diff(avg_color, pk);
        if (diff < min_diff) {
            min_diff = diff;
        }
    }

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
    unsigned long long magnitude = (unsigned long long)gx
                                 * (unsigned long long)gx
                                 + (unsigned long long)gy
                                 * (unsigned long long)gy;
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

static SIXELSTATUS
sixel_dequantize_k_undither(unsigned char *indexed_pixels,
                            int width,
                            int height,
                            unsigned char *palette,
                            int ncolors,
                            int similarity_bias,
                            int edge_strength,
                            int enable_refine,
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
                /*
                 * Edge detection keeps high-frequency content intact while we
                 * smooth dithering noise in flatter regions.
                 */
                prewitt[y * width + x] = sixel_prewitt_value(
                    gray,
                    width,
                    height,
                    x,
                    y);

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

            out_index = (size_t)(y * width + x) * 3;
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
                    weight = sixel_similarity_compare(
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
    if (enable_refine) {
        /*
         *  Only run when the caller requested k_undither+.
         */
        sixel_post_undither_refine(*output, width, height, NULL);
    }
    status = SIXEL_OK;

end:
    sixel_similarity_destroy(&similarity, allocator);
    sixel_allocator_free(allocator, rgb);
    sixel_allocator_free(allocator, gray);
    sixel_allocator_free(allocator, prewitt);
    return status;
}
/*
 * The dequantizer accepts both a method and an optional refine flag.
 * The shared option helper returns an integer payload, so the decoder
 * keeps a parallel lookup table that translates the matched index into
 * the tuple expected by the execution path.
 */
typedef struct sixel_decoder_dequant_mapping {
    int method;
    int refine;
} sixel_decoder_dequant_mapping_t;

static sixel_decoder_dequant_mapping_t const
g_decoder_dequant_mappings[] = {
    { SIXEL_DEQUANTIZE_NONE, 0 },
    { SIXEL_DEQUANTIZE_K_UNDITHER, 0 },
    { SIXEL_DEQUANTIZE_K_UNDITHER_PLUS, 1 }
};

static sixel_option_choice_t const g_decoder_dequant_choices[] = {
    { "none", 0 },
    { "k_undither", 1 },
    { "k_undither+", 2 }
};

static void
normalise_windows_drive_path(char *path)
{
#if defined(_WIN32)
    size_t length;

    length = 0u;

    if (path == NULL) {
        return;
    }

    length = strlen(path);
    if (length >= 3u
            && path[0] == '/'
            && ((path[1] >= 'A' && path[1] <= 'Z')
                || (path[1] >= 'a' && path[1] <= 'z'))
            && path[2] == '/') {
        path[0] = path[1];
        path[1] = ':';
    }
#else
    (void)path;
#endif
}

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
    size_t length;
    sixel_clipboard_spec_t clipboard_spec;
    int match_index;
    sixel_option_choice_result_t match_result;
    char match_detail[128];
    char match_message[256];
    char const *filename = NULL;
    char *p = NULL;

    sixel_decoder_ref(decoder);
    path_flags = 0u;
    path_check = 0;

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
            length = strlen(payload);
            filename = p = malloc(length + 1U);
            if (p == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_decoder_setopt: malloc() failed for png path filename.");
                return SIXEL_BAD_ALLOCATION;
            }
            memcpy(p, payload, length + 1U);
            normalise_windows_drive_path(p);
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
        free(p);
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
                g_decoder_dequant_mappings[match_index].method;
            decoder->dequantize_refine =
                g_decoder_dequant_mappings[match_index].refine;
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
        decoder->dequantize_similarity_bias = atoi(value);
        if (decoder->dequantize_similarity_bias < 0 ||
            decoder->dequantize_similarity_bias > 1000) {
            sixel_helper_set_additional_message(
                "similarity must be between 1 and 1000.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;

    case SIXEL_OPTFLAG_SIZE:  /* s */
        decoder->thumbnail_size = atoi(value);
        if (decoder->thumbnail_size <= 0) {
            sixel_helper_set_additional_message(
                "size must be greater than zero.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;

    case SIXEL_OPTFLAG_EDGE:  /* e */
        decoder->dequantize_edge_strength = atoi(value);
        if (decoder->dequantize_edge_strength < 0 ||
            decoder->dequantize_edge_strength > 1000) {
            sixel_helper_set_additional_message(
                "edge bias must be between 1 and 1000.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;

    case SIXEL_OPTFLAG_DIRECT:  /* D */
        decoder->direct_color = 1;
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
# if HAVE__SETMODE
            _setmode(STDIN_FILENO, O_BINARY);
# elif HAVE_SETMODE
            setmode(STDIN_FILENO, O_BINARY);
# endif  /* HAVE_SETMODE */
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

    if (decoder->direct_color != 0 &&
            decoder->dequantize_method != SIXEL_DEQUANTIZE_NONE) {
        sixel_helper_set_additional_message(
            "sixel_decoder_decode: direct option "
            "cannot be combined with dequantize option.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    ncolors = 0;

    if (decoder->direct_color != 0) {
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

    if (decoder->direct_color != 0) {
        output_pixels = direct_pixels;
        output_palette = NULL;
        output_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame_ncolors = 0;
    } else {
        output_pixels = indexed_pixels;
        output_palette = palette;
        output_pixelformat = SIXEL_PIXELFORMAT_PAL8;

        if (decoder->dequantize_method == SIXEL_DEQUANTIZE_K_UNDITHER ||
            decoder->dequantize_method == SIXEL_DEQUANTIZE_K_UNDITHER_PLUS) {
            status = sixel_dequantize_k_undither(
                indexed_pixels,
                sx,
                sy,
                palette,
                ncolors,
                decoder->dequantize_similarity_bias,
                decoder->dequantize_edge_strength,
                decoder->dequantize_refine,
                decoder->allocator,
                &rgb_pixels);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            output_pixels = rgb_pixels;
            output_palette = NULL;
            output_pixelformat = SIXEL_PIXELFORMAT_RGB888;
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
                status = sixel_frame_new(&frame, decoder->allocator);
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
        goto end;
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

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    decoder = sixel_decoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (decoder == NULL) {
        goto error;
    }
    sixel_decoder_ref(decoder);
    sixel_decoder_unref(decoder);
    nret = EXIT_SUCCESS;

error:
    sixel_decoder_unref(decoder);
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    SIXELSTATUS status;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_decoder_ref(decoder);
    sixel_decoder_unref(decoder);
    nret = EXIT_SUCCESS;

error:
    sixel_decoder_unref(decoder);
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 1;

    status = sixel_allocator_new(
        &allocator,
        sixel_bad_malloc,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test4(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 2;

    status = sixel_allocator_new(
        &allocator,
        sixel_bad_malloc,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test5(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 4;

    status = sixel_allocator_new(
        &allocator,
        sixel_bad_malloc,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(
        decoder,
        SIXEL_OPTFLAG_INPUT,
        "/");
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test6(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 4;

    status = sixel_allocator_new(
        &allocator,
        sixel_bad_malloc,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(
        decoder,
        SIXEL_OPTFLAG_OUTPUT,
        "/");
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test7(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    status = sixel_allocator_new(
        &allocator,
        NULL,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(
        decoder,
        SIXEL_OPTFLAG_INPUT,
        "../images/file");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_decode(decoder);
    if ((status >> 8) != (SIXEL_LIBC_ERROR >> 8)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test8(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 5;

    status = sixel_allocator_new(
        &allocator,
        sixel_bad_malloc,
        NULL,
        NULL,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(
        decoder,
        SIXEL_OPTFLAG_INPUT,
        "../images/map8.six");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_decode(decoder);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


SIXELAPI int
sixel_decoder_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
        test5,
        test6,
        test7,
        test8
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
