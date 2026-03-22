/*
 * SPDX-License-Identifier: MIT
 *
 * Small JPEG -> PPM reference decoder for loader expected image generation.
 * This tool intentionally bypasses libsixel loader paths so tests can keep
 * fixed references without using img2sixel to produce expected images.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_JPEG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <setjmp.h>
#include <jpeglib.h>

#if defined(HAVE_JPEG12_API) && HAVE_JPEG12_API
# define JPEGREF_HAS_JPEG12 1
#elif defined(J12SAMPLE) && defined(MAXJ12SAMPLE)
# define JPEGREF_HAS_JPEG12 1
#else
# define JPEGREF_HAS_JPEG12 0
#endif

#if defined(HAVE_JPEG16_API) && HAVE_JPEG16_API
# define JPEGREF_HAS_JPEG16 1
#elif defined(J16SAMPLE) && defined(MAXJ16SAMPLE)
# define JPEGREF_HAS_JPEG16 1
#else
# define JPEGREF_HAS_JPEG16 0
#endif

typedef struct jpegref_error_mgr {
    struct jpeg_error_mgr base;
    jmp_buf jmpbuf;
    char message[JMSG_LENGTH_MAX];
} jpegref_error_mgr_t;

static void
jpegref_error_exit(j_common_ptr cinfo)
{
    jpegref_error_mgr_t *err;

    err = NULL;
    if (cinfo == NULL || cinfo->err == NULL) {
        return;
    }

    err = (jpegref_error_mgr_t *)cinfo->err;
    err->message[0] = '\0';
    (*cinfo->err->format_message)(cinfo, err->message);
    longjmp(err->jmpbuf, 1);
}

#if JPEGREF_HAS_JPEG12 || JPEGREF_HAS_JPEG16
/* libjpeg 12/16-bit samples are 16-bit unsigned values. */
# if HAVE_STDINT_H
typedef uint16_t jpegref_sample_t;
# else
typedef unsigned short jpegref_sample_t;
# endif

static unsigned char
jpegref_scale_to_u8(unsigned int sample, unsigned int max_sample)
{
    if (max_sample == 0u) {
        return 0u;
    }
    if (sample >= max_sample) {
        return 255u;
    }
    return (unsigned char)((sample * 255u + max_sample / 2u) / max_sample);
}
#endif

static unsigned char
jpegref_cmyk_to_rgb_u8(unsigned int c,
                       unsigned int m,
                       unsigned int y,
                       unsigned int k,
                       unsigned int max_sample,
                       int channel)
{
    double scale;
    double cd;
    double md;
    double yd;
    double kd;
    double out;

    if (max_sample == 0u) {
        return 0u;
    }
    scale = 1.0 / (double)max_sample;
    cd = (double)c * scale;
    md = (double)m * scale;
    yd = (double)y * scale;
    kd = (double)k * scale;
    switch (channel) {
    case 0:
        out = cd * kd;
        break;
    case 1:
        out = md * kd;
        break;
    default:
        out = yd * kd;
        break;
    }
    if (out < 0.0) {
        out = 0.0;
    } else if (out > 1.0) {
        out = 1.0;
    }
    return (unsigned char)(out * 255.0 + 0.5);
}

static int
jpegref_convert_row_u8(unsigned char const *src,
                       unsigned char *dst,
                       size_t width,
                       int components)
{
    size_t x;

    if (src == NULL || dst == NULL) {
        return 0;
    }
    for (x = 0u; x < width; ++x) {
        size_t src_base;
        size_t dst_base;

        src_base = x * (size_t)components;
        dst_base = x * 3u;
        if (components == 1) {
            dst[dst_base + 0u] = src[src_base + 0u];
            dst[dst_base + 1u] = src[src_base + 0u];
            dst[dst_base + 2u] = src[src_base + 0u];
        } else if (components == 3) {
            dst[dst_base + 0u] = src[src_base + 0u];
            dst[dst_base + 1u] = src[src_base + 1u];
            dst[dst_base + 2u] = src[src_base + 2u];
        } else if (components == 4) {
            dst[dst_base + 0u] = jpegref_cmyk_to_rgb_u8(src[src_base + 0u],
                                                        src[src_base + 1u],
                                                        src[src_base + 2u],
                                                        src[src_base + 3u],
                                                        255u,
                                                        0);
            dst[dst_base + 1u] = jpegref_cmyk_to_rgb_u8(src[src_base + 0u],
                                                        src[src_base + 1u],
                                                        src[src_base + 2u],
                                                        src[src_base + 3u],
                                                        255u,
                                                        1);
            dst[dst_base + 2u] = jpegref_cmyk_to_rgb_u8(src[src_base + 0u],
                                                        src[src_base + 1u],
                                                        src[src_base + 2u],
                                                        src[src_base + 3u],
                                                        255u,
                                                        2);
        } else {
            return 0;
        }
    }
    return 1;
}

#if JPEGREF_HAS_JPEG12 || JPEGREF_HAS_JPEG16
static int
jpegref_convert_row_u16(jpegref_sample_t const *src,
                        unsigned char *dst,
                        size_t width,
                        int components,
                        unsigned int max_sample)
{
    size_t x;

    if (src == NULL || dst == NULL || max_sample == 0u) {
        return 0;
    }
    for (x = 0u; x < width; ++x) {
        size_t src_base;
        size_t dst_base;
        unsigned int s0;
        unsigned int s1;
        unsigned int s2;
        unsigned int s3;

        src_base = x * (size_t)components;
        dst_base = x * 3u;
        s0 = (unsigned int)src[src_base + 0u];
        s1 = 0u;
        s2 = 0u;
        s3 = 0u;
        if (components > 1) {
            s1 = (unsigned int)src[src_base + 1u];
        }
        if (components > 2) {
            s2 = (unsigned int)src[src_base + 2u];
        }
        if (components > 3) {
            s3 = (unsigned int)src[src_base + 3u];
        }

        if (components == 1) {
            unsigned char gray;

            gray = jpegref_scale_to_u8(s0, max_sample);
            dst[dst_base + 0u] = gray;
            dst[dst_base + 1u] = gray;
            dst[dst_base + 2u] = gray;
        } else if (components == 3) {
            dst[dst_base + 0u] = jpegref_scale_to_u8(s0, max_sample);
            dst[dst_base + 1u] = jpegref_scale_to_u8(s1, max_sample);
            dst[dst_base + 2u] = jpegref_scale_to_u8(s2, max_sample);
        } else if (components == 4) {
            dst[dst_base + 0u] = jpegref_cmyk_to_rgb_u8(s0,
                                                        s1,
                                                        s2,
                                                        s3,
                                                        max_sample,
                                                        0);
            dst[dst_base + 1u] = jpegref_cmyk_to_rgb_u8(s0,
                                                        s1,
                                                        s2,
                                                        s3,
                                                        max_sample,
                                                        1);
            dst[dst_base + 2u] = jpegref_cmyk_to_rgb_u8(s0,
                                                        s1,
                                                        s2,
                                                        s3,
                                                        max_sample,
                                                        2);
        } else {
            return 0;
        }
    }
    return 1;
}
#endif

static void
jpegref_print_usage(char const *prog)
{
    fprintf(stderr, "usage: %s <input.jpg> <output.ppm|->\n", prog);
}

int
main(int argc, char **argv)
{
    char const *input_path;
    char const *output_path;
    FILE *input_fp;
    FILE *output_fp;
    struct jpeg_decompress_struct cinfo;
    jpegref_error_mgr_t jerr;
    unsigned char *row_rgb;
    int status;
    int started;
    int decoded;

#if JPEGREF_HAS_JPEG12
    J12SAMPARRAY row12;
#endif
#if JPEGREF_HAS_JPEG16
    J16SAMPARRAY row16;
#endif
    JSAMPARRAY row8;
    JDIMENSION row_stride;

    input_path = NULL;
    output_path = NULL;
    input_fp = NULL;
    output_fp = NULL;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));
    row_rgb = NULL;
    status = 1;
    started = 0;
    decoded = 0;
#if JPEGREF_HAS_JPEG12
    row12 = NULL;
#endif
#if JPEGREF_HAS_JPEG16
    row16 = NULL;
#endif
    row8 = NULL;
    row_stride = 0u;

    if (argc != 3) {
        jpegref_print_usage(argv[0]);
        return 2;
    }

    input_path = argv[1];
    output_path = argv[2];

    if (strcmp(input_path, "-") == 0) {
        input_fp = stdin;
    } else {
        input_fp = fopen(input_path, "rb");
        if (input_fp == NULL) {
            fprintf(stderr, "jpegref: failed to open input: %s\n", input_path);
            goto cleanup;
        }
    }

    if (strcmp(output_path, "-") == 0) {
        output_fp = stdout;
    } else {
        output_fp = fopen(output_path, "wb");
        if (output_fp == NULL) {
            fprintf(stderr, "jpegref: failed to open output: %s\n", output_path);
            goto cleanup;
        }
    }

    cinfo.err = jpeg_std_error(&jerr.base);
    jerr.base.error_exit = jpegref_error_exit;
    if (setjmp(jerr.jmpbuf) != 0) {
        if (jerr.message[0] != '\0') {
            fprintf(stderr, "jpegref: %s\n", jerr.message);
        } else {
            fprintf(stderr, "jpegref: libjpeg decode error\n");
        }
        goto cleanup;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, input_fp);
    (void)jpeg_read_header(&cinfo, TRUE);

    if (cinfo.jpeg_color_space == JCS_CMYK ||
        cinfo.jpeg_color_space == JCS_YCCK) {
        cinfo.out_color_space = JCS_CMYK;
    } else if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
        cinfo.out_color_space = JCS_GRAYSCALE;
    } else {
        cinfo.out_color_space = JCS_RGB;
    }

    (void)jpeg_start_decompress(&cinfo);
    started = 1;
    if (cinfo.output_width == 0u || cinfo.output_height == 0u) {
        fprintf(stderr, "jpegref: empty JPEG image\n");
        goto cleanup;
    }
    if (cinfo.output_components != 1u &&
        cinfo.output_components != 3u &&
        cinfo.output_components != 4u) {
        fprintf(stderr, "jpegref: unsupported output component count: %u\n",
                cinfo.output_components);
        goto cleanup;
    }

    if ((size_t)cinfo.output_width > SIZE_MAX / 3u) {
        fprintf(stderr, "jpegref: width overflow\n");
        goto cleanup;
    }
    row_rgb = (unsigned char *)malloc((size_t)cinfo.output_width * 3u);
    if (row_rgb == NULL) {
        fprintf(stderr, "jpegref: out of memory\n");
        goto cleanup;
    }

    fprintf(output_fp, "P6\n%u %u\n255\n", cinfo.output_width, cinfo.output_height);

    row_stride = cinfo.output_width * cinfo.output_components;
    if (cinfo.data_precision <= 8) {
        row8 = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                          JPOOL_IMAGE,
                                          row_stride,
                                          1u);
        while (cinfo.output_scanline < cinfo.output_height) {
            if (jpeg_read_scanlines(&cinfo, row8, 1u) != 1u) {
                fprintf(stderr, "jpegref: jpeg_read_scanlines failed\n");
                goto cleanup;
            }
            if (!jpegref_convert_row_u8(row8[0],
                                        row_rgb,
                                        (size_t)cinfo.output_width,
                                        (int)cinfo.output_components)) {
                fprintf(stderr, "jpegref: row conversion failed\n");
                goto cleanup;
            }
            if (fwrite(row_rgb,
                       1u,
                       (size_t)cinfo.output_width * 3u,
                       output_fp) != (size_t)cinfo.output_width * 3u) {
                fprintf(stderr, "jpegref: failed to write output row\n");
                goto cleanup;
            }
        }
    } else if (cinfo.data_precision <= 12) {
#if JPEGREF_HAS_JPEG12
        unsigned int max_sample12;

        row12 = (J12SAMPARRAY)(*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                                         JPOOL_IMAGE,
                                                         row_stride,
                                                         1u);
# if defined(MAXJ12SAMPLE)
        max_sample12 = (unsigned int)MAXJ12SAMPLE;
# else
        max_sample12 = (1u << 12u) - 1u;
# endif
        while (cinfo.output_scanline < cinfo.output_height) {
            if (jpeg12_read_scanlines(&cinfo, row12, 1u) != 1u) {
                fprintf(stderr, "jpegref: jpeg12_read_scanlines failed\n");
                goto cleanup;
            }
            if (!jpegref_convert_row_u16((jpegref_sample_t const *)row12[0],
                                         row_rgb,
                                         (size_t)cinfo.output_width,
                                         (int)cinfo.output_components,
                                         max_sample12)) {
                fprintf(stderr, "jpegref: row conversion failed\n");
                goto cleanup;
            }
            if (fwrite(row_rgb,
                       1u,
                       (size_t)cinfo.output_width * 3u,
                       output_fp) != (size_t)cinfo.output_width * 3u) {
                fprintf(stderr, "jpegref: failed to write output row\n");
                goto cleanup;
            }
        }
#else
        fprintf(stderr, "jpegref: 12-bit JPEG API is unavailable in this build\n");
        goto cleanup;
#endif
    } else if (cinfo.data_precision <= 16) {
#if JPEGREF_HAS_JPEG16
        unsigned int max_sample16;

        row16 = (J16SAMPARRAY)(*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                                         JPOOL_IMAGE,
                                                         row_stride,
                                                         1u);
# if defined(MAXJ16SAMPLE)
        max_sample16 = (unsigned int)MAXJ16SAMPLE;
# else
        max_sample16 = 65535u;
# endif
        while (cinfo.output_scanline < cinfo.output_height) {
            if (jpeg16_read_scanlines(&cinfo, row16, 1u) != 1u) {
                fprintf(stderr, "jpegref: jpeg16_read_scanlines failed\n");
                goto cleanup;
            }
            if (!jpegref_convert_row_u16((jpegref_sample_t const *)row16[0],
                                         row_rgb,
                                         (size_t)cinfo.output_width,
                                         (int)cinfo.output_components,
                                         max_sample16)) {
                fprintf(stderr, "jpegref: row conversion failed\n");
                goto cleanup;
            }
            if (fwrite(row_rgb,
                       1u,
                       (size_t)cinfo.output_width * 3u,
                       output_fp) != (size_t)cinfo.output_width * 3u) {
                fprintf(stderr, "jpegref: failed to write output row\n");
                goto cleanup;
            }
        }
#else
        fprintf(stderr, "jpegref: 16-bit JPEG API is unavailable in this build\n");
        goto cleanup;
#endif
    } else {
        fprintf(stderr, "jpegref: unsupported JPEG precision: %u\n",
                cinfo.data_precision);
        goto cleanup;
    }

    decoded = 1;
    status = 0;

cleanup:
    if (cinfo.mem != NULL) {
        if (started) {
            if (decoded) {
                (void)jpeg_finish_decompress(&cinfo);
            } else {
                jpeg_abort_decompress(&cinfo);
            }
        }
        jpeg_destroy_decompress(&cinfo);
    }
    if (row_rgb != NULL) {
        free(row_rgb);
    }
    if (output_fp != NULL && output_fp != stdout) {
        fclose(output_fp);
    }
    if (input_fp != NULL && input_fp != stdin) {
        fclose(input_fp);
    }
    return status;
}

#else

#include <stdio.h>

int
main(void)
{
    fprintf(stderr, "jpegref: JPEG support is disabled in this build\n");
    return 1;
}

#endif
