/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "dither-diffusion-adaptive.h"
#include "dither-common-pipeline.h"

/*
 * Local helper describing serpentine scan conversion.  Adaptive diffusion
 * adjusts its error propagation dynamically, so keeping the traversal logic
 * here documents the row order without exposing another shared header.
 */
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

static const int (*
lso2_table(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

typedef void (*diffuse_varerr_mode)(unsigned char *data,
                                    int width,
                                    int height,
                                    int x,
                                    int y,
                                    int depth,
                                    int32_t error,
                                    int index,
                                    int direction);

typedef void (*diffuse_varerr_carry_mode)(int32_t *carry_curr,
                                          int32_t *carry_next,
                                          int32_t *carry_far,
                                          int width,
                                          int height,
                                          int depth,
                                          int x,
                                          int y,
                                          int32_t error,
                                          int index,
                                          int direction,
                                          int channel);


static int32_t
diffuse_varerr_term(int32_t error, int weight, int denom)
{
    int64_t delta;

    delta = (int64_t)error * (int64_t)weight;
    if (delta >= 0) {
        delta = (delta + denom / 2) / denom;
    } else {
        delta = (delta - denom / 2) / denom;
    }

    return (int32_t)delta;
}

static void
diffuse_varerr_apply_direct(unsigned char *target, int depth, size_t offset,
                            int32_t delta)
{
    int64_t value;
    int result;

    value = (int64_t)target[offset * depth] << VARERR_SCALE_SHIFT;
    value += delta;
    if (value < 0) {
        value = 0;
    } else {
        int64_t max_value;

        max_value = VARERR_MAX_VALUE;
        if (value > max_value) {
            value = max_value;
        }
    }

    result = (int)((value + VARERR_ROUND) >> VARERR_SCALE_SHIFT);
    if (result < 0) {
        result = 0;
    }
    if (result > 255) {
        result = 255;
    }
    target[offset * depth] = (unsigned char)result;
}

static void
diffuse_lso2(unsigned char *data, int width, int height,
             int x, int y, int depth, int32_t error,
             int index, int direction)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    int32_t term_r = 0;
    int32_t term_r2 = 0;
    int32_t term_dl = 0;
    int32_t term_d = 0;
    int32_t term_dr = 0;
    int32_t term_d2 = 0;
    size_t offset;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table();
    entry = table[index];
    denom = entry[6];
    if (denom == 0) {
        return;
    }

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = diffuse_varerr_term(error, entry[5], denom);


    if (direction >= 0) {
        if (x + 1 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x + 2 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    } else {
        if (x - 1 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x - 2 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    }
}

static void
diffuse_lso2_carry(int32_t *carry_curr, int32_t *carry_next, int32_t *carry_far,
                   int width, int height, int depth,
                   int x, int y, int32_t error,
                   int index, int direction, int channel)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    int32_t term_r = 0;
    int32_t term_r2 = 0;
    int32_t term_dl = 0;
    int32_t term_d = 0;
    int32_t term_dr = 0;
    int32_t term_d2 = 0;
    size_t base;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table();
    entry = table[index];
    denom = entry[6];
    if (denom == 0) {
        return;
    }

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = error - term_r - term_r2 - term_dl - term_d - term_dr;

    if (direction >= 0) {
        if (x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x + 2 < width) {
            base = ((size_t)(x + 2) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    } else {
        if (x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x - 2 >= 0) {
            base = ((size_t)(x - 2) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    }
}

SIXELSTATUS
sixel_dither_apply_variable(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int method_for_scan,
    int optimize_palette,
    int (*f_lookup)(const unsigned char *pixel,
                    int depth,
                    const unsigned char *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion),
    unsigned short *indextable,
    int complexion,
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors,
    int method_for_diffuse,
    int method_for_carry,
    sixel_dither_t *dither)
{
    SIXELSTATUS status = SIXEL_FALSE;
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    int serpentine;
    int y;
    diffuse_varerr_mode varerr_diffuse;
    diffuse_varerr_carry_mode varerr_diffuse_carry;
    int use_carry;
    size_t carry_len;
    int32_t *carry_curr = NULL;
    int32_t *carry_next = NULL;
    int32_t *carry_far = NULL;
    unsigned char corrected[max_channels];
    int32_t sample_scaled[max_channels];
    int32_t accum_scaled[max_channels];
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    size_t carry_base;
    const unsigned char *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int diff;
    int table_index;
    int64_t accum;
    int64_t clamped;
    int32_t target_scaled;
    int32_t error_scaled;
    int32_t *tmp;

    if (depth > max_channels) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    use_carry = (method_for_carry == SIXEL_CARRY_ENABLE);
    carry_len = 0;

    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_LSO2:
        varerr_diffuse = diffuse_lso2;
        varerr_diffuse_carry = diffuse_lso2_carry;
        break;
    default:
        varerr_diffuse = diffuse_lso2;
        varerr_diffuse_carry = diffuse_lso2_carry;
        break;
    }

    if (use_carry) {
        carry_len = (size_t)width * (size_t)depth;
        carry_curr = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_curr == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        carry_next = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_next == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        carry_far = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_far == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);

    if (optimize_palette) {
        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    }

    for (y = 0; y < height; ++y) {
        sixel_dither_scanline_params(serpentine, y, width,
                        &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            carry_base = (size_t)x * (size_t)depth;
            if (use_carry) {
                for (n = 0; n < depth; ++n) {
                    accum = ((int64_t)data[base + n]
                             << VARERR_SCALE_SHIFT)
                          + carry_curr[carry_base + (size_t)n];
                    if (accum < INT32_MIN) {
                        accum = INT32_MIN;
                    } else if (accum > INT32_MAX) {
                        accum = INT32_MAX;
                    }
                    carry_curr[carry_base + (size_t)n] = 0;
                    clamped = accum;
                    if (clamped < 0) {
                        clamped = 0;
                    } else if (clamped > VARERR_MAX_VALUE) {
                        clamped = VARERR_MAX_VALUE;
                    }
                    accum_scaled[n] = (int32_t)clamped;
                    corrected[n]
                        = (unsigned char)((clamped + VARERR_ROUND)
                                          >> VARERR_SCALE_SHIFT);
                }
                source_pixel = corrected;
            } else {
                for (n = 0; n < depth; ++n) {
                    sample_scaled[n]
                        = (int32_t)data[base + n]
                        << VARERR_SCALE_SHIFT;
                    corrected[n] = data[base + n];
                }
                source_pixel = data + base;
            }

            color_index = f_lookup(source_pixel, depth, palette,
                                   reqcolor, indextable,
                                   complexion);

            if (optimize_palette) {
                if (migration_map[color_index] == 0) {
                    output_index = *ncolors;
                    for (n = 0; n < depth; ++n) {
                        new_palette[output_index * depth + n]
                            = palette[color_index * depth + n];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    output_index = migration_map[color_index] - 1;
                }
                result[pos] = output_index;
            } else {
                output_index = color_index;
                result[pos] = output_index;
            }

            for (n = 0; n < depth; ++n) {
                if (optimize_palette) {
                    palette_value = new_palette[output_index * depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                }
                diff = (int)source_pixel[n] - palette_value;
                if (diff < 0) {
                    diff = -diff;
                }
                if (diff > 255) {
                    diff = 255;
                }
                table_index = diff;
                if (use_carry) {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = accum_scaled[n] - target_scaled;
                    varerr_diffuse_carry(carry_curr, carry_next, carry_far,
                                         width, height, depth,
                                         x, y, error_scaled,
                                         table_index,
                                         direction, n);
                } else {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = sample_scaled[n] - target_scaled;
                    varerr_diffuse(data + n, width, height,
                                   x, y, depth, error_scaled,
                                   table_index,
                                   direction);
                }
            }
        }
        if (use_carry) {
            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(int32_t));
            }
        }
        sixel_dither_pipeline_row_notify(dither, y);
    }

    if (optimize_palette) {
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
    } else {
        *ncolors = reqcolor;
    }

    status = SIXEL_OK;

end:
    free(carry_next);
    free(carry_curr);
    free(carry_far);
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
