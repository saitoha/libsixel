#include "config.h"

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <string.h>

#include "dither-fixed-float32.h"
#include "dither-common-pipeline.h"

static unsigned char
sixel_dither_float_channel_to_byte(float value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }

    return (unsigned char)(value * 255.0f + 0.5f);
}

static float
sixel_dither_byte_to_float(unsigned char value)
{
    return (float)value / 255.0f;
}

static void
error_diffuse_float(float *data,
                    int pos,
                    int depth,
                    float error,
                    int numerator,
                    int denominator)
{
    float *channel;
    float delta;

    channel = data + ((size_t)pos * (size_t)depth);
    delta = error * ((float)numerator / (float)denominator);
    *channel += delta;
    if (*channel < 0.0f) {
        *channel = 0.0f;
    } else if (*channel > 1.0f) {
        *channel = 1.0f;
    }
}

static void
sixel_dither_scanline_params(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

static void
diffuse_fs_float(float *data,
                 int width,
                 int height,
                 int x,
                 int y,
                 int depth,
                 float error,
                 int direction)
{
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_float(data, pos + 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x > 0) {
                error_diffuse_float(data,
                                    pos + width - 1,
                                    depth, error, 3, 16);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth, error, 5, 16);
            if (x < width - 1) {
                error_diffuse_float(data,
                                    pos + width + 1,
                                    depth, error, 1, 16);
            }
        }
    } else {
        if (x > 0) {
            error_diffuse_float(data, pos - 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                error_diffuse_float(data,
                                    pos + width + 1,
                                    depth, error, 3, 16);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth, error, 5, 16);
            if (x > 0) {
                error_diffuse_float(data,
                                    pos + width - 1,
                                    depth, error, 1, 16);
            }
        }
    }
}

#if HAVE_TESTS
static int g_sixel_dither_float32_diffusion_hits = 0;

void
sixel_dither_diffusion_tests_reset_float32_hits(void)
{
    g_sixel_dither_float32_diffusion_hits = 0;
}

int
sixel_dither_diffusion_tests_float32_hits(void)
{
    return g_sixel_dither_float32_diffusion_hits;
}

#define SIXEL_DITHER_FLOAT32_HIT()                                      \
    do {                                                                \
        ++g_sixel_dither_float32_diffusion_hits;                        \
    } while (0)
#else
#define SIXEL_DITHER_FLOAT32_HIT()                                      \
    do {                                                                \
    } while (0)
#endif

SIXELSTATUS
sixel_dither_apply_fixed_float32(sixel_dither_t *dither,
                                 sixel_dither_context_t *context)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    SIXELSTATUS status;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int serpentine;
    int y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    float *source_pixel;
    unsigned char quantized[max_channels];
    float snapshot[max_channels];
    int color_index;
    int output_index;
    int palette_value;
    float palette_value_float;
    float error;
    int n;
    float *data;
    unsigned char *palette;
    int float_index;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    data = context->pixels_float;
    if (data == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->new_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->migration_map == NULL || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = context->palette;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    if (context->depth > max_channels || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->reqcolor < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);

    if (context->optimize_palette) {
        *context->ncolors = 0;
        memset(context->new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(context->migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    } else {
        *context->ncolors = context->reqcolor;
    }

    for (y = 0; y < context->height; ++y) {
        sixel_dither_scanline_params(serpentine, y, context->width,
                                     &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)context->depth;
            source_pixel = data + base;

            for (n = 0; n < context->depth; ++n) {
                snapshot[n] = source_pixel[n];
                quantized[n]
                    = sixel_dither_float_channel_to_byte(source_pixel[n]);
            }

            color_index = context->lookup(quantized,
                                          context->depth,
                                          palette,
                                          context->reqcolor,
                                          context->indextable,
                                          context->complexion);

            if (context->optimize_palette) {
                if (context->migration_map[color_index] == 0) {
                    output_index = *context->ncolors;
                    for (n = 0; n < context->depth; ++n) {
                        context->new_palette[output_index * context->depth + n]
                            = palette[color_index * context->depth + n];
                    }
                    if (palette_float != NULL
                            && new_palette_float != NULL
                            && float_depth > 0) {
                        for (float_index = 0;
                                float_index < float_depth;
                                ++float_index) {
                            new_palette_float[output_index * float_depth
                                              + float_index]
                                = palette_float[color_index * float_depth
                                                + float_index];
                        }
                    }
                    ++*context->ncolors;
                    context->migration_map[color_index] = *context->ncolors;
                } else {
                    output_index = context->migration_map[color_index] - 1;
                }
                context->result[pos] = output_index;
            } else {
                output_index = color_index;
                context->result[pos] = output_index;
            }

            for (n = 0; n < context->depth; ++n) {
                if (context->optimize_palette) {
                    palette_value
                        = context->new_palette[output_index
                                               * context->depth + n];
                } else {
                    palette_value
                        = palette[color_index * context->depth + n];
                }
                palette_value_float
                    = sixel_dither_byte_to_float((unsigned char)palette_value);
                error = snapshot[n] - palette_value_float;
                source_pixel[n] = palette_value_float;
                diffuse_fs_float(data + (size_t)n,
                                 context->width,
                                 context->height,
                                 x,
                                 y,
                                 context->depth,
                                 error,
                                 direction);
            }
        }
        sixel_dither_pipeline_row_notify(dither, y);
    }

    if (context->optimize_palette) {
        memcpy(context->palette,
               context->new_palette,
               (size_t)(*context->ncolors * context->depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*context->ncolors * float_depth)
                       * sizeof(float));
        }
    }

    status = SIXEL_OK;
    SIXEL_DITHER_FLOAT32_HIT();
    return status;
}
