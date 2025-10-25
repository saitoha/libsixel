/*
 * evaluate.c - High-speed image quality evaluator ported from Python.
 *
 *  +-------------------------------------------------------------+
 *  |                        PIPELINE MAP                         |
 *  +---------------------+------------------+--------------------+
 *  | image loading (RGB) | metric kernels   | JSON emit          |
 *  |      (stb_image)    |  (MS-SSIM, etc.) | (stdout + files)   |
 *  +---------------------+------------------+--------------------+
 *
 *  ASCII flow for the metrics modules:
 *
 *        +-----------+      +---------------+      +-------------+
 *        |   Luma    | ---> |  Spectral +   | ---> |  Composite  |
 *        |  Stack    |      |  Spatial      |      |   Report    |
 *        +-----------+      +---------------+      +-------------+
 *              |                    |                      |
 *              |                    |                      +--> JSON writer
 *              |                    +--> FFT / histogram engines
 *              +--> MS-SSIM / PSNR / GMSD
 *
 *  Every function carries intentionally verbose comments so that future
 *  maintainers can follow the numerical steps without cross-referencing the
 *  removed Python original.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../src/stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ARRAY_GUARD(ptr)                                   \
    do {                                                   \
        if ((ptr) == NULL) {                              \
            fprintf(stderr, "Out of memory\n");           \
            exit(EXIT_FAILURE);                           \
        }                                                  \
    } while (0)

/* ================================================================
 * Utility data structures
 * ================================================================ */

typedef struct Image {
    int width;
    int height;
    int channels;
    float *pixels; /* interleaved RGB, values in [0,1] */
} Image;

typedef struct FloatBuffer {
    size_t length;
    float *values;
} FloatBuffer;

typedef struct Complex {
    double re;
    double im;
} Complex;

typedef struct Metrics {
    float ms_ssim;
    float high_freq_out;
    float high_freq_ref;
    float high_freq_delta;
    float stripe_ref;
    float stripe_out;
    float stripe_rel;
    float band_run_rel;
    float band_grad_rel;
    float clip_l_ref;
    float clip_r_ref;
    float clip_g_ref;
    float clip_b_ref;
    float clip_l_out;
    float clip_r_out;
    float clip_g_out;
    float clip_b_out;
    float clip_l_rel;
    float clip_r_rel;
    float clip_g_rel;
    float clip_b_rel;
    float delta_chroma_mean;
    float delta_e00_mean;
    float gmsd_value;
    float psnr_y;
    float lpips_vgg;
} Metrics;

static const char *lpips_script_source =
    "import sys\n"
    "def _emit_nan():\n"
    "    print('nan')\n"
    "    return 0\n"
    "def main():\n"
    "    if len(sys.argv) != 3:\n"
    "        return _emit_nan()\n"
    "    try:\n"
    "        import numpy as np\n"
    "        import torch\n"
    "        import lpips\n"
    "        from PIL import Image\n"
    "    except Exception:\n"
    "        return _emit_nan()\n"
    "    ref = Image.open(sys.argv[1]).convert('RGB')\n"
    "    out = Image.open(sys.argv[2]).convert('RGB')\n"
    "    ref_arr = np.asarray(ref, dtype=np.float32) / 255.0\n"
    "    out_arr = np.asarray(out, dtype=np.float32) / 255.0\n"
    "    ref_t = torch.from_numpy(ref_arr.transpose(2, 0, 1)).unsqueeze(0)\n"
    "    out_t = torch.from_numpy(out_arr.transpose(2, 0, 1)).unsqueeze(0)\n"
    "    ref_t = ref_t * 2.0 - 1.0\n"
    "    out_t = out_t * 2.0 - 1.0\n"
    "    with torch.no_grad():\n"
    "        net = lpips.LPIPS(net='vgg')\n"
    "        dist = net(ref_t, out_t)\n"
    "    print(float(dist.item()))\n"
    "    return 0\n"
    "if __name__ == '__main__':\n"
    "    sys.exit(main())\n";

/* ================================================================
 * Memory helpers
 * ================================================================ */

static void *xmalloc(size_t size)
{
    void *ptr;
    ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "malloc failed (%zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void *xcalloc(size_t nmemb, size_t size)
{
    void *ptr;
    ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        fprintf(stderr, "calloc failed (%zu bytes)\n", nmemb * size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void image_free(Image *img)
{
    if (img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
    }
}

/* ================================================================
 * File loading helpers
 * ================================================================ */

static unsigned char *read_file_to_memory(const char *path, size_t *size)
{
    FILE *fp;
    unsigned char *buffer;
    unsigned char *tmp;
    size_t capacity;
    size_t total;
    size_t chunk;
    size_t nread;
    size_t new_capacity;

    fp = NULL;
    buffer = NULL;
    tmp = NULL;
    capacity = 0;
    total = 0;
    chunk = 64 * 1024;
    nread = 0;
    new_capacity = 0;

    if (strcmp(path, "-") == 0 || strcmp(path, "/dev/stdin") == 0) {
        fp = stdin;
    } else {
        fp = fopen(path, "rb");
        if (fp == NULL) {
            fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
            return NULL;
        }
    }

    while (1) {
        if (total + chunk > capacity) {
            new_capacity = capacity == 0 ? chunk : capacity * 2;
            tmp = realloc(buffer, new_capacity);
            if (tmp == NULL) {
                fprintf(stderr, "realloc failed (%zu bytes)\n", new_capacity);
                free(buffer);
                if (fp != stdin && fp != NULL) {
                    fclose(fp);
                }
                return NULL;
            }
            buffer = tmp;
            capacity = new_capacity;
        }
        nread = fread(buffer + total, 1, chunk, fp);
        total += nread;
        if (nread < chunk) {
            if (feof(fp)) {
                break;
            }
            if (ferror(fp)) {
                fprintf(stderr, "Error while reading %s\n", path);
                free(buffer);
                if (fp != stdin && fp != NULL) {
                    fclose(fp);
                }
                return NULL;
            }
        }
    }

    if (fp != stdin && fp != NULL) {
        fclose(fp);
    }

    *size = total;
    return buffer;
}

static int load_image(const char *path, Image *img)
{
    unsigned char *file_data;
    size_t file_size;
    int w;
    int h;
    int comp;
    int target_comp;
    unsigned char *decoded;
    float *pixels;
    size_t count;
    size_t i;

    file_data = NULL;
    file_size = 0;
    w = 0;
    h = 0;
    comp = 0;
    target_comp = 3;
    decoded = NULL;
    pixels = NULL;
    count = 0;
    i = 0;

    if (strcmp(path, "-") == 0 || strcmp(path, "/dev/stdin") == 0) {
        file_data = read_file_to_memory(path, &file_size);
        if (file_data == NULL) {
            return -1;
        }
        decoded = stbi_load_from_memory(
            file_data, (int)file_size, &w, &h, &comp, target_comp);
        free(file_data);
    } else {
        decoded = stbi_load(path, &w, &h, &comp, target_comp);
    }

    if (decoded == NULL) {
        fprintf(stderr, "stb_image failed: %s\n", stbi_failure_reason());
        return -1;
    }

    count = (size_t)w * (size_t)h * (size_t)target_comp;
    pixels = (float *)xmalloc(count * sizeof(float));
    for (i = 0; i < count; ++i) {
        pixels[i] = decoded[i] / 255.0f;
    }

    stbi_image_free(decoded);

    img->width = w;
    img->height = h;
    img->channels = target_comp;
    img->pixels = pixels;
    return 0;
}

/* ================================================================
 * Array math helpers
 * ================================================================ */

static FloatBuffer float_buffer_create(size_t length)
{
    FloatBuffer buf;
    buf.length = length;
    buf.values = (float *)xcalloc(length, sizeof(float));
    return buf;
}

static void float_buffer_free(FloatBuffer *buf)
{
    if (buf->values != NULL) {
        free(buf->values);
        buf->values = NULL;
        buf->length = 0;
    }
}

static float clamp_float(float v, float min_v, float max_v)
{
    float result;
    result = v;
    if (result < min_v) {
        result = min_v;
    }
    if (result > max_v) {
        result = max_v;
    }
    return result;
}

static unsigned char float_to_byte(float v)
{
    float scaled;
    int ivalue;
    unsigned char byte;

    scaled = clamp_float(v, 0.0f, 1.0f) * 255.0f + 0.5f;
    ivalue = (int)scaled;
    if (ivalue < 0) {
        ivalue = 0;
    }
    if (ivalue > 255) {
        ivalue = 255;
    }
    byte = (unsigned char)ivalue;
    return byte;
}

/* ================================================================
 * Luma conversion and resizing utilities
 * ================================================================ */

static FloatBuffer image_to_luma709(const Image *img)
{
    FloatBuffer buf;
    size_t total;
    size_t i;
    const float *src;
    float r;
    float g;
    float b;

    total = (size_t)img->width * (size_t)img->height;
    buf = float_buffer_create(total);
    src = img->pixels;
    for (i = 0; i < total; ++i) {
        r = src[i * 3 + 0];
        g = src[i * 3 + 1];
        b = src[i * 3 + 2];
        buf.values[i] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
    return buf;
}

static FloatBuffer image_channel(const Image *img, int channel)
{
    FloatBuffer buf;
    size_t total;
    size_t i;
    const float *src;
    float value;

    total = (size_t)img->width * (size_t)img->height;
    buf = float_buffer_create(total);
    src = img->pixels;
    for (i = 0; i < total; ++i) {
        value = src[i * 3 + channel];
        buf.values[i] = value;
    }
    return buf;
}
/* ================================================================
 * Gaussian kernel and separable convolution
 * ================================================================ */

static FloatBuffer gaussian_kernel1d(int size, float sigma)
{
    FloatBuffer kernel;
    int i;
    float mean;
    float sum;
    float x;
    float value;

    kernel = float_buffer_create((size_t)size);
    mean = ((float)size - 1.0f) * 0.5f;
    sum = 0.0f;
    for (i = 0; i < size; ++i) {
        x = (float)i - mean;
        value = expf(-0.5f * (x / sigma) * (x / sigma));
        kernel.values[i] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (i = 0; i < size; ++i) {
            kernel.values[i] /= sum;
        }
    }
    return kernel;
}

static FloatBuffer separable_conv2d(const FloatBuffer *img, int width,
                                    int height, const FloatBuffer *kernel)
{
    FloatBuffer tmp;
    FloatBuffer out;
    int pad;
    int x;
    int y;
    int k;
    int kernel_size;
    float acc;
    int px;
    int py;
    int idx;
    float sample;

    pad = (int)kernel->length / 2;
    kernel_size = (int)kernel->length;
    tmp = float_buffer_create((size_t)width * (size_t)height);
    out = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                px = x + k - pad;
                if (px < 0) {
                    px = -px;
                }
                if (px >= width) {
                    px = width - (px - width) - 1;
                    if (px < 0) {
                        px = 0;
                    }
                }
                idx = y * width + px;
                sample = img->values[idx];
                acc += kernel->values[k] * sample;
            }
            tmp.values[y * width + x] = acc;
        }
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                py = y + k - pad;
                if (py < 0) {
                    py = -py;
                }
                if (py >= height) {
                    py = height - (py - height) - 1;
                    if (py < 0) {
                        py = 0;
                    }
                }
                idx = py * width + x;
                sample = tmp.values[idx];
                acc += kernel->values[k] * sample;
            }
            out.values[y * width + x] = acc;
        }
    }

    float_buffer_free(&tmp);
    return out;
}

/* ================================================================
 * SSIM and MS-SSIM computation
 * ================================================================ */

static float ssim_luma(const FloatBuffer *x, const FloatBuffer *y,
                       int width, int height, float k1, float k2,
                       int win_size, float sigma)
{
    FloatBuffer kernel;
    FloatBuffer mu_x;
    FloatBuffer mu_y;
    FloatBuffer mu_x2;
    FloatBuffer mu_y2;
    FloatBuffer mu_xy;
    FloatBuffer sigma_x2;
    FloatBuffer sigma_y2;
    FloatBuffer sigma_xy;
    float C1;
    float C2;
    size_t total;
    size_t i;
    float mean;
    float numerator;
    float denominator;
    float value;
    FloatBuffer x_sq;
    FloatBuffer y_sq;
    FloatBuffer xy_buf;

    kernel = gaussian_kernel1d(win_size, sigma);
    mu_x = separable_conv2d(x, width, height, &kernel);
    mu_y = separable_conv2d(y, width, height, &kernel);

    total = (size_t)width * (size_t)height;
    mu_x2 = float_buffer_create(total);
    mu_y2 = float_buffer_create(total);
    mu_xy = float_buffer_create(total);
    sigma_x2 = float_buffer_create(total);
    sigma_y2 = float_buffer_create(total);
    sigma_xy = float_buffer_create(total);
    x_sq = float_buffer_create(total);
    y_sq = float_buffer_create(total);
    xy_buf = float_buffer_create(total);

    for (i = 0; i < total; ++i) {
        mu_x2.values[i] = mu_x.values[i] * mu_x.values[i];
        mu_y2.values[i] = mu_y.values[i] * mu_y.values[i];
        mu_xy.values[i] = mu_x.values[i] * mu_y.values[i];
        x_sq.values[i] = x->values[i] * x->values[i];
        y_sq.values[i] = y->values[i] * y->values[i];
        xy_buf.values[i] = x->values[i] * y->values[i];
    }

    float_buffer_free(&sigma_x2);
    float_buffer_free(&sigma_y2);
    float_buffer_free(&sigma_xy);
    sigma_x2 = separable_conv2d(&x_sq, width, height, &kernel);
    sigma_y2 = separable_conv2d(&y_sq, width, height, &kernel);
    sigma_xy = separable_conv2d(&xy_buf, width, height, &kernel);

    for (i = 0; i < total; ++i) {
        sigma_x2.values[i] -= mu_x2.values[i];
        sigma_y2.values[i] -= mu_y2.values[i];
        sigma_xy.values[i] -= mu_xy.values[i];
    }

    C1 = (k1 * 1.0f) * (k1 * 1.0f);
    C2 = (k2 * 1.0f) * (k2 * 1.0f);

    mean = 0.0f;
    for (i = 0; i < total; ++i) {
        numerator = (2.0f * mu_xy.values[i] + C1) *
                    (2.0f * sigma_xy.values[i] + C2);
        denominator = (mu_x2.values[i] + mu_y2.values[i] + C1) *
                      (sigma_x2.values[i] + sigma_y2.values[i] + C2);
        if (denominator != 0.0f) {
            value = numerator / (denominator + 1e-12f);
        } else {
            value = 0.0f;
        }
        mean += value;
    }
    mean /= (float)total;
    mean = clamp_float(mean, 0.0f, 1.0f);

    float_buffer_free(&kernel);
    float_buffer_free(&mu_x);
    float_buffer_free(&mu_y);
    float_buffer_free(&mu_x2);
    float_buffer_free(&mu_y2);
    float_buffer_free(&mu_xy);
    float_buffer_free(&sigma_x2);
    float_buffer_free(&sigma_y2);
    float_buffer_free(&sigma_xy);
    float_buffer_free(&x_sq);
    float_buffer_free(&y_sq);
    float_buffer_free(&xy_buf);

    return mean;
}

static FloatBuffer downsample2(const FloatBuffer *img, int width, int height,
                               int *new_width, int *new_height)
{
    int h2;
    int w2;
    int y;
    int x;
    FloatBuffer out;
    float sum;
    int idx0;
    int idx1;
    int idx2;
    int idx3;

    h2 = height / 2;
    w2 = width / 2;
    out = float_buffer_create((size_t)h2 * (size_t)w2);
    for (y = 0; y < h2; ++y) {
        for (x = 0; x < w2; ++x) {
            sum = 0.0f;
            idx0 = (2 * y) * width + (2 * x);
            idx1 = (2 * y + 1) * width + (2 * x);
            idx2 = (2 * y) * width + (2 * x + 1);
            idx3 = (2 * y + 1) * width + (2 * x + 1);
            sum += img->values[idx0];
            sum += img->values[idx1];
            sum += img->values[idx2];
            sum += img->values[idx3];
            out.values[y * w2 + x] = sum * 0.25f;
        }
    }
    *new_width = w2;
    *new_height = h2;
    return out;
}

static float ms_ssim_luma(const FloatBuffer *ref, const FloatBuffer *out,
                          int width, int height)
{
    static const float weights[5] = {
        0.0448f, 0.2856f, 0.3001f, 0.2363f, 0.1333f
    };
    FloatBuffer cur_ref;
    FloatBuffer cur_out;
    FloatBuffer next_ref;
    FloatBuffer next_out;
    int cur_width;
    int cur_height;
    float weighted_sum;
    float weight_total;
    int level;
    float ssim_value;
    int next_width;
    int next_height;

    cur_ref = float_buffer_create((size_t)width * (size_t)height);
    cur_out = float_buffer_create((size_t)width * (size_t)height);
    memcpy(cur_ref.values, ref->values,
           sizeof(float) * (size_t)width * (size_t)height);
    memcpy(cur_out.values, out->values,
           sizeof(float) * (size_t)width * (size_t)height);
    cur_width = width;
    cur_height = height;
    weighted_sum = 0.0f;
    weight_total = 0.0f;

    for (level = 0; level < 5; ++level) {
        ssim_value = ssim_luma(&cur_ref, &cur_out, cur_width, cur_height,
                               0.01f, 0.03f, 11, 1.5f);
        weighted_sum += ssim_value * weights[level];
        weight_total += weights[level];
        if (level < 4) {
            next_ref = downsample2(&cur_ref, cur_width, cur_height,
                                   &next_width, &next_height);
            next_out = downsample2(&cur_out, cur_width, cur_height,
                                   &next_width, &next_height);
            float_buffer_free(&cur_ref);
            float_buffer_free(&cur_out);
            cur_ref = next_ref;
            cur_out = next_out;
            cur_width = next_width;
            cur_height = next_height;
        }
    }

    float_buffer_free(&cur_ref);
    float_buffer_free(&cur_out);

    if (weight_total > 0.0f) {
        return weighted_sum / weight_total;
    }
    return 0.0f;
}
/* ================================================================
 * FFT helpers for spectral metrics
 * ================================================================ */

static int next_power_of_two(int value)
{
    int n;
    n = 1;
    while (n < value) {
        n <<= 1;
    }
    return n;
}

static void fft_bit_reverse(Complex *data, int n)
{
    int i;
    int j;
    int bit;
    Complex tmp;

    j = 0;
    for (i = 0; i < n; ++i) {
        if (i < j) {
            tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        bit = n >> 1;
        while ((j & bit) != 0) {
            j &= ~bit;
            bit >>= 1;
        }
        j |= bit;
    }
}

static void fft_cooley_tukey(Complex *data, int n, int inverse)
{
    int len;
    double angle;
    Complex wlen;
    int half;
    int i;
    Complex w;
    int j;
    Complex u;
    Complex v;
    double tmp_re;
    double tmp_im;

    fft_bit_reverse(data, n);
    for (len = 2; len <= n; len <<= 1) {

        angle = 2.0 * M_PI / (double)len;
        if (inverse) {
            angle = -angle;
        }
        wlen.re = cos(angle);
        wlen.im = sin(angle);
        half = len >> 1;
        for (i = 0; i < n; i += len) {
            w.re = 1.0;
            w.im = 0.0;
            for (j = 0; j < half; ++j) {
                u = data[i + j];
                v.re = data[i + j + half].re * w.re -
                       data[i + j + half].im * w.im;
                v.im = data[i + j + half].re * w.im +
                       data[i + j + half].im * w.re;
                data[i + j].re = u.re + v.re;
                data[i + j].im = u.im + v.im;
                data[i + j + half].re = u.re - v.re;
                data[i + j + half].im = u.im - v.im;
                tmp_re = w.re * wlen.re - w.im * wlen.im;
                tmp_im = w.re * wlen.im + w.im * wlen.re;
                w.re = tmp_re;
                w.im = tmp_im;
            }
        }
    }
    if (inverse) {
        for (i = 0; i < n; ++i) {
            data[i].re /= n;
            data[i].im /= n;
        }
    }
}

static void fft2d(FloatBuffer *input, int width, int height,
                  Complex *output, int out_width, int out_height)
{
    int padded_width;
    int padded_height;
    int y;
    int x;
    Complex *row;
    Complex *col;
    Complex value;

    padded_width = out_width;
    padded_height = out_height;
    row = (Complex *)xmalloc(sizeof(Complex) * (size_t)padded_width);
    col = (Complex *)xmalloc(sizeof(Complex) * (size_t)padded_height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            if (y < height && x < width) {
                value.re = input->values[y * width + x];
                value.im = 0.0;
            } else {
                value.re = 0.0;
                value.im = 0.0;
            }
            output[y * padded_width + x] = value;
        }
    }

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            row[x] = output[y * padded_width + x];
        }
        fft_cooley_tukey(row, padded_width, 0);
        for (x = 0; x < padded_width; ++x) {
            output[y * padded_width + x] = row[x];
        }
    }

    for (x = 0; x < padded_width; ++x) {
        for (y = 0; y < padded_height; ++y) {
            col[y] = output[y * padded_width + x];
        }
        fft_cooley_tukey(col, padded_height, 0);
        for (y = 0; y < padded_height; ++y) {
            output[y * padded_width + x] = col[y];
        }
    }

    free(row);
    free(col);
}

static void fft_shift(Complex *data, int width, int height)
{
    int half_w;
    int half_h;
    int y;
    int x;
    int nx;
    int ny;
    Complex tmp;

    half_w = width / 2;
    half_h = height / 2;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            nx = (x + half_w) % width;
            ny = (y + half_h) % height;
            if (ny > y || (ny == y && nx > x)) {
                continue;
            }
            tmp = data[y * width + x];
            data[y * width + x] = data[ny * width + nx];
            data[ny * width + nx] = tmp;
        }
    }
}
static float high_frequency_ratio(const FloatBuffer *img,
                                  int width, int height, float cutoff)
{
    int padded_width;
    int padded_height;
    Complex *freq;
    size_t total;
    FloatBuffer centered;
    double hi_sum;
    double total_sum;
    int y;
    int x;
    double cy;
    double cx;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double norm;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    total = (size_t)padded_width * (size_t)padded_height;
    freq = (Complex *)xmalloc(total * sizeof(Complex));
    centered = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hi_sum = 0.0;
    total_sum = 0.0;
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            norm = r / (0.5 * sqrt((double)padded_height *
                                   (double)padded_height +
                                   (double)padded_width *
                                   (double)padded_width));
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            total_sum += power;
            if (norm >= cutoff) {
                hi_sum += power;
            }
        }
    }

    free(freq);
    float_buffer_free(&centered);

    if (total_sum <= 0.0) {
        return 0.0f;
    }
    return (float)(hi_sum / total_sum);
}

static float stripe_score(const FloatBuffer *img, int width, int height,
                          int bins)
{
    int padded_width;
    int padded_height;
    Complex *freq;
    FloatBuffer centered;
    double cy;
    double cx;
    double rmin;
    double *hist;
    int y;
    int x;
    double mean_val;
    double max_val;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double ang;
    int index;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    freq = (Complex *)xmalloc(sizeof(Complex) *
                              (size_t)padded_width * (size_t)padded_height);
    centered = float_buffer_create((size_t)width * (size_t)height);
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;
    rmin = 0.05 * (double)(width > height ? width : height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            if (r < rmin) {
                continue;
            }
            ang = atan2(dy, dx);
            if (ang < 0.0) {
                ang += M_PI;
            }
            index = (int)(ang / M_PI * bins);
            if (index < 0) {
                index = 0;
            }
            if (index >= bins) {
                index = bins - 1;
            }
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            hist[index] += power;
        }
    }

    mean_val = 0.0;
    for (x = 0; x < bins; ++x) {
        mean_val += hist[x];
    }
    mean_val = mean_val / (double)bins + 1e-12;

    max_val = hist[0];
    for (x = 1; x < bins; ++x) {
        if (hist[x] > max_val) {
            max_val = hist[x];
        }
    }

    free(hist);
    free(freq);
    float_buffer_free(&centered);

    return (float)(max_val / mean_val);
}
/* ================================================================
 * Banding metrics (run-length and gradient-based)
 * ================================================================ */

static float banding_index_runlen(const FloatBuffer *img,
                                  int width, int height, int levels)
{
    int y;
    int x;
    double total_runs;
    double total_segments;
    int prev;
    int run_len;
    int segs;
    int runs_sum;
    int value;

    total_runs = 0.0;
    total_segments = 0.0;
    for (y = 0; y < height; ++y) {
        prev = (int)clamp_float(img->values[y * width] * (levels - 1) + 0.5f,
                                0.0f, (float)(levels - 1));
        run_len = 1;
        segs = 0;
        runs_sum = 0;
        for (x = 1; x < width; ++x) {
            value = (int)clamp_float(
                img->values[y * width + x] * (levels - 1) + 0.5f,
                0.0f, (float)(levels - 1));
            if (value == prev) {
                run_len += 1;
            } else {
                runs_sum += run_len;
                segs += 1;
                run_len = 1;
                prev = value;
            }
        }
        runs_sum += run_len;
        segs += 1;
        total_runs += (double)runs_sum / (double)segs;
        total_segments += 1.0;
    }
    if (total_segments <= 0.0) {
        return 0.0f;
    }
    return (float)((total_runs / total_segments) / (double)width);
}

static FloatBuffer gaussian_blur(const FloatBuffer *img, int width,
                                 int height, float sigma, int ksize)
{
    FloatBuffer kernel;
    FloatBuffer blurred;

    kernel = gaussian_kernel1d(ksize, sigma);
    blurred = separable_conv2d(img, width, height, &kernel);
    float_buffer_free(&kernel);
    return blurred;
}

static void finite_diff(const FloatBuffer *img, int width, int height,
                        FloatBuffer *dx, FloatBuffer *dy)
{
    int x;
    int y;
    int xm1;
    int xp1;
    int ym1;
    int yp1;
    float vxm1;
    float vxp1;
    float vym1;
    float vyp1;

    *dx = float_buffer_create((size_t)width * (size_t)height);
    *dy = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            xm1 = x - 1;
            xp1 = x + 1;
            ym1 = y - 1;
            yp1 = y + 1;
            if (xm1 < 0) {
                xm1 = 0;
            }
            if (xp1 >= width) {
                xp1 = width - 1;
            }
            if (ym1 < 0) {
                ym1 = 0;
            }
            if (yp1 >= height) {
                yp1 = height - 1;
            }
            vxm1 = img->values[y * width + xm1];
            vxp1 = img->values[y * width + xp1];
            vym1 = img->values[ym1 * width + x];
            vyp1 = img->values[yp1 * width + x];
            dx->values[y * width + x] = (vxp1 - vxm1) * 0.5f;
            dy->values[y * width + x] = (vyp1 - vym1) * 0.5f;
        }
    }
}

static int compare_floats(const void *a, const void *b)
{
    float fa;
    float fb;
    fa = *(const float *)a;
    fb = *(const float *)b;
    if (fa < fb) {
        return -1;
    }
    if (fa > fb) {
        return 1;
    }
    return 0;
}

static float banding_index_gradient(const FloatBuffer *img,
                                    int width, int height)
{
    FloatBuffer blurred;
    FloatBuffer dx;
    FloatBuffer dy;
    FloatBuffer grad;
    size_t total;
    float *sorted;
    double g99;
    int bins;
    double *hist;
    double *centers;
    int half;
    int i;
    double sum_hist;
    double sum_residual;
    double zero_thresh;
    double zero_mass;
    double sum_x;
    double sum_y;
    double sum_xx;
    double sum_xy;
    int count;
    double slope;
    double intercept;
    double gx;
    double gy;
    size_t idx_pos;
    double value;
    int bin_index;
    double xval;
    double yval;
    double denom;
    double env;
    double resid;

    blurred = gaussian_blur(img, width, height, 1.0f, 7);
    finite_diff(&blurred, width, height, &dx, &dy);
    grad = float_buffer_create((size_t)width * (size_t)height);

    total = (size_t)width * (size_t)height;
    for (i = 0; (size_t)i < total; ++i) {
        gx = dx.values[i];
        gy = dy.values[i];
        grad.values[i] = (float)sqrt(gx * gx + gy * gy);
    }

    sorted = (float *)xmalloc(total * sizeof(float));
    memcpy(sorted, grad.values, total * sizeof(float));
    qsort(sorted, total, sizeof(float), compare_floats);
    if (total == 0) {
        g99 = 0.0;
    } else {
        idx_pos = (size_t)((double)(total - 1) * 0.995);
        g99 = sorted[idx_pos];
    }
    g99 += 1e-9;
    free(sorted);

    bins = 128;
    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    centers = (double *)xmalloc((size_t)bins * sizeof(double));
    for (i = 0; i < bins; ++i) {
        centers[i] = ((double)i + 0.5) * (g99 / (double)bins);
    }

    for (i = 0; (size_t)i < total; ++i) {
        if (grad.values[i] > (float)g99) {
            grad.values[i] = (float)g99;
        }
        value = grad.values[i];
        bin_index = (int)(value / g99 * bins);
        if (bin_index < 0) {
            bin_index = 0;
        }
        if (bin_index >= bins) {
            bin_index = bins - 1;
        }
        hist[bin_index] += 1.0;
    }

    for (i = 0; i < bins; ++i) {
        hist[i] += 1e-12;
    }

    half = bins / 2;
    sum_x = 0.0;
    sum_y = 0.0;
    sum_xx = 0.0;
    sum_xy = 0.0;
    count = bins - half;
    for (i = half; i < bins; ++i) {
        xval = centers[i];
        yval = log(hist[i]);
        sum_x += xval;
        sum_y += yval;
        sum_xx += xval * xval;
        sum_xy += xval * yval;
    }
    slope = 0.0;
    intercept = 0.0;
    if (count > 1) {
        denom = (double)count * sum_xx - sum_x * sum_x;
        if (fabs(denom) > 1e-12) {
            slope = ((double)count * sum_xy - sum_x * sum_y) / denom;
            intercept = (sum_y - slope * sum_x) / (double)count;
        }
    }

    sum_hist = 0.0;
    sum_residual = 0.0;
    for (i = 0; i < bins; ++i) {
        env = exp(intercept + slope * centers[i]);
        resid = hist[i] - env;
        if (resid < 0.0) {
            resid = 0.0;
        }
        sum_hist += hist[i];
        sum_residual += resid;
    }

    zero_thresh = 0.01 * g99;
    zero_mass = 0.0;
    if (total > 0) {
        for (i = 0; (size_t)i < total; ++i) {
            if (grad.values[i] <= (float)zero_thresh) {
                zero_mass += 1.0;
            }
        }
        zero_mass /= (double)total;
    }

    float_buffer_free(&blurred);
    float_buffer_free(&dx);
    float_buffer_free(&dy);
    float_buffer_free(&grad);
    free(hist);
    free(centers);

    if (sum_hist <= 0.0) {
        return 0.0f;
    }
    return (float)(0.6 * (sum_residual / sum_hist) + 0.4 * zero_mass);
}
/* ================================================================
 * Clipping statistics
 * ================================================================ */

static void clipping_rates(const Image *img, float *clip_l, float *clip_r,
                           float *clip_g, float *clip_b)
{
    FloatBuffer luma;
    FloatBuffer rch;
    FloatBuffer gch;
    FloatBuffer bch;
    size_t total;
    size_t i;
    double eps;
    double lo;
    double hi;

    luma = image_to_luma709(img);
    rch = image_channel(img, 0);
    gch = image_channel(img, 1);
    bch = image_channel(img, 2);
    total = (size_t)img->width * (size_t)img->height;
    eps = 1e-6;

    lo = 0.0;
    hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (luma.values[i] <= eps) {
            lo += 1.0;
        }
        if (luma.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_l = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (rch.values[i] <= eps) {
            lo += 1.0;
        }
        if (rch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_r = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (gch.values[i] <= eps) {
            lo += 1.0;
        }
        if (gch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_g = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (bch.values[i] <= eps) {
            lo += 1.0;
        }
        if (bch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_b = (float)((lo + hi) / (double)total);

    float_buffer_free(&luma);
    float_buffer_free(&rch);
    float_buffer_free(&gch);
    float_buffer_free(&bch);
}

/* ================================================================
 * sRGB <-> CIELAB conversions
 * ================================================================ */

static void srgb_to_linear(const float *src, float *dst, size_t count)
{
    size_t i;
    float c;
    float result;
    for (i = 0; i < count; ++i) {
        c = src[i];
        if (c <= 0.04045f) {
            result = c / 12.92f;
        } else {
            result = powf((c + 0.055f) / 1.055f, 2.4f);
        }
        dst[i] = result;
    }
}

static void linear_to_xyz(const float *rgb, float *xyz, size_t pixels)
{
    size_t i;
    float r;
    float g;
    float b;
    float X;
    float Y;
    float Z;
    for (i = 0; i < pixels; ++i) {
        r = rgb[i * 3 + 0];
        g = rgb[i * 3 + 1];
        b = rgb[i * 3 + 2];
        X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
        Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
        Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
        xyz[i * 3 + 0] = X;
        xyz[i * 3 + 1] = Y;
        xyz[i * 3 + 2] = Z;
    }
}

static float f_lab(float t)
{
    float delta;
    delta = 6.0f / 29.0f;
    if (t > delta * delta * delta) {
        return cbrtf(t);
    }
    return t / (3.0f * delta * delta) + 4.0f / 29.0f;
}

static void xyz_to_lab(const float *xyz, float *lab, size_t pixels)
{
    const float Xn = 0.95047f;
    const float Yn = 1.00000f;
    const float Zn = 1.08883f;
    size_t i;
    float X;
    float Y;
    float Z;
    float fx;
    float fy;
    float fz;
    float L;
    float a;
    float b;
    for (i = 0; i < pixels; ++i) {
        X = xyz[i * 3 + 0] / Xn;
        Y = xyz[i * 3 + 1] / Yn;
        Z = xyz[i * 3 + 2] / Zn;
        fx = f_lab(X);
        fy = f_lab(Y);
        fz = f_lab(Z);
        L = 116.0f * fy - 16.0f;
        a = 500.0f * (fx - fy);
        b = 200.0f * (fy - fz);
        lab[i * 3 + 0] = L;
        lab[i * 3 + 1] = a;
        lab[i * 3 + 2] = b;
    }
}

static FloatBuffer rgb_to_lab(const Image *img)
{
    FloatBuffer lab;
    float *linear;
    float *xyz;
    size_t pixels;

    pixels = (size_t)img->width * (size_t)img->height;
    lab = float_buffer_create(pixels * 3);
    linear = (float *)xmalloc(pixels * 3 * sizeof(float));
    xyz = (float *)xmalloc(pixels * 3 * sizeof(float));
    srgb_to_linear(img->pixels, linear, pixels * 3);
    linear_to_xyz(linear, xyz, pixels);
    xyz_to_lab(xyz, lab.values, pixels);
    free(linear);
    free(xyz);
    return lab;
}

static FloatBuffer chroma_ab(const FloatBuffer *lab, size_t pixels)
{
    FloatBuffer chroma;
    size_t i;
    float a;
    float b;
    chroma = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        a = lab->values[i * 3 + 1];
        b = lab->values[i * 3 + 2];
        chroma.values[i] = sqrtf(a * a + b * b);
    }
    return chroma;
}

static FloatBuffer deltaE00(const FloatBuffer *lab1,
                            const FloatBuffer *lab2, size_t pixels)
{
    FloatBuffer out;
    size_t i;
    double L1;
    double a1;
    double b1;
    double L2;
    double a2;
    double b2;
    double Lbar;
    double C1;
    double C2;
    double Cbar;
    double G;
    double a1p;
    double a2p;
    double C1p;
    double C2p;
    double h1p;
    double h2p;
    double dLp;
    double dCp;
    double dhp;
    double dHp;
    double Lpm;
    double Cpm;
    double hp_sum;
    double hp_diff;
    double Hpm;
    double T;
    double Sl;
    double Sc;
    double Sh;
    double dTheta;
    double Rc;
    double Rt;
    double de;
    out = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        L1 = lab1->values[i * 3 + 0];
        a1 = lab1->values[i * 3 + 1];
        b1 = lab1->values[i * 3 + 2];
        L2 = lab2->values[i * 3 + 0];
        a2 = lab2->values[i * 3 + 1];
        b2 = lab2->values[i * 3 + 2];
        Lbar = 0.5 * (L1 + L2);
        C1 = sqrt(a1 * a1 + b1 * b1);
        C2 = sqrt(a2 * a2 + b2 * b2);
        Cbar = 0.5 * (C1 + C2);
        G = 0.5 * (1.0 - sqrt(pow(Cbar, 7.0) /
                               (pow(Cbar, 7.0) + pow(25.0, 7.0) + 1e-12)));
        a1p = (1.0 + G) * a1;
        a2p = (1.0 + G) * a2;
        C1p = sqrt(a1p * a1p + b1 * b1);
        C2p = sqrt(a2p * a2p + b2 * b2);
        h1p = atan2(b1, a1p);
        if (h1p < 0.0) {
            h1p += 2.0 * M_PI;
        }
        h2p = atan2(b2, a2p);
        if (h2p < 0.0) {
            h2p += 2.0 * M_PI;
        }
        dLp = L2 - L1;
        dCp = C2p - C1p;
        dhp = h2p - h1p;
        if (dhp > M_PI) {
            dhp -= 2.0 * M_PI;
        }
        if (dhp < -M_PI) {
            dhp += 2.0 * M_PI;
        }
        dHp = 2.0 * sqrt(C1p * C2p + 1e-12) * sin(dhp / 2.0);
        Lpm = Lbar;
        Cpm = 0.5 * (C1p + C2p);
        hp_sum = h1p + h2p;
        hp_diff = fabs(h1p - h2p);
        if (C1p * C2p == 0.0) {
            Hpm = hp_sum;
        } else {
            if (hp_diff <= M_PI) {
                Hpm = 0.5 * hp_sum;
            } else {
                if (hp_sum < 2.0 * M_PI) {
                    Hpm = 0.5 * (hp_sum + 2.0 * M_PI);
                } else {
                    Hpm = 0.5 * (hp_sum - 2.0 * M_PI);
                }
            }
        }
        T = 1.0 - 0.17 * cos(Hpm - M_PI / 6.0) +
            0.24 * cos(2.0 * Hpm) +
            0.32 * cos(3.0 * Hpm + M_PI / 30.0) -
            0.20 * cos(4.0 * Hpm - 7.0 * M_PI / 20.0);
        Sl = 1.0 + ((0.015 * pow(Lpm - 50.0, 2.0)) /
                    sqrt(20.0 + pow(Lpm - 50.0, 2.0)));
        Sc = 1.0 + 0.045 * Cpm;
        Sh = 1.0 + 0.015 * Cpm * T;
        dTheta = 30.0 * exp(-pow((Hpm - 275.0 * M_PI / 180.0) /
                                 (25.0 * M_PI / 180.0), 2.0));
        Rc = 2.0 * sqrt(pow(Cpm, 7.0) /
                        (pow(Cpm, 7.0) + pow(25.0, 7.0) + 1e-12));
        Rt = -Rc * sin(2.0 * dTheta * M_PI / 180.0);
        de = sqrt(pow(dLp / (Sl), 2.0) + pow(dCp / (Sc), 2.0) +
                  pow(dHp / (Sh), 2.0) +
                  Rt * (dCp / Sc) * (dHp / Sh));
        out.values[i] = (float)de;
    }
    return out;
}
/* ================================================================
 * GMSD and PSNR
 * ================================================================ */

static float gmsd_metric(const FloatBuffer *ref, const FloatBuffer *out,
                         int width, int height)
{
    static const float kx[9] = {0.25f, 0.0f, -0.25f,
                                0.5f, 0.0f, -0.5f,
                                0.25f, 0.0f, -0.25f};
    static const float ky[9] = {0.25f, 0.5f, 0.25f,
                                0.0f, 0.0f, 0.0f,
                                -0.25f, -0.5f, -0.25f};
    FloatBuffer gx1;
    FloatBuffer gy1;
    FloatBuffer gx2;
    FloatBuffer gy2;
    FloatBuffer gm1;
    FloatBuffer gm2;
    double c;
    double mean;
    double sq_sum;
    size_t total;
    int y;
    int x;
    float accx1;
    float accy1;
    float accx2;
    float accy2;
    int dy;
    int dx;
    int yy;
    int xx;
    int kidx;
    size_t idx;
    float mag1;
    float mag2;
    double gms;

    gx1 = float_buffer_create((size_t)width * (size_t)height);
    gy1 = float_buffer_create((size_t)width * (size_t)height);
    gx2 = float_buffer_create((size_t)width * (size_t)height);
    gy2 = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            accx1 = 0.0f;
            accy1 = 0.0f;
            accx2 = 0.0f;
            accy2 = 0.0f;
            for (dy = -1; dy <= 1; ++dy) {
                yy = y + dy;
                if (yy < 0) {
                    yy = -yy;
                }
                if (yy >= height) {
                    yy = height - (yy - height) - 1;
                    if (yy < 0) {
                        yy = 0;
                    }
                }
                for (dx = -1; dx <= 1; ++dx) {
                    xx = x + dx;
                    if (xx < 0) {
                        xx = -xx;
                    }
                    if (xx >= width) {
                        xx = width - (xx - width) - 1;
                        if (xx < 0) {
                            xx = 0;
                        }
                    }
                    kidx = (dy + 1) * 3 + (dx + 1);
                    accx1 += ref->values[yy * width + xx] * kx[kidx];
                    accy1 += ref->values[yy * width + xx] * ky[kidx];
                    accx2 += out->values[yy * width + xx] * kx[kidx];
                    accy2 += out->values[yy * width + xx] * ky[kidx];
                }
            }
            gx1.values[y * width + x] = accx1;
            gy1.values[y * width + x] = accy1;
            gx2.values[y * width + x] = accx2;
            gy2.values[y * width + x] = accy2;
        }
    }

    gm1 = float_buffer_create((size_t)width * (size_t)height);
    gm2 = float_buffer_create((size_t)width * (size_t)height);
    total = (size_t)width * (size_t)height;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            mag1 = sqrtf(gx1.values[idx] * gx1.values[idx] +
                         gy1.values[idx] * gy1.values[idx]) + 1e-12f;
            mag2 = sqrtf(gx2.values[idx] * gx2.values[idx] +
                         gy2.values[idx] * gy2.values[idx]) + 1e-12f;
            gm1.values[idx] = mag1;
            gm2.values[idx] = mag2;
        }
    }

    c = 0.0026;
    mean = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            mean += gms;
        }
    }
    mean /= (double)total;

    sq_sum = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            sq_sum += (gms - mean) * (gms - mean);
        }
    }
    sq_sum /= (double)total;

    float_buffer_free(&gx1);
    float_buffer_free(&gy1);
    float_buffer_free(&gx2);
    float_buffer_free(&gy2);
    float_buffer_free(&gm1);
    float_buffer_free(&gm2);

    return (float)sqrt(sq_sum);
}

static float psnr_metric(const FloatBuffer *ref, const FloatBuffer *out,
                         int width, int height)
{
    double mse;
    size_t total;
    size_t i;
    double diff;

    mse = 0.0;
    total = (size_t)width * (size_t)height;
    for (i = 0; i < total; ++i) {
        diff = ref->values[i] - out->values[i];
        mse += diff * diff;
    }
    mse /= (double)total;
    if (mse <= 1e-12) {
        return 99.0f;
    }
    return (float)(10.0 * log10(1.0 / mse));
}

/* ================================================================
 * Metrics aggregation
 * ================================================================ */

static Metrics evaluate_metrics(const Image *ref_img, const Image *out_img)
{
    Metrics metrics;
    FloatBuffer ref_luma;
    FloatBuffer out_luma;
    FloatBuffer ref_lab;
    FloatBuffer out_lab;
    FloatBuffer ref_chroma;
    FloatBuffer out_chroma;
    FloatBuffer de00;
    size_t pixels;
    size_t iter;
    double sum_value;

    memset(&metrics, 0, sizeof(metrics));
    metrics.lpips_vgg = NAN;

    ref_luma = image_to_luma709(ref_img);
    out_luma = image_to_luma709(out_img);

    metrics.ms_ssim = ms_ssim_luma(&ref_luma, &out_luma,
                                   ref_img->width, ref_img->height);

    metrics.high_freq_out = high_frequency_ratio(&out_luma,
                                                 out_img->width,
                                                 out_img->height, 0.25f);
    metrics.high_freq_ref = high_frequency_ratio(&ref_luma,
                                                 ref_img->width,
                                                 ref_img->height, 0.25f);
    metrics.high_freq_delta = metrics.high_freq_out - metrics.high_freq_ref;

    metrics.stripe_ref = stripe_score(&ref_luma, ref_img->width,
                                      ref_img->height, 180);
    metrics.stripe_out = stripe_score(&out_luma, out_img->width,
                                      out_img->height, 180);
    metrics.stripe_rel = metrics.stripe_out - metrics.stripe_ref;

    metrics.band_run_rel = banding_index_runlen(&out_luma, out_img->width,
                                                out_img->height, 32) -
                           banding_index_runlen(&ref_luma, ref_img->width,
                                                ref_img->height, 32);

    metrics.band_grad_rel = banding_index_gradient(&out_luma,
                                                   out_img->width,
                                                   out_img->height) -
                            banding_index_gradient(&ref_luma,
                                                   ref_img->width,
                                                   ref_img->height);

    clipping_rates(ref_img, &metrics.clip_l_ref, &metrics.clip_r_ref,
                   &metrics.clip_g_ref, &metrics.clip_b_ref);
    clipping_rates(out_img, &metrics.clip_l_out, &metrics.clip_r_out,
                   &metrics.clip_g_out, &metrics.clip_b_out);

    metrics.clip_l_rel = metrics.clip_l_out - metrics.clip_l_ref;
    metrics.clip_r_rel = metrics.clip_r_out - metrics.clip_r_ref;
    metrics.clip_g_rel = metrics.clip_g_out - metrics.clip_g_ref;
    metrics.clip_b_rel = metrics.clip_b_out - metrics.clip_b_ref;

    ref_lab = rgb_to_lab(ref_img);
    out_lab = rgb_to_lab(out_img);
    pixels = (size_t)ref_img->width * (size_t)ref_img->height;
    ref_chroma = chroma_ab(&ref_lab, pixels);
    out_chroma = chroma_ab(&out_lab, pixels);

    sum_value = 0.0;
    for (iter = 0; iter < pixels; ++iter) {
        sum_value += fabs(out_chroma.values[iter] -
                          ref_chroma.values[iter]);
    }
    metrics.delta_chroma_mean = (float)(sum_value / (double)pixels);

    de00 = deltaE00(&ref_lab, &out_lab, pixels);
    sum_value = 0.0;
    for (iter = 0; iter < pixels; ++iter) {
        sum_value += de00.values[iter];
    }
    metrics.delta_e00_mean = (float)(sum_value / (double)pixels);

    metrics.gmsd_value = gmsd_metric(&ref_luma, &out_luma,
                                     ref_img->width, ref_img->height);
    metrics.psnr_y = psnr_metric(&ref_luma, &out_luma,
                                 ref_img->width, ref_img->height);

    float_buffer_free(&ref_luma);
    float_buffer_free(&out_luma);
    float_buffer_free(&ref_lab);
    float_buffer_free(&out_lab);
    float_buffer_free(&ref_chroma);
    float_buffer_free(&out_chroma);
    float_buffer_free(&de00);

    return metrics;
}

/* ================================================================
 * Optional LPIPS bridge (Python helper)
 * ================================================================ */

static int write_temp_ppm(const Image *img, char *templ_path,
                          size_t templ_size)
{
    int fd;
    FILE *fp;
    size_t pixels;
    size_t i;
    unsigned char pixel[3];
    int width;
    int height;

    if (templ_size < 1) {
        return -1;
    }
    fd = mkstemp(templ_path);
    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "wb");
    if (fp == NULL) {
        close(fd);
        unlink(templ_path);
        return -1;
    }

    width = img->width;
    height = img->height;
    if (fprintf(fp, "P6\n%d %d\n255\n", width, height) < 0) {
        fclose(fp);
        unlink(templ_path);
        return -1;
    }

    pixels = (size_t)width * (size_t)height;
    for (i = 0; i < pixels; ++i) {
        pixel[0] = float_to_byte(img->pixels[i * 3 + 0]);
        pixel[1] = float_to_byte(img->pixels[i * 3 + 1]);
        pixel[2] = float_to_byte(img->pixels[i * 3 + 2]);
        if (fwrite(pixel, sizeof(unsigned char), 3, fp) != 3) {
            fclose(fp);
            unlink(templ_path);
            return -1;
        }
    }

    if (fclose(fp) != 0) {
        unlink(templ_path);
        return -1;
    }
    return 0;
}

static int write_temp_script(char *templ_path, size_t templ_size)
{
    int fd;
    FILE *fp;

    if (templ_size < 1) {
        return -1;
    }
    fd = mkstemp(templ_path);
    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(templ_path);
        return -1;
    }
    if (fputs(lpips_script_source, fp) == EOF) {
        fclose(fp);
        unlink(templ_path);
        return -1;
    }
    if (fclose(fp) != 0) {
        unlink(templ_path);
        return -1;
    }
    return 0;
}

static float compute_lpips_vgg(const Image *ref_img, const Image *out_img)
{
    char ref_template[] = "/tmp/libsixel_lpips_ref_XXXXXX";
    char out_template[] = "/tmp/libsixel_lpips_out_XXXXXX";
    char script_template[] = "/tmp/libsixel_lpips_script_XXXXXX";
    float result;
    char command[512];
    FILE *pipe;
    char buffer[256];
    char *endptr;
    size_t len;
    int status;

    result = NAN;
    if (write_temp_ppm(ref_img, ref_template, sizeof(ref_template)) != 0) {
        return result;
    }
    if (write_temp_ppm(out_img, out_template, sizeof(out_template)) != 0) {
        unlink(ref_template);
        return result;
    }
    if (write_temp_script(script_template, sizeof(script_template)) != 0) {
        unlink(ref_template);
        unlink(out_template);
        return result;
    }

    len = snprintf(command, sizeof(command),
                   "python3 %s %s %s", script_template, ref_template,
                   out_template);
    if (len >= sizeof(command)) {
        unlink(ref_template);
        unlink(out_template);
        unlink(script_template);
        return result;
    }

    pipe = popen(command, "r");
    if (pipe == NULL) {
        unlink(ref_template);
        unlink(out_template);
        unlink(script_template);
        return result;
    }
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result = strtof(buffer, &endptr);
        if (endptr == buffer) {
            result = NAN;
        }
    }
    status = pclose(pipe);
    (void)status;

    unlink(ref_template);
    unlink(out_template);
    unlink(script_template);
    return result;
}

typedef struct MetricItem {
    const char *name;
    float value;
} MetricItem;

/* ================================================================
 * JSON writer and pretty stderr output
 * ================================================================ */

static void json_print_pair(FILE *fp, const char *key, float value, int last)
{
    if (isnan(value)) {
        fprintf(fp, "  \"%s\": NaN%s\n", key, last ? "" : ",");
    } else {
        fprintf(fp, "  \"%s\": %.6f%s\n", key, value, last ? "" : ",");
    }
}

static void write_metrics_json(FILE *fp, const Metrics *m)
{
    fprintf(fp, "{\n");
    json_print_pair(fp, "MS-SSIM", m->ms_ssim, 0);
    json_print_pair(fp, "HighFreqRatio_out", m->high_freq_out, 0);
    json_print_pair(fp, "HighFreqRatio_ref", m->high_freq_ref, 0);
    json_print_pair(fp, "HighFreqRatio_delta", m->high_freq_delta, 0);
    json_print_pair(fp, "StripeScore_ref", m->stripe_ref, 0);
    json_print_pair(fp, "StripeScore_out", m->stripe_out, 0);
    json_print_pair(fp, "StripeScore_rel", m->stripe_rel, 0);
    json_print_pair(fp, "BandingIndex_rel", m->band_run_rel, 0);
    json_print_pair(fp, "BandingIndex_grad_rel", m->band_grad_rel, 0);
    json_print_pair(fp, "ClipRate_L_ref", m->clip_l_ref, 0);
    json_print_pair(fp, "ClipRate_R_ref", m->clip_r_ref, 0);
    json_print_pair(fp, "ClipRate_G_ref", m->clip_g_ref, 0);
    json_print_pair(fp, "ClipRate_B_ref", m->clip_b_ref, 0);
    json_print_pair(fp, "ClipRate_L_out", m->clip_l_out, 0);
    json_print_pair(fp, "ClipRate_R_out", m->clip_r_out, 0);
    json_print_pair(fp, "ClipRate_G_out", m->clip_g_out, 0);
    json_print_pair(fp, "ClipRate_B_out", m->clip_b_out, 0);
    json_print_pair(fp, "ClipRate_L_rel", m->clip_l_rel, 0);
    json_print_pair(fp, "ClipRate_R_rel", m->clip_r_rel, 0);
    json_print_pair(fp, "ClipRate_G_rel", m->clip_g_rel, 0);
    json_print_pair(fp, "ClipRate_B_rel", m->clip_b_rel, 0);
    json_print_pair(fp, "Δ Chroma_mean", m->delta_chroma_mean, 0);
    json_print_pair(fp, "Δ E00_mean", m->delta_e00_mean, 0);
    json_print_pair(fp, "GMSD", m->gmsd_value, 0);
    json_print_pair(fp, "PSNR_Y", m->psnr_y, 0);
    json_print_pair(fp, "LPIPS(vgg)", m->lpips_vgg, 1);
    fprintf(fp, "}\n");
}

static void verbose_print(const Metrics *m)
{
    MetricItem items[] = {
        {"MS-SSIM", m->ms_ssim},
        {"HighFreqRatio_out", m->high_freq_out},
        {"HighFreqRatio_ref", m->high_freq_ref},
        {"HighFreqRatio_delta", m->high_freq_delta},
        {"StripeScore_ref", m->stripe_ref},
        {"StripeScore_out", m->stripe_out},
        {"StripeScore_rel", m->stripe_rel},
        {"BandingIndex_rel", m->band_run_rel},
        {"BandingIndex_grad_rel", m->band_grad_rel},
        {"ClipRate_L_ref", m->clip_l_ref},
        {"ClipRate_R_ref", m->clip_r_ref},
        {"ClipRate_G_ref", m->clip_g_ref},
        {"ClipRate_B_ref", m->clip_b_ref},
        {"ClipRate_L_out", m->clip_l_out},
        {"ClipRate_R_out", m->clip_r_out},
        {"ClipRate_G_out", m->clip_g_out},
        {"ClipRate_B_out", m->clip_b_out},
        {"ClipRate_L_rel", m->clip_l_rel},
        {"ClipRate_R_rel", m->clip_r_rel},
        {"ClipRate_G_rel", m->clip_g_rel},
        {"ClipRate_B_rel", m->clip_b_rel},
        {"Δ Chroma_mean", m->delta_chroma_mean},
        {"Δ E00_mean", m->delta_e00_mean},
        {"GMSD", m->gmsd_value},
        {"PSNR_Y", m->psnr_y},
        {"LPIPS(vgg)", m->lpips_vgg},
    };
    size_t i;
    fprintf(stderr, "\n=== Image Quality Report (Raw Metrics) ===\n");
    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        if (isnan(items[i].value)) {
            fprintf(stderr, "%24s: NaN\n", items[i].name);
        } else {
            fprintf(stderr, "%24s: %.6f\n", items[i].name,
                    items[i].value);
        }
    }
}
/* ================================================================
 * Image alignment helper
 * ================================================================ */

static void align_images(Image *ref, Image *out)
{
    int width;
    int height;
    int channels;
    float *ref_new;
    float *out_new;
    int y;
    size_t row_bytes;

    width = ref->width < out->width ? ref->width : out->width;
    height = ref->height < out->height ? ref->height : out->height;
    channels = ref->channels;
    if (channels != out->channels) {
        fprintf(stderr, "Channel mismatch between images\n");
        exit(EXIT_FAILURE);
    }
    ref_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    out_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    row_bytes = (size_t)width * (size_t)channels * sizeof(float);
    for (y = 0; y < height; ++y) {
        memcpy(ref_new + (size_t)y * (size_t)width * (size_t)channels,
               ref->pixels + (size_t)y * (size_t)ref->width *
               (size_t)channels, row_bytes);
        memcpy(out_new + (size_t)y * (size_t)width * (size_t)channels,
               out->pixels + (size_t)y * (size_t)out->width *
               (size_t)channels, row_bytes);
    }
    free(ref->pixels);
    free(out->pixels);
    ref->pixels = ref_new;
    out->pixels = out_new;
    ref->width = width;
    ref->height = height;
    out->width = width;
    out->height = height;
}

/* ================================================================
 * Argument parsing and main orchestration
 * ================================================================ */

typedef struct Options {
    const char *ref_path;
    const char *out_path;
    const char *prefix;
    int verbose;
} Options;

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --ref <path> [--out <path>] [--prefix <p>] [-v]\n",
            prog);
}

static int parse_args(int argc, char **argv, Options *opts)
{
    int i;
    opts->ref_path = NULL;
    opts->out_path = "-";
    opts->prefix = "report";
    opts->verbose = 0;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ref") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--ref requires a value\n");
                return -1;
            }
            opts->ref_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--out requires a value\n");
                return -1;
            }
            opts->out_path = argv[++i];
        } else if (strcmp(argv[i], "--prefix") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--prefix requires a value\n");
                return -1;
            }
            opts->prefix = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 ||
                   strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = 1;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return -1;
        }
    }

    if (opts->ref_path == NULL) {
        fprintf(stderr, "--ref is required\n");
        return -1;
    }
    return 0;
}
int main(int argc, char **argv)
{
    Options opts;
    Image ref_img;
    Image out_img;
    Metrics metrics;
    int status;
    size_t path_len;
    char *json_path;
    FILE *fp;

    ref_img.width = 0;
    ref_img.height = 0;
    ref_img.channels = 0;
    ref_img.pixels = NULL;
    out_img = ref_img;

    status = parse_args(argc, argv, &opts);
    if (status != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (load_image(opts.ref_path, &ref_img) != 0) {
        fprintf(stderr, "Failed to load reference image\n");
        return EXIT_FAILURE;
    }
    if (load_image(opts.out_path, &out_img) != 0) {
        fprintf(stderr, "Failed to load output image\n");
        image_free(&ref_img);
        return EXIT_FAILURE;
    }

    align_images(&ref_img, &out_img);

    metrics = evaluate_metrics(&ref_img, &out_img);
    metrics.lpips_vgg = compute_lpips_vgg(&ref_img, &out_img);

    if (opts.verbose) {
        verbose_print(&metrics);
    }

    path_len = strlen(opts.prefix) + strlen("_metrics.json") + 1;
    json_path = (char *)xmalloc(path_len);
    snprintf(json_path, path_len, "%s_metrics.json", opts.prefix);
    fp = fopen(json_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s for writing: %s\n",
                json_path, strerror(errno));
        free(json_path);
        image_free(&ref_img);
        image_free(&out_img);
        return EXIT_FAILURE;
    }
    write_metrics_json(fp, &metrics);
    fclose(fp);
    if (opts.verbose) {
        fprintf(stderr, "\nWrote: %s\n", json_path);
    }

    write_metrics_json(stdout, &metrics);

    free(json_path);
    image_free(&ref_img);
    image_free(&out_img);
    return EXIT_SUCCESS;
}
