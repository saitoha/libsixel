/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Target color construction.
 *
 * The forward and inverse transforms below are the ones the palette owner
 * uses for serialization and for interpreting emitted registers
 * (sixel_palette_transform_to_output and sixel_palette_from_output in
 * palette.c). Keep both files on the same arithmetic: the target must
 * describe the color the receiver reproduces for the encoder's own codes.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "target-color.h"
#include "colorspace.h"
#include "named-color.h"

/* The same numerical membership slack as the gamut mapper: float storage
 * and matrix pairs can straddle a code boundary by a few ulps. */
#define SIXEL_TARGET_COLOR_CODE_EPSILON 2e-7


struct sixel_target_color {
    sixel_allocator_t *allocator;
    sixel_color_def_t *srgb;      /* NULL for builtin destinations */
    sixel_color_def_t *device;    /* NULL for builtin destinations */
    int builtin_space;
    sixel_gamut_t *gamut;         /* NULL selects wire-code clipping */
    sixel_target_color_options_t options;
};

typedef struct sixel_target_color_lut {
    int n;
    double lo[3];
    double step[3];
    float *delta;                 /* n^3 target-minus-node triplets */
    unsigned char *node_ok;       /* node evaluated and inside -U */
    unsigned char *cell_ok;       /* (n-1)^3 cells safe to interpolate */
} sixel_target_color_lut_t;

static int
sixel_target_color_finite(double const value[3])
{
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static int
sixel_target_color_encode(sixel_target_color_t const *target,
                          double const input[3], double codes[3],
                          int colorspace)
{
    double gamma[3];
    double xyz[3];

    if (target->device == NULL) {
        return sixel_color_convert_output(codes, input, colorspace,
                                          target->builtin_space);
    }
    return sixel_color_convert(gamma, input, colorspace,
                               SIXEL_COLORSPACE_SRGB) &&
        sixel_cdef_to_xyz(target->srgb, gamma, xyz,
                          SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC) &&
        sixel_cdef_from_xyz(target->device, xyz, codes,
                            SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC);
}

static int
sixel_target_color_interpret(sixel_target_color_t const *target,
                             double const codes[3], double output[3],
                             int colorspace)
{
    double gamma[3];
    double xyz[3];

    if (target->device == NULL) {
        return sixel_color_from_output(output, codes,
                                       target->builtin_space, colorspace);
    }
    return sixel_cdef_to_xyz(target->device, codes, xyz,
                             SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC) &&
        sixel_cdef_from_xyz(target->srgb, xyz, gamma,
                            SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC) &&
        sixel_color_convert(output, gamma, SIXEL_COLORSPACE_SRGB,
                            colorspace);
}

static int
sixel_target_color_inside(double const codes[3])
{
    int i;

    for (i = 0; i < 3; ++i) {
        if (!isfinite(codes[i]) ||
                codes[i] < -SIXEL_TARGET_COLOR_CODE_EPSILON ||
                codes[i] > 1.0 + SIXEL_TARGET_COLOR_CODE_EPSILON) {
            return 0;
        }
    }
    return 1;
}

/* Evaluate the aim of one triplet. INSIDE reports gamut membership of the
 * input. */
static int
sixel_target_color_eval(sixel_target_color_t const *target,
                        double const input[3], double output[3],
                        int colorspace, int *inside)
{
    double codes[3];
    double mapped[3];
    double result[3];
    int member;
    int i;

    if (!sixel_target_color_finite(input) ||
            !sixel_target_color_encode(target, input, codes, colorspace)) {
        return 0;
    }
    member = sixel_target_color_inside(codes);
    if (inside != NULL) {
        *inside = member;
    }
    if (member) {
        /* A representable color is its own aim, bit for bit. */
        memcpy(output, input, sizeof(result));
        return 1;
    }
    if (!member && target->gamut != NULL) {
        if (!sixel_gamut_apply(target->gamut, input, mapped, colorspace) ||
                !sixel_target_color_encode(target, mapped, codes,
                                           colorspace)) {
            return 0;
        }
    }
    for (i = 0; i < 3; ++i) {
        if (!isfinite(codes[i])) {
            return 0;
        }
        /* The wire carries only [0,1] codes; quantization is excluded. */
        codes[i] = fmin(1.0, fmax(0.0, codes[i]));
    }
    if (!sixel_target_color_interpret(target, codes, result, colorspace) ||
            !sixel_target_color_finite(result)) {
        return 0;
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

static int
sixel_target_color_delta_e(double const left[3], double const right[3],
                           int colorspace, double *delta)
{
    double a[3];
    double b[3];

    if (!sixel_color_convert(a, left, colorspace, SIXEL_COLORSPACE_OKLAB) ||
            !sixel_color_convert(b, right, colorspace,
                                 SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    *delta = sqrt((a[0] - b[0]) * (a[0] - b[0]) +
                  (a[1] - b[1]) * (a[1] - b[1]) +
                  (a[2] - b[2]) * (a[2] - b[2]));
    return isfinite(*delta);
}

SIXELSTATUS
sixel_target_color_new(sixel_target_color_t **out,
                       sixel_target_color_options_t const *options,
                       int output_definition,
                       int output_colorspace,
                       sixel_color_def_t const *output_icc,
                       sixel_gamut_t const *gamut,
                       sixel_allocator_t *allocator)
{
    sixel_target_color_t *target;
    SIXELSTATUS status;
    char const *name;

    if (out == NULL || options == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    if (options->aim < SIXEL_TARGET_COLOR_AIM_SOURCE ||
            options->aim > SIXEL_TARGET_COLOR_AIM_GAMUT ||
            options->cluster < SIXEL_TARGET_COLOR_CLUSTER_SOURCE ||
            options->cluster > SIXEL_TARGET_COLOR_CLUSTER_GAMUT ||
            options->method < SIXEL_TARGET_COLOR_METHOD_EXACT ||
            options->method > SIXEL_TARGET_COLOR_METHOD_AUTO ||
            options->lut_size < SIXEL_TARGET_COLOR_LUT_SIZE_MIN ||
            options->lut_size > SIXEL_TARGET_COLOR_LUT_SIZE_MAX ||
            !(options->tolerance >= 0.0)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (output_icc == NULL && output_definition == SIXEL_NAMED_COLOR_SRGB &&
            output_colorspace != SIXEL_COLORSPACE_SRGB &&
            output_colorspace != SIXEL_COLORSPACE_SRGB_LINEAR &&
            output_colorspace != SIXEL_COLORSPACE_SMPTEC) {
        return SIXEL_BAD_ARGUMENT;
    }
    target = sixel_allocator_calloc(allocator, 1u, sizeof(*target));
    if (target == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    target->allocator = allocator;
    sixel_allocator_ref(allocator);
    target->options = *options;
    target->builtin_space = output_colorspace;
    status = SIXEL_BAD_ALLOCATION;
    if (output_icc != NULL || output_definition != SIXEL_NAMED_COLOR_SRGB) {
        /* Named and ICC destinations use the device definition exactly as
         * the palette owner does; legacy sRGB keeps its numeric kernels. */
        name = output_icc == NULL ? sixel_cdef_named_name(output_definition)
                                  : NULL;
        if (output_icc == NULL && name == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto fail;
        }
        target->srgb = sixel_cdef_name("srgb");
        target->device = output_icc != NULL ? sixel_cdef_clone(output_icc)
                                            : sixel_cdef_name(name);
        if (target->srgb == NULL || target->device == NULL) {
            goto fail;
        }
    }
    if (gamut != NULL) {
        status = sixel_gamut_clone(gamut, &target->gamut);
        if (SIXEL_FAILED(status)) {
            goto fail;
        }
    }
    *out = target;
    return SIXEL_OK;

fail:
    sixel_target_color_free(target);
    return status;
}

SIXELSTATUS
sixel_target_color_clone(sixel_target_color_t const *source,
                         sixel_target_color_t **out)
{
    sixel_target_color_t *copy;
    SIXELSTATUS status;

    if (source == NULL || out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    copy = sixel_allocator_calloc(source->allocator, 1u, sizeof(*copy));
    if (copy == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    *copy = *source;
    copy->srgb = NULL;
    copy->device = NULL;
    copy->gamut = NULL;
    sixel_allocator_ref(copy->allocator);
    status = SIXEL_BAD_ALLOCATION;
    if (source->device != NULL) {
        copy->srgb = sixel_cdef_clone(source->srgb);
        copy->device = sixel_cdef_clone(source->device);
        if (copy->srgb == NULL || copy->device == NULL) {
            goto fail;
        }
    }
    if (source->gamut != NULL) {
        status = sixel_gamut_clone(source->gamut, &copy->gamut);
        if (SIXEL_FAILED(status)) {
            goto fail;
        }
    }
    *out = copy;
    return SIXEL_OK;

fail:
    sixel_target_color_free(copy);
    return status;
}

void
sixel_target_color_free(sixel_target_color_t *target)
{
    sixel_allocator_t *allocator;

    if (target == NULL) {
        return;
    }
    allocator = target->allocator;
    sixel_cdef_free(target->srgb);
    sixel_cdef_free(target->device);
    sixel_gamut_free(target->gamut);
    sixel_allocator_free(allocator, target);
    sixel_allocator_unref(allocator);
}

void
sixel_target_color_get_axes(sixel_target_color_t const *target,
                            sixel_target_color_axes_t *axes)
{
    if (axes == NULL) {
        return;
    }
    if (target == NULL) {
        axes->aim = SIXEL_TARGET_COLOR_AIM_SOURCE;
        axes->cluster = SIXEL_TARGET_COLOR_CLUSTER_SOURCE;
        return;
    }
    axes->aim = target->options.aim;
    axes->cluster = target->options.cluster;
}

void
sixel_target_color_get_options(sixel_target_color_t const *target,
                               sixel_target_color_options_t *options)
{
    if (target != NULL && options != NULL) {
        *options = target->options;
    }
}

int
sixel_target_color_is_identity(sixel_target_color_t const *target,
                               int colorspace)
{
    return target != NULL && target->device == NULL &&
        target->builtin_space == colorspace;
}

static size_t
sixel_target_color_node(sixel_target_color_lut_t const *lut,
                        int r, int g, int b)
{
    return ((size_t)r * (size_t)lut->n + (size_t)g) * (size_t)lut->n
        + (size_t)b;
}

static size_t
sixel_target_color_cell(sixel_target_color_lut_t const *lut,
                        int r, int g, int b)
{
    size_t cells;

    cells = (size_t)lut->n - 1u;
    return ((size_t)r * cells + (size_t)g) * cells + (size_t)b;
}

static void
sixel_target_color_lut_dispose(sixel_target_color_t const *target,
                               sixel_target_color_lut_t *lut)
{
    sixel_allocator_free(target->allocator, lut->delta);
    sixel_allocator_free(target->allocator, lut->node_ok);
    sixel_allocator_free(target->allocator, lut->cell_ok);
    memset(lut, 0, sizeof(*lut));
}

/*
 * Build a LUT over the bounding box of the visible source. A cell is used
 * only when its eight corners and its center lie inside the -U gamut and
 * the trilinear aim at the center agrees with the exact aim within the
 * tolerance. Every other cell, and therefore every gamut boundary, falls
 * back to exact evaluation. A color inside the gamut is its own aim, so
 * every usable node has a zero offset and the LUT is a verified membership
 * cache that saves the per-pixel encode.
 */
static SIXELSTATUS
sixel_target_color_lut_build(sixel_target_color_t const *target,
                             float const *pixels, size_t count,
                             int colorspace, unsigned char const *hidden,
                             size_t hidden_count,
                             sixel_target_color_lut_t *lut)
{
    double hi[3];
    double node[3];
    double result[3];
    double interpolated[3];
    double delta;
    size_t nodes;
    size_t cells;
    size_t i;
    size_t index;
    int visible;
    int inside;
    int n;
    int r;
    int g;
    int b;
    int c;
    int corner;
    int ok;

    memset(lut, 0, sizeof(*lut));
    memset(hi, 0, sizeof(hi));
    n = target->options.lut_size;
    visible = 0;
    for (i = 0; i < count; ++i) {
        if (hidden != NULL && i < hidden_count && hidden[i] != 0) {
            continue;
        }
        for (c = 0; c < 3; ++c) {
            node[c] = pixels[i * 3u + (size_t)c];
            if (!isfinite(node[c])) {
                return SIXEL_BAD_INPUT;
            }
            if (!visible || node[c] < lut->lo[c]) {
                lut->lo[c] = node[c];
            }
            if (!visible || node[c] > hi[c]) {
                hi[c] = node[c];
            }
        }
        visible = 1;
    }
    if (!visible) {
        return SIXEL_OK;
    }
    lut->n = n;
    for (c = 0; c < 3; ++c) {
        lut->step[c] = (hi[c] - lut->lo[c]) / (double)(n - 1);
    }
    nodes = (size_t)n * (size_t)n * (size_t)n;
    cells = (size_t)(n - 1) * (size_t)(n - 1) * (size_t)(n - 1);
    lut->delta = sixel_allocator_malloc(target->allocator,
                                        nodes * 3u * sizeof(float));
    lut->node_ok = sixel_allocator_calloc(target->allocator, nodes, 1u);
    lut->cell_ok = sixel_allocator_calloc(target->allocator, cells, 1u);
    if (lut->delta == NULL || lut->node_ok == NULL || lut->cell_ok == NULL) {
        sixel_target_color_lut_dispose(target, lut);
        return SIXEL_BAD_ALLOCATION;
    }
    for (r = 0; r < n; ++r) {
        for (g = 0; g < n; ++g) {
            for (b = 0; b < n; ++b) {
                index = sixel_target_color_node(lut, r, g, b);
                node[0] = lut->lo[0] + lut->step[0] * r;
                node[1] = lut->lo[1] + lut->step[1] * g;
                node[2] = lut->lo[2] + lut->step[2] * b;
                inside = 0;
                ok = sixel_target_color_eval(target, node, result,
                                             colorspace, &inside);
                /* A box corner may be a nonphysical color. It only
                 * disables interpolation for the cells that touch it. */
                lut->node_ok[index] = (unsigned char)(ok && inside);
                for (c = 0; c < 3; ++c) {
                    lut->delta[index * 3u + (size_t)c] = ok
                        ? (float)(result[c] - node[c]) : 0.0f;
                }
            }
        }
    }
    for (r = 0; r < n - 1; ++r) {
        for (g = 0; g < n - 1; ++g) {
            for (b = 0; b < n - 1; ++b) {
                ok = 1;
                interpolated[0] = interpolated[1] = interpolated[2] = 0.0;
                for (corner = 0; ok && corner < 8; ++corner) {
                    index = sixel_target_color_node(lut,
                        r + ((corner >> 2) & 1), g + ((corner >> 1) & 1),
                        b + (corner & 1));
                    ok = lut->node_ok[index];
                    for (c = 0; c < 3; ++c) {
                        interpolated[c] +=
                            lut->delta[index * 3u + (size_t)c] / 8.0;
                    }
                }
                if (!ok) {
                    continue;
                }
                node[0] = lut->lo[0] + lut->step[0] * (r + 0.5);
                node[1] = lut->lo[1] + lut->step[1] * (g + 0.5);
                node[2] = lut->lo[2] + lut->step[2] * (b + 0.5);
                inside = 0;
                if (!sixel_target_color_eval(target, node, result,
                                             colorspace, &inside) ||
                        !inside) {
                    continue;
                }
                for (c = 0; c < 3; ++c) {
                    interpolated[c] += node[c];
                }
                if (sixel_target_color_delta_e(result, interpolated,
                        colorspace, &delta) &&
                        delta <= target->options.tolerance) {
                    lut->cell_ok[sixel_target_color_cell(lut, r, g, b)] = 1;
                }
            }
        }
    }
    return SIXEL_OK;
}

/* Interpolate the offset for SOURCE. Return 0 when the exact path owns
 * this pixel. */
static int
sixel_target_color_lut_lookup(sixel_target_color_lut_t const *lut,
                              double const source[3], double offset[3])
{
    double position;
    double f[3];
    double weight;
    int cell[3];
    int last;
    int corner;
    int c;
    size_t index;

    last = lut->n - 2;
    for (c = 0; c < 3; ++c) {
        if (lut->step[c] > 0.0) {
            position = (source[c] - lut->lo[c]) / lut->step[c];
        } else {
            position = 0.0;
        }
        if (!(position >= 0.0) || position > (double)(lut->n - 1)) {
            return 0;
        }
        cell[c] = (int)position;
        if (cell[c] > last) {
            cell[c] = last;
        }
        f[c] = position - cell[c];
    }
    if (!lut->cell_ok[sixel_target_color_cell(lut, cell[0], cell[1],
                                               cell[2])]) {
        return 0;
    }
    offset[0] = offset[1] = offset[2] = 0.0;
    for (corner = 0; corner < 8; ++corner) {
        weight = (((corner >> 2) & 1) ? f[0] : 1.0 - f[0]) *
            (((corner >> 1) & 1) ? f[1] : 1.0 - f[1]) *
            ((corner & 1) ? f[2] : 1.0 - f[2]);
        index = sixel_target_color_node(lut,
            cell[0] + ((corner >> 2) & 1), cell[1] + ((corner >> 1) & 1),
            cell[2] + (corner & 1));
        for (c = 0; c < 3; ++c) {
            offset[c] += weight * lut->delta[index * 3u + (size_t)c];
        }
    }
    return 1;
}

static SIXELSTATUS
sixel_target_color_run(sixel_target_color_t *target,
                       float const *pixels, size_t count, int colorspace,
                       unsigned char const *hidden, size_t hidden_count,
                       float *output, int write_offset, int *nonzero)
{
    sixel_target_color_lut_t lut;
    SIXELSTATUS status;
    double source[3];
    double result[3];
    double offset[3];
    size_t nodes;
    size_t i;
    int use_lut;
    int c;

    if (target == NULL || (count != 0u && (pixels == NULL ||
            output == NULL))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (nonzero != NULL) {
        *nonzero = 0;
    }
    memset(&lut, 0, sizeof(lut));
    nodes = (size_t)target->options.lut_size *
        (size_t)target->options.lut_size * (size_t)target->options.lut_size;
    /* The aim still pays one encode per pixel to test membership. */
    use_lut = target->options.method == SIXEL_TARGET_COLOR_METHOD_LUT ||
        (target->options.method == SIXEL_TARGET_COLOR_METHOD_AUTO &&
         count / 4u > nodes);
    if (use_lut) {
        status = sixel_target_color_lut_build(target, pixels,
            count, colorspace, hidden, hidden_count, &lut);
        if (SIXEL_FAILED(status)) {
            if (status == SIXEL_BAD_INPUT) {
                sixel_helper_set_additional_message(
                    "gamut aim: nonfinite source sample.");
            }
            return status;
        }
    }
    status = SIXEL_OK;
    for (i = 0; i < count; ++i) {
        if (hidden != NULL && i < hidden_count && hidden[i] != 0) {
            if (write_offset) {
                for (c = 0; c < 3; ++c) {
                    output[i * 3u + (size_t)c] = 0.0f;
                }
            }
            continue;
        }
        for (c = 0; c < 3; ++c) {
            source[c] = pixels[i * 3u + (size_t)c];
        }
        if (lut.n != 0 &&
                sixel_target_color_lut_lookup(&lut, source, offset)) {
            for (c = 0; c < 3; ++c) {
                result[c] = source[c] + offset[c];
            }
        } else if (!sixel_target_color_eval(target, source, result,
                                            colorspace, NULL)) {
            sixel_helper_set_additional_message(
                "gamut aim: cannot evaluate the -U aim "
                "for a source color.");
            status = SIXEL_BAD_INPUT;
            break;
        }
        for (c = 0; c < 3; ++c) {
            if (!isfinite(result[c]) || fabs(result[c]) > FLT_MAX) {
                status = SIXEL_BAD_INPUT;
                break;
            }
        }
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "gamut aim: aim leaves the float32 range.");
            break;
        }
        for (c = 0; c < 3; ++c) {
            if (write_offset) {
                output[i * 3u + (size_t)c] =
                    fabs(result[c] - source[c]) <
                        SIXEL_TARGET_COLOR_OFFSET_EPSILON
                    ? 0.0f : (float)(result[c] - source[c]);
                if (nonzero != NULL &&
                        output[i * 3u + (size_t)c] != 0.0f) {
                    *nonzero = 1;
                }
            } else {
                output[i * 3u + (size_t)c] = (float)result[c];
            }
        }
    }
    sixel_target_color_lut_dispose(target, &lut);
    return status;
}

SIXELSTATUS
sixel_target_color_offset_rgbf32(sixel_target_color_t *target,
                                 float const *pixels, size_t count,
                                 int colorspace,
                                 unsigned char const *hidden,
                                 size_t hidden_count,
                                 float *offset, int *nonzero)
{
    return sixel_target_color_run(target, pixels, count, colorspace,
                                  hidden, hidden_count, offset, 1, nonzero);
}

SIXELSTATUS
sixel_target_color_map_rgbf32(sixel_target_color_t *target,
                              float *pixels, size_t count, int colorspace,
                              unsigned char const *hidden,
                              size_t hidden_count)
{
    /* Each triplet is read before its own slot is written, and the LUT
     * bounds are fixed before the first write, so aliasing is safe. */
    return sixel_target_color_run(target, pixels, count, colorspace,
                                  hidden, hidden_count, pixels, 0, NULL);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
