/*
 * Copyright (c) 2014-2018 Hayaki Saito
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

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_UNISTD_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */

#include "decoder.h"
#include "frame.h"


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
        strcpy(p, s);
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
    (*ppdecoder)->thumbnail_size = 0;

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

#if 0
   /*    original: n = (p1 + p2) / 2
    */
    avg_color[0] = (unsigned char)(((unsigned int)p1[0]
                                    + (unsigned int)p2[0]) >> 1);
    avg_color[1] = (unsigned char)(((unsigned int)p1[1]
                                    + (unsigned int)p2[1]) >> 1);
    avg_color[2] = (unsigned char)(((unsigned int)p1[2]
                                    + (unsigned int)p2[2]) >> 1);
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
    avg_color[0] = (p1[0] * numerator + p2[0] * denominator)
                 / (numerator + denominator);
    avg_color[1] = (p1[1] * numerator + p2[1] * denominator)
                 / (numerator + denominator);
    avg_color[2] = (p1[2] * numerator + p2[2] * denominator)
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
        result = 12U;
    } else if (min_diff >= base_distance) {
        result = 8U;
    } else if ((unsigned long long)min_diff * 6ULL
               >= (unsigned long long)base_distance * 5ULL) {
        result = 8U;
    } else if ((unsigned long long)min_diff * 4ULL
               >= (unsigned long long)base_distance * 3ULL) {
        result = 7U;
    } else if ((unsigned long long)min_diff * 3ULL
               >= (unsigned long long)base_distance * 2ULL) {
        result = 5U;
    } else if ((unsigned long long)min_diff * 5ULL
               >= (unsigned long long)base_distance * 3ULL) {
        result = 4U;
    } else if ((unsigned long long)min_diff * 2ULL
               >= (unsigned long long)base_distance * 1ULL) {
        result = 3U;
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
        {-1, -1,  2, 16}, {0, -1, 10, 16}, {1, -1,  6, 16},
        {-1,  0, 14, 16},                  {1,  0, 14, 16},
        {-1,  1,  6, 16}, {0,  1, 10, 16}, {1,  1,  2, 16}
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
    status = SIXEL_OK;

end:
    sixel_similarity_destroy(&similarity, allocator);
    sixel_allocator_free(allocator, rgb);
    sixel_allocator_free(allocator, gray);
    sixel_allocator_free(allocator, prewitt);
    return status;
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

    sixel_decoder_ref(decoder);

    switch(arg) {
    case SIXEL_OPTFLAG_INPUT:  /* i */
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
        free(decoder->output);
        decoder->output = strdup_with_allocator(value, decoder->allocator);
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

        if (strcmp(value, "none") == 0) {
            decoder->dequantize_method = SIXEL_DEQUANTIZE_NONE;
        } else if (strcmp(value, "k_undither") == 0) {
            decoder->dequantize_method = SIXEL_DEQUANTIZE_K_UNDITHER;
        } else {
            sixel_helper_set_additional_message(
                "unsupported dequantize method.");
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
    unsigned char *indexed_pixels = NULL;
    unsigned char *palette = NULL;
    unsigned char *rgb_pixels = NULL;
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

    sixel_decoder_ref(decoder);

    frame = NULL;
    new_width = 0;
    new_height = 0;
    scaled_width = 0.0;
    scaled_height = 0.0;
    max_dimension = 0;
    thumbnail_size = decoder->thumbnail_size;
    frame_ncolors = -1;

    if (strcmp(decoder->input, "-") == 0) {
        /* for windows */
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        input_fp = stdin;
    } else {
        input_fp = fopen(decoder->input, "rb");
        if (!input_fp) {
            sixel_helper_set_additional_message(
                "sixel_decoder_decode: fopen() failed.");
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

    if (input_fp != stdout) {
        fclose(input_fp);
    }

    status = sixel_decode_raw(
        raw_data,
        raw_len,
        &indexed_pixels,
        &sx,
        &sy,
        &palette,
        &ncolors,
        decoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (sx > SIXEL_WIDTH_LIMIT || sy > SIXEL_HEIGHT_LIMIT) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    output_pixels = indexed_pixels;
    output_palette = palette;
    output_pixelformat = SIXEL_PIXELFORMAT_PAL8;

    if (decoder->dequantize_method == SIXEL_DEQUANTIZE_K_UNDITHER) {
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
            goto end;
        }
        output_pixels = rgb_pixels;
        output_palette = NULL;
        output_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    }

    if (output_pixelformat == SIXEL_PIXELFORMAT_PAL8) {
        frame_ncolors = ncolors;
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

    status = sixel_helper_write_image_file(
        output_pixels,
        sx,
        sy,
        output_palette,
        output_pixelformat,
        decoder->output,
        SIXEL_FORMAT_PNG,
        decoder->allocator);

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_frame_unref(frame);
    sixel_allocator_free(decoder->allocator, raw_data);
    sixel_allocator_free(decoder->allocator, indexed_pixels);
    sixel_allocator_free(decoder->allocator, palette);
    sixel_allocator_free(decoder->allocator, rgb_pixels);

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
