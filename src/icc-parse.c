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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "icc-parse.h"
#include "loader-builtin.h"
#include "loader-common.h"

#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include <stdlib.h>
#include <string.h>

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define SIXEL_ICC_PARAMETRIC_TABLE_SIZE 4096u
#define SIXEL_ICC_MAX_TAGS 256u
#define SIXEL_ICC_MAX_MPE_ELEMENTS 256u
#define SIXEL_ICC_MAX_PARSE_BYTES ((size_t)16u * 1024u * 1024u)

typedef struct sixel_icc_parse_budget {
    size_t remaining; /* Cumulative parse limit, including scratch. */
    size_t retained;  /* Payload bytes owned by accepted paths. */
    char const *limit;
} sixel_icc_parse_budget_t;

typedef struct sixel_icc_mpet_cache {
    struct {
        size_t offset;
        size_t length;
        sixel_icc_mpet_t *owner;
    } tags[2 * SIXEL_ICC_MPET_SLOT_COUNT];
    size_t count;
} sixel_icc_mpet_cache_t;

static void
sixel_icc_record_issue(sixel_icc_issue_t *issue, sixel_icc_issue_t value)
{
    if (value > *issue) {
        *issue = value;
    }
}

/* Charge all allocations made while parsing a profile, even tables that
 * are later discarded and separate copies of a shared on-disk payload. */
static int
sixel_icc_parse_fits(sixel_icc_parse_budget_t *budget,
                     size_t count, size_t element_size,
                     sixel_icc_issue_t *issue)
{
    if (budget->limit != NULL || count > budget->remaining / element_size) {
        if (budget->limit == NULL) {
            budget->limit = "parse-bytes";
        }
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_RESOURCE_LIMIT);
        return 0;
    }
    return 1;
}

static void *
sixel_icc_parse_scratch(sixel_icc_parse_budget_t *budget, size_t size,
                        sixel_icc_issue_t *issue)
{
    void *data;

    if (!sixel_icc_parse_fits(budget, size, 1u, issue)) {
        return NULL;
    }
    data = malloc(size);
    if (data == NULL) {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_ALLOCATION);
    } else {
        budget->remaining -= size;
    }
    return data;
}

/* Payloads use the same cumulative limit as scratch. If an optional
 * path is discarded, its caller restores only the retained byte count. */
static void *
sixel_icc_parse_alloc(sixel_icc_parse_budget_t *budget, size_t size,
                      sixel_icc_issue_t *issue)
{
    void *data;

    data = sixel_icc_parse_scratch(budget, size, issue);
    if (data != NULL) {
        budget->retained += size;
    }
    return data;
}

static uint16_t
sixel_icc_read_be16(unsigned char const *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | (uint16_t)p[1]);
}

static uint32_t
sixel_icc_read_be32(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24u)
        | ((uint32_t)p[1] << 16u)
        | ((uint32_t)p[2] << 8u)
        | (uint32_t)p[3];
}

static int
sixel_icc_is_mpet_slot(void const *signature)
{
    return memcmp(signature, "D2B", 3u) == 0
        || memcmp(signature, "B2D", 3u) == 0;
}

/*
 * Audit only when requested. Signatures are hex so untrusted profile bytes
 * cannot inject terminal controls or additional log records. This describes
 * parser capabilities, not whether a transform was ultimately applied.
 */
static void
sixel_icc_trace_tags(unsigned char const *data, size_t size)
{
    static char const consumed[][5] = {
        "rXYZ", "gXYZ", "bXYZ", "rTRC", "gTRC", "bTRC", "kTRC", "wtpt"
    };
    static char const descriptive[][5] = {
        "desc", "cprt", "dmnd", "dmdd"
    };
    unsigned char const *entry;
    unsigned char const *payload;
    char const *reason;
    size_t count;
    size_t index;
    size_t offset;
    size_t length;
    size_t item;
    unsigned int emitted;
    int known;
    int pipeline;

    if (!loader_trace_is_enabled()) {
        return;
    }
    count = (size_t)sixel_icc_read_be32(data + 128u);
    if (count > (size - 132u) / 12u) {
        loader_trace_message("LSXCMS1|engine=builtin|event=invalid-table");
        return;
    }
    emitted = 0u;
    for (index = 0u; index < count; ++index) {
        entry = data + 132u + index * 12u;
        offset = (size_t)sixel_icc_read_be32(entry + 4u);
        length = (size_t)sixel_icc_read_be32(entry + 8u);
        reason = NULL;
        payload = NULL;
        pipeline = memcmp(entry, "A2B", 3u) == 0
            || memcmp(entry, "B2A", 3u) == 0
            || sixel_icc_is_mpet_slot(entry);
        known = 0;
        for (item = 0u; item < sizeof(consumed) / sizeof(consumed[0]);
             ++item) {
            known |= memcmp(entry, consumed[item], 4u) == 0;
        }
        for (item = 0u;
             item < sizeof(descriptive) / sizeof(descriptive[0]); ++item) {
            known |= memcmp(entry, descriptive[item], 4u) == 0;
        }
        if (offset > size || length > size - offset || length < 4u) {
            reason = "invalid-tag-bounds";
        } else {
            payload = data + offset;
            if (pipeline && (entry[3] < '0' || entry[3] >
                             (sixel_icc_is_mpet_slot(entry) ? '3' : '2'))) {
                reason = "unsupported-intent-slot";
            } else if (pipeline
                       && !(sixel_icc_is_mpet_slot(entry) &&
                            memcmp(payload, "mpet", 4u) == 0)
                       && memcmp(payload, "mAB ", 4u) != 0
                       && memcmp(payload, "mBA ", 4u) != 0
                       && memcmp(payload, "mft1", 4u) != 0
                       && memcmp(payload, "mft2", 4u) != 0) {
                reason = "unsupported-pipeline-type";
            } else if (!pipeline && !known) {
                reason = "tag-not-consumed";
            }
        }
        if (reason == NULL) {
            continue;
        }
        if (emitted == 64u) {
            loader_trace_message(
                "LSXCMS1|engine=builtin|event=tag-audit-truncated|limit=64");
            break;
        }
        loader_trace_message(
            "LSXCMS1|engine=builtin|event=ignored-tag|tag=%08lx"
            "|type=%08lx|reason=%s",
            (unsigned long)sixel_icc_read_be32(entry),
            payload != NULL
                ? (unsigned long)sixel_icc_read_be32(payload) : 0ul,
            reason);
        ++emitted;
    }
}

static double
sixel_icc_read_s15fixed16(unsigned char const *p)
{
    int32_t fixed;

    fixed = (int32_t)sixel_icc_read_be32(p);
    return (double)fixed / 65536.0;
}

/* The caller has already checked the 36-byte row-major matrix block. */
static void
sixel_icc_read_matrix(double matrix[3][3], unsigned char const *data)
{
    size_t row;
    size_t column;

    for (row = 0u; row < 3u; ++row) {
        for (column = 0u; column < 3u; ++column) {
            matrix[row][column] = sixel_icc_read_s15fixed16(data);
            data += 4u;
        }
    }
}

static void
sixel_icc_curve_reset(sixel_icc_curve_t *curve)
{
    if (curve == NULL) {
        return;
    }

    curve->kind = SIXEL_ICC_CURVE_INVALID;
    curve->gamma = 1.0;
    curve->table = NULL;
    curve->table_length = 0u;
    curve->param_count = 0u;
    curve->function_type = 0u;
    memset(curve->params, 0, sizeof(curve->params));
}

static void
sixel_icc_curve_destroy(sixel_icc_curve_t *curve)
{
    if (curve == NULL) {
        return;
    }

    if (curve->table != NULL) {
        free(curve->table);
    }
    sixel_icc_curve_reset(curve);
}

static void
sixel_icc_lut_reset(sixel_icc_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    lut->kind = SIXEL_ICC_LUT_INVALID;
    lut->input_channels = 0u;
    lut->output_channels = 0u;
    lut->clut_grid_points = 0u;
    lut->input_entries = 0u;
    lut->output_entries = 0u;
    memset(lut->matrix, 0, sizeof(lut->matrix));
    lut->input_tables = NULL;
    lut->clut_values = NULL;
    lut->output_tables = NULL;
}

static void
sixel_icc_lut_destroy(sixel_icc_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->input_tables != NULL) {
        free(lut->input_tables);
    }
    if (lut->clut_values != NULL) {
        free(lut->clut_values);
    }
    if (lut->output_tables != NULL) {
        free(lut->output_tables);
    }
    sixel_icc_lut_reset(lut);
}

static void
sixel_icc_mab_clut_reset(sixel_icc_mab_clut_t *clut)
{
    if (clut == NULL) {
        return;
    }

    memset(clut, 0, sizeof(*clut));
}

static void
sixel_icc_mab_clut_destroy(sixel_icc_mab_clut_t *clut)
{
    if (clut == NULL) {
        return;
    }

    if (clut->values != NULL) {
        free(clut->values);
    }
    sixel_icc_mab_clut_reset(clut);
}

static void
sixel_icc_mab_pipeline_reset(sixel_icc_mab_pipeline_t *pipeline)
{
    size_t i;

    i = 0u;
    if (pipeline == NULL) {
        return;
    }

    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->type = SIXEL_ICC_MAB_TYPE_INVALID;
    for (i = 0u; i < 16u; ++i) {
        sixel_icc_curve_reset(&pipeline->a_curves[i]);
        sixel_icc_curve_reset(&pipeline->m_curves[i]);
        sixel_icc_curve_reset(&pipeline->b_curves[i]);
    }
    sixel_icc_mab_clut_reset(&pipeline->clut);
}

static void
sixel_icc_mab_pipeline_destroy(sixel_icc_mab_pipeline_t *pipeline)
{
    size_t i;

    i = 0u;
    if (pipeline == NULL) {
        return;
    }

    for (i = 0u; i < 16u; ++i) {
        sixel_icc_curve_destroy(&pipeline->a_curves[i]);
        sixel_icc_curve_destroy(&pipeline->m_curves[i]);
        sixel_icc_curve_destroy(&pipeline->b_curves[i]);
    }
    sixel_icc_mab_clut_destroy(&pipeline->clut);
    pipeline->type = SIXEL_ICC_MAB_TYPE_INVALID;
    pipeline->input_channels = 0u;
    pipeline->output_channels = 0u;
    pipeline->has_a_curves = 0;
    pipeline->has_m_curves = 0;
    pipeline->has_b_curves = 0;
    pipeline->has_clut = 0;
    pipeline->has_matrix = 0;
    memset(pipeline->matrix, 0, sizeof(pipeline->matrix));
    memset(pipeline->matrix_offset, 0, sizeof(pipeline->matrix_offset));
}

static void
sixel_icc_mpe_curve_destroy(sixel_icc_mpe_curve_t *curve)
{
    double *samples;
    size_t segment;

    /* Each curve owns its initialized segment array, including partially
     * decoded segments. Detach each allocation before releasing it so
     * later iterations cannot retain a stale owner in the array. */
    for (segment = 0u; segment < curve->segment_count; ++segment) {
        samples = curve->segments[segment].samples;
        curve->segments[segment].samples = NULL;
        free(samples);
    }
    free(curve->segments);
    memset(curve, 0, sizeof(*curve));
}

static void
sixel_icc_mpe_curves_destroy(sixel_icc_mpe_t *mpe)
{
    size_t channel;

    if (mpe->curves == NULL) {
        return;
    }
    for (channel = 0u; channel < mpe->input_channels; ++channel) {
        sixel_icc_mpe_curve_destroy(&mpe->curves[channel]);
    }
    free(mpe->curves);
}

static void
sixel_icc_mpet_destroy(sixel_icc_mpet_t *mpet)
{
    double *values;
    size_t i;

    if (!mpet->shared_payload) {
        for (i = 0u; i < mpet->element_count; ++i) {
            sixel_icc_mpe_curves_destroy(&mpet->elements[i]);
            values = mpet->elements[i].values;
            mpet->elements[i].values = NULL;
            free(values);
        }
        free(mpet->elements);
        free(mpet->data);
    }
    memset(mpet, 0, sizeof(*mpet));
}

static void
sixel_icc_profile_reset(sixel_icc_profile_t *profile)
{
    size_t i;

    if (profile == NULL) {
        return;
    }

    i = 0u;
    memset(profile, 0, sizeof(*profile));
    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    profile->pcs = SIXEL_ICC_PROFILE_PCS_INVALID;
    sixel_icc_curve_reset(&profile->curves[0]);
    sixel_icc_curve_reset(&profile->curves[1]);
    sixel_icc_curve_reset(&profile->curves[2]);
    for (i = 0u; i < SIXEL_ICC_A2B_SLOT_COUNT; ++i) {
        sixel_icc_lut_reset(&profile->a2b_lut[i]);
        sixel_icc_mab_pipeline_reset(&profile->a2b_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_B2A_SLOT_COUNT; ++i) {
        sixel_icc_lut_reset(&profile->b2a_lut[i]);
        sixel_icc_mab_pipeline_reset(&profile->b2a_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_D2B_SLOT_COUNT; ++i) {
        sixel_icc_lut_reset(&profile->d2b_lut[i]);
        sixel_icc_mab_pipeline_reset(&profile->d2b_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_B2D_SLOT_COUNT; ++i) {
        sixel_icc_lut_reset(&profile->b2d_lut[i]);
        sixel_icc_mab_pipeline_reset(&profile->b2d_mab[i]);
    }
}

void
sixel_icc_profile_destroy(sixel_icc_profile_t *profile)
{
    size_t i;

    if (profile == NULL) {
        return;
    }

    for (i = 0u; i < 3u; ++i) {
        sixel_icc_curve_destroy(&profile->curves[i]);
    }
    for (i = 0u; i < SIXEL_ICC_A2B_SLOT_COUNT; ++i) {
        sixel_icc_lut_destroy(&profile->a2b_lut[i]);
        sixel_icc_mab_pipeline_destroy(&profile->a2b_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_B2A_SLOT_COUNT; ++i) {
        sixel_icc_lut_destroy(&profile->b2a_lut[i]);
        sixel_icc_mab_pipeline_destroy(&profile->b2a_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_D2B_SLOT_COUNT; ++i) {
        sixel_icc_lut_destroy(&profile->d2b_lut[i]);
        sixel_icc_mab_pipeline_destroy(&profile->d2b_mab[i]);
    }
    for (i = 0u; i < SIXEL_ICC_B2D_SLOT_COUNT; ++i) {
        sixel_icc_lut_destroy(&profile->b2d_lut[i]);
        sixel_icc_mab_pipeline_destroy(&profile->b2d_mab[i]);
    }

    for (i = 0u; i < SIXEL_ICC_MPET_SLOT_COUNT; ++i) {
        sixel_icc_mpet_destroy(&profile->d2b_mpet[i]);
        sixel_icc_mpet_destroy(&profile->b2d_mpet[i]);
    }

    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    profile->pcs = SIXEL_ICC_PROFILE_PCS_INVALID;
    profile->lut_only = 0;
    memset(profile->matrix_to_xyz_d50, 0, sizeof(profile->matrix_to_xyz_d50));
    memset(profile->media_white_xyz, 0, sizeof(profile->media_white_xyz));
}

static int
sixel_icc_find_tag(unsigned char const *profile_data,
                   size_t profile_size,
                   char const signature[4],
                   size_t *out_offset,
                   size_t *out_length,
                   sixel_icc_issue_t *issue)
{
    uint32_t tag_count;
    size_t table_offset;
    size_t i;

    if (profile_data == NULL || signature == NULL ||
        out_offset == NULL || out_length == NULL) {
        return 0;
    }
    if (profile_size < 132u) {
        return 0;
    }

    tag_count = sixel_icc_read_be32(profile_data + 128u);
    if ((size_t)tag_count > (profile_size - 132u) / 12u) {
        return 0;
    }

    table_offset = 132u;
    for (i = 0u; i < (size_t)tag_count; ++i) {
        unsigned char const *entry;
        uint32_t tag_offset_u32;
        uint32_t tag_length_u32;
        size_t tag_offset;
        size_t tag_length;

        entry = profile_data + table_offset + i * 12u;
        if (memcmp(entry + 0u, signature, 4u) != 0) {
            continue;
        }

        tag_offset_u32 = sixel_icc_read_be32(entry + 4u);
        tag_length_u32 = sixel_icc_read_be32(entry + 8u);
        tag_offset = (size_t)tag_offset_u32;
        tag_length = (size_t)tag_length_u32;

        if (tag_length == 0u || tag_offset > profile_size ||
            tag_length > profile_size - tag_offset) {
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
            return 0;
        }

        *out_offset = tag_offset;
        *out_length = tag_length;
        return 1;
    }

    return 0;
}

static int
sixel_icc_parse_xyz_tag(unsigned char const *profile_data,
                        size_t profile_size,
                        size_t tag_offset,
                        size_t tag_length,
                        double xyz[3])
{
    unsigned char const *tag_data;

    if (profile_data == NULL || xyz == NULL) {
        return 0;
    }
    if (tag_offset > profile_size || tag_length > profile_size - tag_offset) {
        return 0;
    }
    if (tag_length < 20u) {
        return 0;
    }

    tag_data = profile_data + tag_offset;
    if (memcmp(tag_data + 0u, "XYZ ", 4u) != 0) {
        return 0;
    }

    xyz[0] = sixel_icc_read_s15fixed16(tag_data + 8u);
    xyz[1] = sixel_icc_read_s15fixed16(tag_data + 12u);
    xyz[2] = sixel_icc_read_s15fixed16(tag_data + 16u);

    return 1;
}

static double
sixel_icc_parse_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
sixel_icc_eval_parametric(uint16_t function_type,
                          double const *params,
                          size_t param_count,
                          double x,
                          int *ok)
{
    double y;

    y = 0.0;
    if (ok != NULL) {
        *ok = 1;
    }
    x = sixel_icc_parse_clamp_unit(x);

    switch (function_type) {
    case 0u:
        if (param_count < 1u || params[0] <= 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        y = pow(x, params[0]);
        break;
    case 1u:
        if (param_count < 3u || params[0] <= 0.0 || params[1] == 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= (-params[2] / params[1])) {
            y = pow(params[1] * x + params[2], params[0]);
        } else {
            y = 0.0;
        }
        break;
    case 2u:
        if (param_count < 4u || params[0] <= 0.0 || params[1] == 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= (-params[2] / params[1])) {
            y = pow(params[1] * x + params[2], params[0]) + params[3];
        } else {
            y = params[3];
        }
        break;
    case 3u:
        if (param_count < 5u || params[0] <= 0.0 || params[1] <= 0.0 ||
            params[3] < 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= params[4]) {
            y = pow(params[1] * x + params[2], params[0]);
        } else {
            y = params[3] * x;
        }
        break;
    case 4u:
        if (param_count < 7u || params[0] <= 0.0 || params[1] <= 0.0 ||
            params[3] < 0.0 || params[4] < 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        /* ICC.1:2022 Table 68: g,a,b,c,d,e,f; d is the threshold. */
        if (x >= params[4]) {
            y = pow(params[1] * x + params[2], params[0]) + params[5];
        } else {
            y = params[3] * x + params[6];
        }
        break;
    default:
        if (ok != NULL) {
            *ok = 0;
        }
        return 0.0;
    }

    /*
     * Finite ICC coefficients can still yield NaN (a fractional power of a
     * negative base) or infinity (power overflow). Reject before clamping:
     * NaN survives comparisons and cannot be cast to a table integer, while
     * clamping infinity would silently turn an invalid curve into an endpoint.
     * Both para and segm table builders use ok to release partial allocations.
     */
    if (!isfinite(y)) {
        if (ok != NULL) {
            *ok = 0;
        }
        return 0.0;
    }
    return sixel_icc_parse_clamp_unit(y);
}

int
sixel_icc_parametric_device(sixel_icc_curve_t const *curve, double value,
                              double *out)
{
    double result;
    int ok;

    if (curve == NULL || curve->param_count == 0u || out == NULL ||
        !isfinite(value)) {
        return 0;
    }
    result = sixel_icc_eval_parametric(curve->function_type, curve->params,
                                        curve->param_count, value, &ok);
    if (!ok || !isfinite(result)) {
        return 0;
    }
    *out = result;
    return 1;
}

typedef struct sixel_icc_segm_segment {
    uint16_t function_type;
    size_t param_count;
    double params[7];
} sixel_icc_segm_segment_t;

static int
sixel_icc_size_add(size_t a, size_t b, size_t *out);

static int
sixel_icc_size_multiply(size_t a, size_t b, size_t *out);

static int
sixel_icc_parse_segm_param_count(uint16_t function_type, size_t *param_count)
{
    if (param_count == NULL) {
        return 0;
    }

    switch (function_type) {
    case 0u:
        *param_count = 1u;
        return 1;
    case 1u:
        *param_count = 3u;
        return 1;
    case 2u:
        *param_count = 4u;
        return 1;
    case 3u:
        *param_count = 5u;
        return 1;
    case 4u:
        *param_count = 7u;
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_icc_parse_segmented_curve_tag(unsigned char const *tag_data,
                                    size_t tag_length,
                                    sixel_icc_curve_t *out_curve,
                                sixel_icc_parse_budget_t *budget,
                                sixel_icc_issue_t *issue)
{
    uint32_t segment_count_u32;
    size_t segment_count;
    size_t breakpoint_count;
    double *breakpoints;
    sixel_icc_segm_segment_t *segments;
    size_t offset;
    size_t i;
    size_t j;
    size_t sample_count;
    size_t segment_index;
    size_t param_count;
    uint16_t function_type;
    sixel_icc_segm_segment_t const *segment;
    double x;
    double y;
    int ok;

    segment_count_u32 = 0u;
    segment_count = 0u;
    breakpoint_count = 0u;
    breakpoints = NULL;
    segments = NULL;
    offset = 0u;
    i = 0u;
    j = 0u;
    sample_count = 0u;
    segment_index = 0u;
    param_count = 0u;
    function_type = 0u;
    segment = NULL;
    x = 0.0;
    y = 0.0;
    ok = 0;

    if (tag_data == NULL || out_curve == NULL || tag_length < 12u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "segm", 4u) != 0) {
        return 0;
    }

    segment_count_u32 = sixel_icc_read_be32(tag_data + 8u);
    segment_count = (size_t)segment_count_u32;
    if (segment_count == 0u) {
        return 0;
    }

    if (segment_count > SIZE_MAX / sizeof(*segments)) {
        return 0;
    }
    breakpoint_count = segment_count - 1u;
    if (breakpoint_count > 0u) {
        if (breakpoint_count > (tag_length - 12u) / 4u) {
            return 0;
        }
        if (breakpoint_count > SIZE_MAX / sizeof(*breakpoints)) {
            return 0;
        }
        breakpoints = (double *)sixel_icc_parse_scratch(budget,
            breakpoint_count * sizeof(*breakpoints), issue);
        if (breakpoints == NULL) {
            return 0;
        }
    }

    segments = (sixel_icc_segm_segment_t *)sixel_icc_parse_scratch(budget,
        segment_count * sizeof(*segments), issue);
    if (segments == NULL) {
        free(breakpoints);
        return 0;
    }
    memset(segments, 0, segment_count * sizeof(*segments));

    offset = 12u;
    for (i = 0u; i < breakpoint_count; ++i) {
        breakpoints[i] = sixel_icc_read_s15fixed16(tag_data + offset);
        if (i > 0u && breakpoints[i] < breakpoints[i - 1u]) {
            free(segments);
            free(breakpoints);
            return 0;
        }
        offset += 4u;
    }

    for (i = 0u; i < segment_count; ++i) {
        param_count = 0u;
        if (tag_length - offset < 4u) {
            free(segments);
            free(breakpoints);
            return 0;
        }
        function_type = sixel_icc_read_be16(tag_data + offset);
        if (!sixel_icc_parse_segm_param_count(function_type, &param_count)) {
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
            free(segments);
            free(breakpoints);
            return 0;
        }
        if (param_count > 7u || param_count > (tag_length - offset - 4u) / 4u) {
            free(segments);
            free(breakpoints);
            return 0;
        }

        segments[i].function_type = function_type;
        segments[i].param_count = param_count;
        for (j = 0u; j < param_count; ++j) {
            segments[i].params[j] = sixel_icc_read_s15fixed16(
                tag_data + offset + 4u + j * 4u);
        }
        offset += 4u + param_count * 4u;
    }

    sample_count = SIXEL_ICC_PARAMETRIC_TABLE_SIZE;
    out_curve->table = (uint16_t *)sixel_icc_parse_alloc(budget,
        sample_count * sizeof(uint16_t), issue);
    if (out_curve->table == NULL) {
        free(segments);
        free(breakpoints);
        return 0;
    }
    out_curve->table_length = sample_count;
    out_curve->kind = SIXEL_ICC_CURVE_SEGM_TABLE;

    for (i = 0u; i < sample_count; ++i) {
        x = (sample_count <= 1u)
            ? 0.0
            : (double)i / (double)(sample_count - 1u);
        segment_index = segment_count - 1u;
        if (segment_count > 1u) {
            if (breakpoints == NULL) {
                free(out_curve->table);
                out_curve->table = NULL;
                out_curve->table_length = 0u;
                out_curve->kind = SIXEL_ICC_CURVE_INVALID;
                free(segments);
                return 0;
            }
            for (j = 0u; j < breakpoint_count; ++j) {
                if (x <= breakpoints[j]) {
                    segment_index = j;
                    break;
                }
            }
        }
        if (segment_index >= segment_count) {
            free(out_curve->table);
            out_curve->table = NULL;
            out_curve->table_length = 0u;
            out_curve->kind = SIXEL_ICC_CURVE_INVALID;
            free(segments);
            free(breakpoints);
            return 0;
        }
        segment = &segments[segment_index];

        y = sixel_icc_eval_parametric(segment->function_type,
                                      segment->params,
                                      segment->param_count,
                                      x,
                                      &ok);
        if (!ok) {
            free(out_curve->table);
            out_curve->table = NULL;
            out_curve->table_length = 0u;
            out_curve->kind = SIXEL_ICC_CURVE_INVALID;
            free(segments);
            free(breakpoints);
            return 0;
        }

        out_curve->table[i] = (uint16_t)(
            sixel_icc_parse_clamp_unit(y) * 65535.0 + 0.5);
    }

    free(segments);
    free(breakpoints);
    return 1;
}

static int
sixel_icc_parse_curve_tag_length(unsigned char const *tag_data,
                                 size_t tag_length,
                                 size_t *out_length,
                                sixel_icc_issue_t *issue)
{
    size_t offset;
    size_t segment_count;
    size_t breakpoint_count;
    size_t i;
    uint16_t function_type;
    size_t param_count;

    offset = 0u;
    segment_count = 0u;
    breakpoint_count = 0u;
    i = 0u;
    function_type = 0u;
    param_count = 0u;

    if (tag_data == NULL || out_length == NULL || tag_length < 12u) {
        return 0;
    }

    if (memcmp(tag_data + 0u, "curv", 4u) == 0) {
        uint32_t count;
        size_t required_length;

        count = sixel_icc_read_be32(tag_data + 8u);
        required_length = 12u;
        if (count == 0u) {
            *out_length = required_length;
            return 1;
        }
        if (count == 1u) {
            required_length = 14u;
            if (required_length > tag_length) {
                return 0;
            }
            *out_length = required_length;
            return 1;
        }

        if (!sixel_icc_size_multiply((size_t)count, 2u, &required_length) ||
            !sixel_icc_size_add(12u, required_length, &required_length) ||
            required_length > tag_length) {
            return 0;
        }
        *out_length = required_length;
        return 1;
    }

    if (memcmp(tag_data + 0u, "para", 4u) == 0) {
        function_type = sixel_icc_read_be16(tag_data + 8u);
        if (!sixel_icc_parse_segm_param_count(function_type, &param_count)) {
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
            return 0;
        }
        if (!sixel_icc_size_multiply(param_count, 4u, &offset) ||
            !sixel_icc_size_add(12u, offset, &offset) ||
            offset > tag_length) {
            return 0;
        }
        *out_length = offset;
        return 1;
    }

    if (memcmp(tag_data + 0u, "segm", 4u) != 0) {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
        return 0;
    }

    segment_count = (size_t)sixel_icc_read_be32(tag_data + 8u);
    if (segment_count == 0u) {
        return 0;
    }

    breakpoint_count = segment_count - 1u;
    if (!sixel_icc_size_multiply(breakpoint_count, 4u, &offset) ||
        !sixel_icc_size_add(12u, offset, &offset) ||
        offset > tag_length) {
        return 0;
    }

    for (i = 0u; i < segment_count; ++i) {
        if (tag_length - offset < 4u) {
            return 0;
        }

        function_type = sixel_icc_read_be16(tag_data + offset);
        if (!sixel_icc_parse_segm_param_count(function_type, &param_count)) {
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
            return 0;
        }
        if (!sixel_icc_size_multiply(param_count, 4u, &param_count) ||
            !sixel_icc_size_add(4u, param_count, &param_count) ||
            param_count > tag_length - offset ||
            !sixel_icc_size_add(offset, param_count, &offset)) {
            return 0;
        }
    }

    *out_length = offset;
    return 1;
}

static int
sixel_icc_parse_curve_data(unsigned char const *tag_data,
                           size_t tag_length,
                           sixel_icc_curve_t *out_curve,
                           sixel_icc_parse_budget_t *budget,
                           sixel_icc_issue_t *issue)
{
    if (tag_data == NULL || out_curve == NULL || tag_length < 12u) {
        return 0;
    }

    if (memcmp(tag_data + 0u, "curv", 4u) == 0) {
        uint32_t count;
        size_t table_length;
        size_t i;

        count = sixel_icc_read_be32(tag_data + 8u);
        if (count == 0u) {
            out_curve->kind = SIXEL_ICC_CURVE_IDENTITY;
            out_curve->gamma = 1.0;
            return 1;
        }
        if (count == 1u) {
            uint16_t gamma_u8f8;

            if (tag_length < 14u) {
                return 0;
            }
            gamma_u8f8 = sixel_icc_read_be16(tag_data + 12u);
            out_curve->kind = SIXEL_ICC_CURVE_GAMMA;
            out_curve->gamma = (double)gamma_u8f8 / 256.0;
            if (out_curve->gamma <= 0.0) {
                out_curve->gamma = 1.0;
            }
            return 1;
        }

#if SIZE_MAX <= UINT32_MAX
        if ((size_t)count > (SIZE_MAX - 12u) / 2u) {
            return 0;
        }
#endif
        if (tag_length < 12u + (size_t)count * 2u) {
            return 0;
        }
#if SIZE_MAX <= UINT32_MAX
        if ((size_t)count > SIZE_MAX / sizeof(uint16_t)) {
            return 0;
        }
#endif
        table_length = (size_t)count;

        out_curve->table = (uint16_t *)sixel_icc_parse_alloc(budget,
            table_length * sizeof(uint16_t), issue);
        if (out_curve->table == NULL) {
            return 0;
        }
        out_curve->table_length = table_length;
        out_curve->kind = SIXEL_ICC_CURVE_TABLE;
        for (i = 0u; i < table_length; ++i) {
            out_curve->table[i] = sixel_icc_read_be16(tag_data + 12u + i * 2u);
        }
        return 1;
    }

    if (memcmp(tag_data + 0u, "para", 4u) == 0) {
        uint16_t function_type;
        size_t param_count;
        double params[7];
        size_t sample_count;
        size_t i;

        function_type = sixel_icc_read_be16(tag_data + 8u);
        switch (function_type) {
        case 0u:
            param_count = 1u;
            break;
        case 1u:
            param_count = 3u;
            break;
        case 2u:
            param_count = 4u;
            break;
        case 3u:
            param_count = 5u;
            break;
        case 4u:
            param_count = 7u;
            break;
        default:
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
            return 0;
        }
        if (tag_length < 12u + param_count * 4u) {
            return 0;
        }

        memset(params, 0, sizeof(params));
        for (i = 0u; i < param_count; ++i) {
            params[i] = sixel_icc_read_s15fixed16(tag_data + 12u + i * 4u);
        }

        sample_count = SIXEL_ICC_PARAMETRIC_TABLE_SIZE;
        out_curve->table = (uint16_t *)sixel_icc_parse_alloc(budget,
            sample_count * sizeof(uint16_t), issue);
        if (out_curve->table == NULL) {
            return 0;
        }
        out_curve->table_length = sample_count;
        out_curve->kind = SIXEL_ICC_CURVE_TABLE;

        for (i = 0u; i < sample_count; ++i) {
            double x;
            double y;
            int ok;

            x = (sample_count <= 1u)
                ? 0.0
                : (double)i / (double)(sample_count - 1u);
            y = sixel_icc_eval_parametric(function_type,
                                          params,
                                          param_count,
                                          x,
                                          &ok);
            if (!ok) {
                free(out_curve->table);
                out_curve->table = NULL;
                out_curve->table_length = 0u;
                out_curve->kind = SIXEL_ICC_CURVE_INVALID;
                return 0;
            }

            out_curve->table[i] = (uint16_t)(sixel_icc_parse_clamp_unit(y)
                                             * 65535.0 + 0.5);
        }
        out_curve->param_count = param_count;
        out_curve->function_type = function_type;
        memcpy(out_curve->params, params, sizeof(params));
        return 1;
    }
    if (memcmp(tag_data + 0u, "segm", 4u) == 0) {
        return sixel_icc_parse_segmented_curve_tag(tag_data,
                                                   tag_length,
                                                   out_curve, budget, issue);
    }

    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
    return 0;
}

static int
sixel_icc_parse_curve_tag(unsigned char const *profile_data,
                          size_t profile_size,
                          size_t tag_offset,
                          size_t tag_length,
                          sixel_icc_curve_t *out_curve,
                          sixel_icc_parse_budget_t *budget,
                          sixel_icc_issue_t *issue)
{
    sixel_icc_issue_t failure;

    if (profile_data == NULL || out_curve == NULL) {
        return 0;
    }
    if (tag_offset > profile_size || tag_length > profile_size - tag_offset) {
        return 0;
    }

    failure = SIXEL_ICC_ISSUE_NONE;
    if (sixel_icc_parse_curve_data(profile_data + tag_offset, tag_length,
                                   out_curve, budget, &failure)) {
        return 1;
    }
    sixel_icc_record_issue(issue, failure == SIXEL_ICC_ISSUE_NONE
        ? SIXEL_ICC_ISSUE_MALFORMED : failure);
    return 0;
}

static int
sixel_icc_size_add(size_t a, size_t b, size_t *out)
{
    if (out == NULL) {
        return 0;
    }
    if (a > SIZE_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static int
sixel_icc_size_multiply(size_t a, size_t b, size_t *out)
{
    if (out == NULL) {
        return 0;
    }
    if (a != 0u && b > SIZE_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int
sixel_icc_size_pow(unsigned int base, unsigned int exponent, size_t *out)
{
    size_t value;
    unsigned int i;

    if (out == NULL) {
        return 0;
    }

    value = 1u;
    for (i = 0u; i < exponent; ++i) {
        if (base != 0u && value > SIZE_MAX / (size_t)base) {
            return 0;
        }
        value *= (size_t)base;
    }

    *out = value;
    return 1;
}

static size_t
sixel_icc_align4(size_t value)
{
    return (value + 3u) & ~(size_t)3u;
}

static int
sixel_icc_parse_curve_block(unsigned char const *tag_data,
                            size_t tag_length,
                            size_t block_offset,
                            size_t curve_count,
                            sixel_icc_curve_t *out_curves,
                            sixel_icc_parse_budget_t *budget,
                            sixel_icc_issue_t *issue)
{
    size_t i;
    size_t pos;
    size_t curve_length;
    size_t next_pos;

    i = 0u;
    pos = 0u;
    curve_length = 0u;
    next_pos = 0u;
    if (tag_data == NULL || out_curves == NULL || curve_count == 0u) {
        return 0;
    }
    if (block_offset >= tag_length) {
        return 0;
    }

    pos = block_offset;
    for (i = 0u; i < curve_count; ++i) {
        if (pos >= tag_length) {
            goto fail;
        }
        if (!sixel_icc_parse_curve_tag_length(tag_data + pos,
                                              tag_length - pos,
                                              &curve_length, issue) ||
            !sixel_icc_parse_curve_data(tag_data + pos,
                                        curve_length,
                                        &out_curves[i], budget, issue) ||
            !sixel_icc_size_add(pos, curve_length, &next_pos)) {
            goto fail;
        }

        next_pos = sixel_icc_align4(next_pos);
        if (next_pos > tag_length) {
            goto fail;
        }
        pos = next_pos;
    }

    return 1;

fail:
    while (i > 0u) {
        --i;
        sixel_icc_curve_destroy(&out_curves[i]);
    }
    return 0;
}

static int
sixel_icc_parse_mab_clut_block(unsigned char const *tag_data,
                               size_t tag_length,
                               size_t block_offset,
                               uint8_t input_channels,
                               uint8_t output_channels,
                               sixel_icc_mab_clut_t *out_clut,
                               sixel_icc_parse_budget_t *budget,
                               sixel_icc_issue_t *issue)
{
    sixel_icc_mab_clut_t parsed;
    unsigned char precision;
    size_t clut_points;
    size_t value_count;
    size_t value_bytes;
    size_t value_offset;
    size_t i;

    precision = 0u;
    clut_points = 0u;
    value_count = 0u;
    value_bytes = 0u;
    value_offset = 0u;
    i = 0u;
    sixel_icc_mab_clut_reset(&parsed);

    if (tag_data == NULL || out_clut == NULL || input_channels == 0u ||
        output_channels == 0u || input_channels > 16u ||
        output_channels > 16u || block_offset > tag_length ||
        tag_length - block_offset < 20u) {
        return 0;
    }

    memcpy(parsed.grid_points, tag_data + block_offset, 16u);
    precision = tag_data[block_offset + 16u];
    if (precision != 1u && precision != 2u) {
        return 0;
    }

    clut_points = 1u;
    for (i = 0u; i < (size_t)input_channels; ++i) {
        if (parsed.grid_points[i] == 0u) {
            return 0;
        }
        if (!sixel_icc_size_multiply(clut_points,
                                     (size_t)parsed.grid_points[i],
                                     &clut_points)) {
            return 0;
        }
    }
    if (!sixel_icc_size_multiply(clut_points,
                                 (size_t)output_channels,
                                 &value_count) ||
        !sixel_icc_size_multiply(value_count, (size_t)precision, &value_bytes) ||
        value_bytes > tag_length - block_offset - 20u ||
        value_count > SIZE_MAX / sizeof(*parsed.values)) {
        return 0;
    }

    parsed.values = (uint16_t *)sixel_icc_parse_alloc(budget,
        value_count * sizeof(*parsed.values), issue);
    if (parsed.values == NULL) {
        return 0;
    }

    value_offset = block_offset + 20u;
    if (precision == 1u) {
        for (i = 0u; i < value_count; ++i) {
            parsed.values[i] = (uint16_t)((unsigned int)
                                          tag_data[value_offset + i] * 257u);
        }
    } else {
        for (i = 0u; i < value_count; ++i) {
            parsed.values[i] = sixel_icc_read_be16(tag_data + value_offset
                                                   + i * 2u);
        }
    }

    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.value_count = value_count;

    sixel_icc_mab_clut_destroy(out_clut);
    *out_clut = parsed;
    return 1;
}

static int
sixel_icc_parse_mab_matrix_block(unsigned char const *tag_data,
                                 size_t tag_length,
                                 size_t block_offset,
                                 double matrix[3][3],
                                 double matrix_offset[3])
{
    size_t i;
    size_t offset;

    if (tag_data == NULL || matrix == NULL || matrix_offset == NULL ||
        block_offset > tag_length || tag_length - block_offset < 48u) {
        return 0;
    }

    sixel_icc_read_matrix(matrix, tag_data + block_offset);
    offset = block_offset + 36u;
    for (i = 0u; i < 3u; ++i) {
        matrix_offset[i] = sixel_icc_read_s15fixed16(tag_data + offset);
        offset += 4u;
    }

    return 1;
}

static int
sixel_icc_parse_mab_tag_data(unsigned char const *tag_data,
                             size_t tag_length,
                             sixel_icc_mab_pipeline_t *out_pipeline,
                             sixel_icc_parse_budget_t *budget,
                             sixel_icc_issue_t *issue)
{
    sixel_icc_mab_pipeline_t parsed;
    size_t b_offset;
    size_t matrix_offset;
    size_t m_offset;
    size_t clut_offset;
    size_t a_offset;
    size_t a_count;
    size_t m_count;
    size_t b_count;
    size_t matrix_channel_count;

    b_offset = 0u;
    matrix_offset = 0u;
    m_offset = 0u;
    clut_offset = 0u;
    a_offset = 0u;
    a_count = 0u;
    m_count = 0u;
    b_count = 0u;
    matrix_channel_count = 0u;
    sixel_icc_mab_pipeline_reset(&parsed);

    if (tag_data == NULL || out_pipeline == NULL || tag_length < 32u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "mAB ", 4u) == 0) {
        parsed.type = SIXEL_ICC_MAB_TYPE_MAB;
    } else if (memcmp(tag_data + 0u, "mBA ", 4u) == 0) {
        parsed.type = SIXEL_ICC_MAB_TYPE_MBA;
    } else {
        return 0;
    }

    parsed.input_channels = tag_data[8u];
    parsed.output_channels = tag_data[9u];
    if (parsed.input_channels == 0u || parsed.output_channels == 0u ||
        parsed.input_channels > 16u || parsed.output_channels > 16u) {
        return 0;
    }

    b_offset = (size_t)sixel_icc_read_be32(tag_data + 12u);
    matrix_offset = (size_t)sixel_icc_read_be32(tag_data + 16u);
    m_offset = (size_t)sixel_icc_read_be32(tag_data + 20u);
    clut_offset = (size_t)sixel_icc_read_be32(tag_data + 24u);
    a_offset = (size_t)sixel_icc_read_be32(tag_data + 28u);

    if ((b_offset != 0u && b_offset >= tag_length) ||
        (matrix_offset != 0u && matrix_offset >= tag_length) ||
        (m_offset != 0u && m_offset >= tag_length) ||
        (clut_offset != 0u && clut_offset >= tag_length) ||
        (a_offset != 0u && a_offset >= tag_length)) {
        goto fail;
    }

    if (parsed.type == SIXEL_ICC_MAB_TYPE_MAB) {
        a_count = (size_t)parsed.input_channels;
        m_count = (size_t)parsed.output_channels;
        b_count = (size_t)parsed.output_channels;
        matrix_channel_count = (size_t)parsed.output_channels;
    } else {
        a_count = (size_t)parsed.output_channels;
        m_count = (size_t)parsed.input_channels;
        b_count = (size_t)parsed.input_channels;
        matrix_channel_count = (size_t)parsed.input_channels;
    }

    if (a_offset != 0u) {
        if (!sixel_icc_parse_curve_block(tag_data,
                                         tag_length,
                                         a_offset,
                                         a_count,
                                         parsed.a_curves, budget, issue)) {
            goto fail;
        }
        parsed.has_a_curves = 1;
    }

    if (m_offset != 0u) {
        if (!sixel_icc_parse_curve_block(tag_data,
                                         tag_length,
                                         m_offset,
                                         m_count,
                                         parsed.m_curves, budget, issue)) {
            goto fail;
        }
        parsed.has_m_curves = 1;
    }

    if (b_offset != 0u) {
        if (!sixel_icc_parse_curve_block(tag_data,
                                         tag_length,
                                         b_offset,
                                         b_count,
                                         parsed.b_curves, budget, issue)) {
            goto fail;
        }
        parsed.has_b_curves = 1;
    }

    if (clut_offset != 0u) {
        if (!sixel_icc_parse_mab_clut_block(tag_data,
                                            tag_length,
                                            clut_offset,
                                            parsed.input_channels,
                                            parsed.output_channels,
                                            &parsed.clut, budget, issue)) {
            goto fail;
        }
        parsed.has_clut = 1;
    }

    if (matrix_offset != 0u) {
        if (matrix_channel_count != 3u ||
            !sixel_icc_parse_mab_matrix_block(tag_data,
                                              tag_length,
                                              matrix_offset,
                                              parsed.matrix,
                                              parsed.matrix_offset)) {
            goto fail;
        }
        parsed.has_matrix = 1;
    }

    if (!parsed.has_clut && parsed.input_channels != parsed.output_channels) {
        goto fail;
    }
    if (!parsed.has_a_curves && !parsed.has_m_curves &&
        !parsed.has_b_curves && !parsed.has_clut && !parsed.has_matrix) {
        goto fail;
    }

    sixel_icc_mab_pipeline_destroy(out_pipeline);
    *out_pipeline = parsed;
    return 1;

fail:
    sixel_icc_mab_pipeline_destroy(&parsed);
    return 0;
}

static int
sixel_icc_parse_lut8_tag(unsigned char const *tag_data,
                         size_t tag_length,
                         sixel_icc_lut_t *out_lut,
                         sixel_icc_parse_budget_t *budget,
                         sixel_icc_issue_t *issue)
{
    sixel_icc_lut_t parsed;
    unsigned char input_channels;
    unsigned char output_channels;
    unsigned char clut_grid_points;
    uint16_t input_entries;
    uint16_t output_entries;
    size_t input_count;
    size_t clut_points;
    size_t clut_count;
    size_t output_count;
    size_t total_sample_count;
    size_t required_length;
    size_t offset;
    size_t i;

    sixel_icc_lut_reset(&parsed);
    input_channels = 0u;
    output_channels = 0u;
    clut_grid_points = 0u;
    input_entries = 0u;
    output_entries = 0u;
    input_count = 0u;
    clut_points = 0u;
    clut_count = 0u;
    output_count = 0u;
    total_sample_count = 0u;
    required_length = 0u;
    offset = 0u;
    i = 0u;

    if (tag_data == NULL || out_lut == NULL || tag_length < 48u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "mft1", 4u) != 0) {
        return 0;
    }

    input_channels = tag_data[8u];
    output_channels = tag_data[9u];
    clut_grid_points = tag_data[10u];
    input_entries = 256u;
    output_entries = 256u;

    if (input_channels == 0u || output_channels == 0u || clut_grid_points == 0u) {
        return 0;
    }

    if (!sixel_icc_size_multiply((size_t)input_channels,
                                 (size_t)input_entries,
                                 &input_count) ||
        !sixel_icc_size_pow((unsigned int)clut_grid_points,
                            (unsigned int)input_channels,
                            &clut_points) ||
        !sixel_icc_size_multiply(clut_points,
                                 (size_t)output_channels,
                                 &clut_count) ||
        !sixel_icc_size_multiply((size_t)output_channels,
                                 (size_t)output_entries,
                                 &output_count) ||
        !sixel_icc_size_add(input_count, clut_count, &total_sample_count) ||
        !sixel_icc_size_add(total_sample_count,
                            output_count,
                            &total_sample_count) ||
        !sixel_icc_size_add(48u, total_sample_count, &required_length) ||
        required_length > tag_length) {
        return 0;
    }

    if (!sixel_icc_parse_fits(budget, total_sample_count,
                              sizeof(*parsed.clut_values), issue)) {
        return 0;
    }

    parsed.input_tables = (uint16_t *)sixel_icc_parse_alloc(budget,
        input_count * sizeof(*parsed.input_tables), issue);
    parsed.clut_values = (uint16_t *)sixel_icc_parse_alloc(budget,
        clut_count * sizeof(*parsed.clut_values), issue);
    parsed.output_tables = (uint16_t *)sixel_icc_parse_alloc(budget,
        output_count * sizeof(*parsed.output_tables), issue);
    if (parsed.input_tables == NULL ||
        parsed.clut_values == NULL ||
        parsed.output_tables == NULL) {
        goto fail;
    }

    offset = 48u;
    for (i = 0u; i < input_count; ++i) {
        parsed.input_tables[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }
    offset += input_count;

    for (i = 0u; i < clut_count; ++i) {
        parsed.clut_values[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }
    offset += clut_count;

    for (i = 0u; i < output_count; ++i) {
        parsed.output_tables[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }

    sixel_icc_read_matrix(parsed.matrix, tag_data + 12u);
    parsed.kind = SIXEL_ICC_LUT_MFT1;
    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.clut_grid_points = clut_grid_points;
    parsed.input_entries = input_entries;
    parsed.output_entries = output_entries;

    sixel_icc_lut_destroy(out_lut);
    *out_lut = parsed;
    return 1;

fail:
    sixel_icc_lut_destroy(&parsed);
    return 0;
}

static int
sixel_icc_parse_lut16_tag(unsigned char const *tag_data,
                          size_t tag_length,
                          sixel_icc_lut_t *out_lut,
                          sixel_icc_parse_budget_t *budget,
                          sixel_icc_issue_t *issue)
{
    sixel_icc_lut_t parsed;
    unsigned char input_channels;
    unsigned char output_channels;
    unsigned char clut_grid_points;
    uint16_t input_entries;
    uint16_t output_entries;
    size_t input_count;
    size_t clut_points;
    size_t clut_count;
    size_t output_count;
    size_t total_sample_count;
    size_t total_sample_bytes;
    size_t required_length;
    size_t offset;
    size_t i;

    sixel_icc_lut_reset(&parsed);
    input_channels = 0u;
    output_channels = 0u;
    clut_grid_points = 0u;
    input_entries = 0u;
    output_entries = 0u;
    input_count = 0u;
    clut_points = 0u;
    clut_count = 0u;
    output_count = 0u;
    total_sample_count = 0u;
    total_sample_bytes = 0u;
    required_length = 0u;
    offset = 0u;
    i = 0u;

    if (tag_data == NULL || out_lut == NULL || tag_length < 52u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "mft2", 4u) != 0) {
        return 0;
    }

    input_channels = tag_data[8u];
    output_channels = tag_data[9u];
    clut_grid_points = tag_data[10u];
    input_entries = sixel_icc_read_be16(tag_data + 48u);
    output_entries = sixel_icc_read_be16(tag_data + 50u);

    if (input_channels == 0u ||
        output_channels == 0u ||
        clut_grid_points == 0u ||
        input_entries == 0u ||
        output_entries == 0u) {
        return 0;
    }

    if (!sixel_icc_size_multiply((size_t)input_channels,
                                 (size_t)input_entries,
                                 &input_count) ||
        !sixel_icc_size_pow((unsigned int)clut_grid_points,
                            (unsigned int)input_channels,
                            &clut_points) ||
        !sixel_icc_size_multiply(clut_points,
                                 (size_t)output_channels,
                                 &clut_count) ||
        !sixel_icc_size_multiply((size_t)output_channels,
                                 (size_t)output_entries,
                                 &output_count) ||
        !sixel_icc_size_add(input_count, clut_count, &total_sample_count) ||
        !sixel_icc_size_add(total_sample_count,
                            output_count,
                            &total_sample_count) ||
        !sixel_icc_size_multiply(total_sample_count, 2u, &total_sample_bytes) ||
        !sixel_icc_size_add(52u, total_sample_bytes, &required_length) ||
        required_length > tag_length) {
        return 0;
    }

    if (!sixel_icc_parse_fits(budget, total_sample_count,
                              sizeof(*parsed.clut_values), issue)) {
        return 0;
    }

    parsed.input_tables = (uint16_t *)sixel_icc_parse_alloc(budget,
        input_count * sizeof(*parsed.input_tables), issue);
    parsed.clut_values = (uint16_t *)sixel_icc_parse_alloc(budget,
        clut_count * sizeof(*parsed.clut_values), issue);
    parsed.output_tables = (uint16_t *)sixel_icc_parse_alloc(budget,
        output_count * sizeof(*parsed.output_tables), issue);
    if (parsed.input_tables == NULL ||
        parsed.clut_values == NULL ||
        parsed.output_tables == NULL) {
        goto fail;
    }

    offset = 52u;
    for (i = 0u; i < input_count; ++i) {
        parsed.input_tables[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }
    offset += input_count * 2u;

    for (i = 0u; i < clut_count; ++i) {
        parsed.clut_values[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }
    offset += clut_count * 2u;

    for (i = 0u; i < output_count; ++i) {
        parsed.output_tables[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }

    sixel_icc_read_matrix(parsed.matrix, tag_data + 12u);
    parsed.kind = SIXEL_ICC_LUT_MFT2;
    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.clut_grid_points = clut_grid_points;
    parsed.input_entries = input_entries;
    parsed.output_entries = output_entries;

    sixel_icc_lut_destroy(out_lut);
    *out_lut = parsed;
    return 1;

fail:
    sixel_icc_lut_destroy(&parsed);
    return 0;
}

static char const *const sixel_icc_a2b_tag_names[SIXEL_ICC_A2B_SLOT_COUNT] = {
    "A2B0",
    "A2B1",
    "A2B2"
};

static char const *const sixel_icc_b2a_tag_names[SIXEL_ICC_B2A_SLOT_COUNT] = {
    "B2A0",
    "B2A1",
    "B2A2"
};

static char const *const sixel_icc_d2b_tag_names[SIXEL_ICC_MPET_SLOT_COUNT] = {
    "D2B0",
    "D2B1",
    "D2B2",
    "D2B3"
};

static char const *const sixel_icc_b2d_tag_names[SIXEL_ICC_MPET_SLOT_COUNT] = {
    "B2D0",
    "B2D1",
    "B2D2",
    "B2D3"
};

typedef struct sixel_icc_mpe_range {
    uint32_t offset;
    uint32_t length;
    uint16_t index;
} sixel_icc_mpe_range_t;

static int
sixel_icc_compare_mpe_ranges(void const *left, void const *right)
{
    sixel_icc_mpe_range_t const *a;
    sixel_icc_mpe_range_t const *b;

    a = (sixel_icc_mpe_range_t const *)left;
    b = (sixel_icc_mpe_range_t const *)right;
    if (a->offset == b->offset) {
        return (a->index > b->index) - (a->index < b->index);
    }
    return (a->offset > b->offset) - (a->offset < b->offset);
}

/* Sort the ranges independently of the element or channel order. */
static int
sixel_icc_order_mpe_ranges(sixel_icc_mpe_range_t *ranges, size_t count,
                           size_t first_offset)
{
    size_t i;

    qsort(ranges, count, sizeof(*ranges), sixel_icc_compare_mpe_ranges);
    if (ranges[0].offset != first_offset) {
        return 0;
    }
    for (i = 1u; i < count; ++i) {
        if (ranges[i].offset == ranges[i - 1u].offset &&
            ranges[i].length == ranges[i - 1u].length) {
            continue;
        }
        if (ranges[i].offset - ranges[i - 1u].offset <
            ranges[i - 1u].length) {
            return 0;
        }
    }
    return 1;
}

/* Decode finite ICC binary32 numbers without assuming the host layout. */
static int
sixel_icc_mpe_read_float(unsigned char const *data, double *out)
{
    uint32_t bits;
    uint32_t fraction;
    int exponent;
    double value;

    bits = sixel_icc_read_be32(data);
    exponent = (int)((bits >> 23u) & 255u);
    if (exponent == 255) {
        return 0;
    }
    fraction = bits & 0x007fffffu;
    value = exponent == 0 ? ldexp((double)fraction, -149)
        : ldexp((double)(fraction | 0x00800000u), exponent - 150);
    *out = bits >> 31u ? -value : value;
    return 1;
}

static int
sixel_icc_parse_mpe_segment(unsigned char const *data, size_t length,
                             int allows_samples, sixel_icc_mpe_segment_t *seg,
                             size_t *used, sixel_icc_parse_budget_t *budget,
                             sixel_icc_issue_t *issue)
{
    double *values;
    size_t count;
    size_t i;
    int sampled;

    if (length < 12u || sixel_icc_read_be32(data + 4u) != 0u) {
        goto malformed;
    }
    sampled = memcmp(data, "samf", 4u) == 0;
    if (sampled) {
        count = sixel_icc_read_be32(data + 8u);
        if (!allows_samples || count == 0u) {
            goto malformed;
        }
    } else if (memcmp(data, "parf", 4u) == 0) {
        if (sixel_icc_read_be16(data + 10u) != 0u) {
            goto malformed;
        }
        seg->function_type = sixel_icc_read_be16(data + 8u);
        if (seg->function_type > 2u) {
            goto unsupported;
        }
        count = seg->function_type == 0u ? 4u : 5u;
    } else {
        goto unsupported;
    }

    if (count > (length - 12u) / 4u) {
        goto malformed;
    }
    values = seg->params;
    if (sampled) {
        if (!sixel_icc_parse_fits(budget, count, sizeof(*values), issue)) {
            return 0;
        }
        seg->samples = (double *)sixel_icc_parse_alloc(
            budget, count * sizeof(*values), issue);
        if (seg->samples == NULL) {
            return 0;
        }
        seg->sample_count = count;
        values = seg->samples;
    }
    for (i = 0u; i < count; ++i) {
        if (!sixel_icc_mpe_read_float(data + 12u + i * 4u, &values[i])) {
            goto malformed;
        }
    }
    *used = 12u + count * 4u;
    return 1;

unsupported:
    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
    return 0;

malformed:
    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
    return 0;
}

static int
sixel_icc_parse_mpe_curve(unsigned char const *data, size_t length,
                           sixel_icc_mpe_curve_t *curve,
                           sixel_icc_parse_budget_t *budget,
                           sixel_icc_issue_t *issue)
{
    size_t count;
    size_t offset;
    size_t used;
    size_t i;

    if (length < 12u || memcmp(data, "curf", 4u) != 0 ||
        sixel_icc_read_be32(data + 4u) != 0u ||
        sixel_icc_read_be16(data + 10u) != 0u) {
        goto malformed;
    }
    count = sixel_icc_read_be16(data + 8u);
    if (count == 0u || count - 1u > (length - 12u) / 4u) {
        goto malformed;
    }
    offset = 12u + (count - 1u) * 4u;
    if (count > (length - offset) / 12u) {
        goto malformed;
    }
    /* Every segment must already own a null sample pointer if parsing
     * stops at a breakpoint or in an earlier segment. Use zeroed storage
     * before publishing the count consumed by partial-parse cleanup. */
    if (!sixel_icc_parse_fits(budget, count, sizeof(*curve->segments), issue)) {
        return 0;
    }
    curve->segments = (sixel_icc_mpe_segment_t *)calloc(
        count, sizeof(*curve->segments));
    if (curve->segments == NULL) {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_ALLOCATION);
        return 0;
    }
    budget->remaining -= count * sizeof(*curve->segments);
    budget->retained += count * sizeof(*curve->segments);
    curve->segment_count = count;
    for (i = 0u; i + 1u < count; ++i) {
        if (!sixel_icc_mpe_read_float(data + 12u + i * 4u,
                                       &curve->segments[i].end) ||
            (i != 0u &&
             curve->segments[i].end < curve->segments[i - 1u].end)) {
            goto malformed;
        }
    }
    for (i = 0u; i < count; ++i) {
        if (!sixel_icc_parse_mpe_segment(data + offset, length - offset,
                i != 0u && i + 1u < count, &curve->segments[i], &used,
                budget, issue)) {
            return 0;
        }
        offset += used;
    }
    if (offset == length) {
        return 1;
    }

malformed:
    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
    return 0;
}

/* Parse each shared curve once, keeping the original channel assignment. */
static int
sixel_icc_parse_mpe_curves(unsigned char const *data, sixel_icc_mpe_t *mpe,
                            sixel_icc_parse_budget_t *budget,
                            sixel_icc_issue_t *issue)
{
    sixel_icc_mpe_range_t *ranges;
    sixel_icc_mpe_range_t const *range;
    size_t count;
    size_t start;
    size_t i;
    uint16_t owner;
    int ok;

    count = mpe->input_channels;
    start = 12u + count * 8u;
    ranges = (sixel_icc_mpe_range_t *)sixel_icc_parse_scratch(
        budget, count * sizeof(*ranges), issue);
    if (ranges == NULL) {
        return 0;
    }
    for (i = 0u; i < count; ++i) {
        ranges[i].offset = sixel_icc_read_be32(data + 12u + i * 8u);
        ranges[i].length = sixel_icc_read_be32(data + 16u + i * 8u);
        ranges[i].index = (uint16_t)i;
        if (ranges[i].offset < start || ranges[i].offset % 4u != 0u ||
            ranges[i].offset > mpe->length ||
            ranges[i].length > mpe->length - ranges[i].offset ||
            ranges[i].length < 12u) {
            goto malformed;
        }
    }
    if (!sixel_icc_order_mpe_ranges(ranges, count, start)) {
        goto malformed;
    }
    mpe->curves = (sixel_icc_mpe_curve_t *)sixel_icc_parse_alloc(
        budget, count * sizeof(*mpe->curves), issue);
    if (mpe->curves == NULL) {
        ok = 0;
        goto done;
    }
    memset(mpe->curves, 0, count * sizeof(*mpe->curves));
    owner = ranges[0].index;
    for (i = 0u; i < count; ++i) {
        range = &ranges[i];
        if (i == 0u || range->offset != ranges[i - 1u].offset) {
            owner = range->index;
            if (!sixel_icc_parse_mpe_curve(data + range->offset, range->length,
                                            &mpe->curves[owner], budget,
                                            issue)) {
                ok = 0;
                goto done;
            }
        }
        mpe->curves[range->index].curve_channel = owner;
    }
    ok = 1;
    goto done;

malformed:
    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
    ok = 0;
done:
    free(ranges);
    return ok;
}

static sixel_icc_issue_t
sixel_icc_parse_mpe_clut_grid(unsigned char const *data, sixel_icc_mpe_t *mpe)
{
    size_t values;
    size_t i;
    uint8_t points;

    if (mpe->length < 28u ||
        mpe->input_channels > SIXEL_ICC_MPE_CLUT_MAX_AXES) {
        return SIXEL_ICC_ISSUE_MALFORMED;
    }
    values = mpe->output_channels;
    for (i = 0u; i < mpe->input_channels; ++i) {
        points = data[12u + i];
        if (points < 2u || !sixel_icc_size_multiply(values, points, &values)) {
            return SIXEL_ICC_ISSUE_MALFORMED;
        }
        mpe->grid_points[i] = points;
    }
    for (; i < SIXEL_ICC_MPE_CLUT_MAX_AXES; ++i) {
        if (data[12u + i] != 0u) {
            return SIXEL_ICC_ISSUE_MALFORMED;
        }
    }
    if ((mpe->length - 28u) % 4u != 0u ||
        values != (mpe->length - 28u) / 4u) {
        return SIXEL_ICC_ISSUE_MALFORMED;
    }
    return SIXEL_ICC_ISSUE_NONE;
}

static sixel_icc_issue_t
sixel_icc_parse_mpe_header(unsigned char const *data, sixel_icc_mpe_t *mpe)
{
    size_t matrix_values;

    mpe->input_channels = sixel_icc_read_be16(data + 8u);
    mpe->output_channels = sixel_icc_read_be16(data + 10u);
    if (sixel_icc_read_be32(data + 4u) != 0u ||
        mpe->input_channels == 0u || mpe->output_channels == 0u) {
        return SIXEL_ICC_ISSUE_MALFORMED;
    }

    if (memcmp(data, "cvst", 4u) == 0) {
        mpe->kind = SIXEL_ICC_MPE_CURVE_SET;
        if (mpe->input_channels != mpe->output_channels ||
            mpe->input_channels > (mpe->length - 12u) / 8u) {
            return SIXEL_ICC_ISSUE_MALFORMED;
        }
    } else if (memcmp(data, "matf", 4u) == 0) {
        mpe->kind = SIXEL_ICC_MPE_MATRIX;
        if (!sixel_icc_size_multiply((size_t)mpe->input_channels + 1u,
                                     mpe->output_channels, &matrix_values) ||
            (mpe->length - 12u) % 4u != 0u ||
            matrix_values != (mpe->length - 12u) / 4u) {
            return SIXEL_ICC_ISSUE_MALFORMED;
        }
    } else if (memcmp(data, "clut", 4u) == 0) {
        mpe->kind = SIXEL_ICC_MPE_CLUT;
        return sixel_icc_parse_mpe_clut_grid(data, mpe);
    } else if (memcmp(data, "bACS", 4u) == 0 ||
               memcmp(data, "eACS", 4u) == 0) {
        mpe->kind = data[0] == 'b'
            ? SIXEL_ICC_MPE_BEGIN_ACS : SIXEL_ICC_MPE_END_ACS;
        if (mpe->length != 16u ||
            mpe->input_channels != mpe->output_channels) {
            return SIXEL_ICC_ISSUE_MALFORMED;
        }
    } else {
        return SIXEL_ICC_ISSUE_UNSUPPORTED;
    }
    return SIXEL_ICC_ISSUE_NONE;
}

/* The element header already established the encoded value count. */
static int
sixel_icc_parse_mpe_values(unsigned char const *data, sixel_icc_mpe_t *mpe,
                            sixel_icc_parse_budget_t *budget,
                            sixel_icc_issue_t *issue)
{
    size_t start;
    size_t count;
    size_t i;

    start = mpe->kind == SIXEL_ICC_MPE_MATRIX ? 12u : 28u;
    count = (mpe->length - start) / 4u;
    if (!sixel_icc_parse_fits(budget, count, sizeof(*mpe->values), issue)) {
        return 0;
    }
    mpe->values = (double *)sixel_icc_parse_alloc(
        budget, count * sizeof(*mpe->values), issue);
    if (mpe->values == NULL) {
        return 0;
    }
    for (i = 0u; i < count; ++i) {
        if (!sixel_icc_mpe_read_float(data + start + 4u * i, &mpe->values[i])) {
            sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
            return 0;
        }
    }
    return 1;
}

static int
sixel_icc_parse_mpet(unsigned char const *data, size_t length,
                      uint16_t input_channels, uint16_t output_channels,
                      sixel_icc_mpet_t *out, sixel_icc_parse_budget_t *budget,
                      sixel_icc_issue_t *issue)
{
    sixel_icc_mpe_t elements[SIXEL_ICC_MAX_MPE_ELEMENTS];
    sixel_icc_mpe_range_t ranges[SIXEL_ICC_MAX_MPE_ELEMENTS];
    sixel_icc_mpet_t parsed;
    sixel_icc_mpe_t *mpe;
    sixel_icc_issue_t element_issue;
    size_t count;
    size_t table_end;
    size_t element_bytes;
    size_t i;
    uint16_t channels;
    int unsupported_element;
    int ok;

    if (length < 16u || sixel_icc_read_be32(data + 4u) != 0u ||
        sixel_icc_read_be16(data + 8u) != input_channels ||
        sixel_icc_read_be16(data + 10u) != output_channels) {
        goto malformed;
    }
    count = sixel_icc_read_be32(data + 12u);
    if (count == 0u || count > (length - 16u) / 8u) {
        goto malformed;
    }
    if (count > SIXEL_ICC_MAX_MPE_ELEMENTS) {
        budget->limit = "mpe-count";
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_RESOURCE_LIMIT);
        return 0;
    }

    table_end = 16u + count * 8u;
    channels = input_channels;
    unsupported_element = 0;
    /* Keep the complete scratch array initialized across header parsing
     * and the later transfer of ownership to the allocated descriptors. */
    memset(elements, 0, sizeof(elements));
    for (i = 0u; i < count; ++i) {
        mpe = &elements[i];
        mpe->offset = sixel_icc_read_be32(data + 16u + i * 8u);
        mpe->length = sixel_icc_read_be32(data + 20u + i * 8u);
        if (mpe->offset < table_end || mpe->offset % 4u != 0u ||
            mpe->offset > length || mpe->length > length - mpe->offset ||
            mpe->length < 12u) {
            goto malformed;
        }
        element_issue = sixel_icc_parse_mpe_header(data + mpe->offset, mpe);
        if (element_issue == SIXEL_ICC_ISSUE_MALFORMED ||
            mpe->input_channels != channels) {
            goto malformed;
        }
        unsupported_element |= element_issue == SIXEL_ICC_ISSUE_UNSUPPORTED ||
            mpe->input_channels > SIXEL_ICC_MPE_MAX_CHANNELS ||
            mpe->output_channels > SIXEL_ICC_MPE_MAX_CHANNELS;
        channels = mpe->output_channels;
        ranges[i].offset = mpe->offset;
        ranges[i].length = mpe->length;
        ranges[i].index = (uint16_t)i;
    }
    if (channels != output_channels) {
        goto malformed;
    }

    if (!sixel_icc_order_mpe_ranges(ranges, count, table_end)) {
        goto malformed;
    }
    if (unsupported_element) {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
        return 0;
    }

    element_bytes = count * sizeof(*elements);
    /* Bound the encoded copy before adding the small descriptor array. */
    if (!sixel_icc_parse_fits(budget, length, 1u, issue) ||
        !sixel_icc_parse_fits(budget, length + element_bytes, 1u, issue)) {
        return 0;
    }
    parsed.elements = (sixel_icc_mpe_t *)sixel_icc_parse_alloc(
        budget, element_bytes, issue);
    if (parsed.elements == NULL) {
        return 0;
    }
    parsed.data = (unsigned char *)sixel_icc_parse_alloc(budget, length, issue);
    if (parsed.data == NULL) {
        free(parsed.elements);
        return 0;
    }
    memcpy(parsed.elements, elements, element_bytes);
    memcpy(parsed.data, data, length);
    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.element_count = count;
    parsed.shared_payload = 0;
    for (i = 0u; i < count; ++i) {
        mpe = &parsed.elements[ranges[i].index];
        if (i != 0u && ranges[i].offset == ranges[i - 1u].offset) {
            mpe->payload_element = parsed.elements[ranges[i - 1u].index]
                .payload_element;
            continue;
        }
        mpe->payload_element = ranges[i].index;
        if (mpe->kind == SIXEL_ICC_MPE_CURVE_SET) {
            ok = sixel_icc_parse_mpe_curves(data + mpe->offset, mpe,
                                            budget, issue);
        } else if (mpe->kind == SIXEL_ICC_MPE_MATRIX ||
                   mpe->kind == SIXEL_ICC_MPE_CLUT) {
            ok = sixel_icc_parse_mpe_values(data + mpe->offset, mpe,
                                            budget, issue);
        } else {
            continue;
        }
        if (!ok) {
            sixel_icc_mpet_destroy(&parsed);
            return 0;
        }
    }
    *out = parsed;
    return 1;

malformed:
    sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
    return 0;
}

static void
sixel_icc_parse_mpet_slot(unsigned char const *profile_data,
                           size_t profile_size, char const tag_name[4],
                           uint16_t input_channels, uint16_t output_channels,
                           sixel_icc_mpet_t *out, sixel_icc_mpet_cache_t *cache,
                           sixel_icc_parse_budget_t *budget,
                           sixel_icc_issue_t *issue)
{
    size_t retained_before;
    size_t offset;
    size_t length;
    size_t i;
    sixel_icc_mpet_t const *owner;
    sixel_icc_issue_t failure;
    char const *reason;

    if (budget->limit != NULL ||
        !sixel_icc_find_tag(profile_data, profile_size, tag_name, &offset,
                             &length, issue)) {
        return;
    }

    /* Match both bounds and endpoint widths before borrowing a previously
     * validated payload. A reverse tag may require a different shape. */
    for (i = 0u; i < cache->count; ++i) {
        owner = cache->tags[i].owner;
        if (offset == cache->tags[i].offset &&
            length == cache->tags[i].length &&
            input_channels == owner->input_channels &&
            output_channels == owner->output_channels) {
            *out = *owner;
            out->shared_payload = 1;
            return;
        }
    }

    retained_before = budget->retained;
    failure = SIXEL_ICC_ISSUE_NONE;
    if (length < 4u || memcmp(profile_data + offset, "mpet", 4u) != 0) {
        /* Other payloads in slots 0–2 belong to the compatibility parser. */
        if (tag_name[3] != '3') {
            return;
        }
        failure = length < 4u ? SIXEL_ICC_ISSUE_MALFORMED
            : SIXEL_ICC_ISSUE_UNSUPPORTED;
    } else if (sixel_icc_parse_mpet(profile_data + offset, length,
                                    input_channels, output_channels, out,
                                    budget, &failure)) {
        /* At most one owner is inserted per D2B/B2D slot. */
        cache->tags[cache->count].offset = offset;
        cache->tags[cache->count].length = length;
        cache->tags[cache->count].owner = out;
        ++cache->count;
        return;
    }
    budget->retained = retained_before;
    if (failure == SIXEL_ICC_ISSUE_UNSUPPORTED) {
        reason = length >= 4u &&
            memcmp(profile_data + offset, "mpet", 4u) == 0
            ? "unsupported-mpet-element" : "unsupported-pipeline-type";
    } else {
        reason = "invalid-or-unsupported-pipeline";
    }
    sixel_icc_record_issue(issue, failure);
    loader_trace_message(
        "LSXCMS1|engine=builtin|event=pipeline-rejected|tag=%.4s|reason=%s",
        tag_name, reason);
}

static int
sixel_icc_parse_pipeline(unsigned char const *profile_data,
                          size_t profile_size, char const tag_name[4],
                          uint8_t input_channels, uint8_t output_channels,
                          sixel_icc_lut_t *lut,
                          sixel_icc_mab_pipeline_t *mab,
                          sixel_icc_parse_budget_t *budget,
                          sixel_icc_issue_t *issue)
{
    size_t retained_before;
    size_t offset;
    size_t length;
    unsigned char const *payload;
    sixel_icc_issue_t failure;
    int parsed_lut;

    if (budget->limit != NULL ||
        !sixel_icc_find_tag(profile_data, profile_size, tag_name, &offset,
                             &length, issue)) {
        return 0;
    }
    payload = profile_data + offset;
    retained_before = budget->retained;
    failure = SIXEL_ICC_ISSUE_NONE;
    if (length < 4u) {
        failure = SIXEL_ICC_ISSUE_MALFORMED;
    } else if (sixel_icc_is_mpet_slot(tag_name) &&
               memcmp(payload, "mpet", 4u) == 0) {
        /* Standard floating-point slots are parsed separately. */
        return 0;
    } else if (memcmp(payload, "mAB ", 4u) == 0 ||
               memcmp(payload, "mBA ", 4u) == 0) {
        if (sixel_icc_parse_mab_tag_data(payload, length, mab,
                                         budget, &failure) &&
            mab->input_channels == input_channels &&
            mab->output_channels == output_channels) {
            return 1;
        }
        sixel_icc_mab_pipeline_destroy(mab);
    } else if (memcmp(payload, "mft1", 4u) == 0 ||
               memcmp(payload, "mft2", 4u) == 0) {
        parsed_lut = payload[3] == '1'
            ? sixel_icc_parse_lut8_tag(payload, length, lut, budget, &failure)
            : sixel_icc_parse_lut16_tag(payload, length, lut, budget, &failure);
        if (parsed_lut &&
            lut->input_channels == input_channels &&
            lut->output_channels == output_channels) {
            return 1;
        }
        sixel_icc_lut_destroy(lut);
    } else {
        failure = SIXEL_ICC_ISSUE_UNSUPPORTED;
    }

    budget->retained = retained_before;
    if (failure == SIXEL_ICC_ISSUE_NONE) {
        failure = SIXEL_ICC_ISSUE_MALFORMED;
    }
    sixel_icc_record_issue(issue, failure);
    loader_trace_message(
        "LSXCMS1|engine=builtin|event=pipeline-rejected|tag=%.4s"
        "|reason=invalid-or-unsupported-pipeline", tag_name);
    return 0;
}

static int
sixel_icc_parse_a2b_slot(unsigned char const *profile_data,
                         size_t profile_size, unsigned int slot,
                         uint8_t input_channels, uint8_t output_channels,
                         int is_rgb_or_gray,
                         sixel_icc_profile_t *parsed,
                         sixel_icc_parse_budget_t *budget,
                         sixel_icc_issue_t *issue)
{
    sixel_icc_lut_t *lut;

    lut = &parsed->a2b_lut[slot];
    if (!sixel_icc_parse_pipeline(profile_data, profile_size,
            sixel_icc_a2b_tag_names[slot], input_channels, output_channels,
            lut, &parsed->a2b_mab[slot], budget, issue)) {
        return 0;
    }
    if (is_rgb_or_gray && lut->kind == SIXEL_ICC_LUT_MFT1) {
        lut->kind = SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0;
    } else if (is_rgb_or_gray && lut->kind == SIXEL_ICC_LUT_MFT2) {
        lut->kind = SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0;
    }
    return 1;
}

static int
sixel_icc_parse_b2a_slot(unsigned char const *profile_data,
                         size_t profile_size, unsigned int slot,
                         uint8_t input_channels, uint8_t output_channels,
                         sixel_icc_profile_t *parsed,
                         sixel_icc_parse_budget_t *budget,
                         sixel_icc_issue_t *issue)
{
    return sixel_icc_parse_pipeline(profile_data, profile_size,
        sixel_icc_b2a_tag_names[slot], input_channels, output_channels,
        &parsed->b2a_lut[slot], &parsed->b2a_mab[slot], budget, issue);
}

static int
sixel_icc_parse_d2b_slot(unsigned char const *profile_data,
                         size_t profile_size, unsigned int slot,
                         uint8_t input_channels, uint8_t output_channels,
                         sixel_icc_profile_t *parsed,
                         sixel_icc_parse_budget_t *budget,
                         sixel_icc_issue_t *issue)
{
    return sixel_icc_parse_pipeline(profile_data, profile_size,
        sixel_icc_d2b_tag_names[slot], input_channels, output_channels,
        &parsed->d2b_lut[slot], &parsed->d2b_mab[slot], budget, issue);
}

static int
sixel_icc_parse_b2d_slot(unsigned char const *profile_data,
                         size_t profile_size, unsigned int slot,
                         uint8_t input_channels, uint8_t output_channels,
                         sixel_icc_profile_t *parsed,
                         sixel_icc_parse_budget_t *budget,
                         sixel_icc_issue_t *issue)
{
    return sixel_icc_parse_pipeline(profile_data, profile_size,
        sixel_icc_b2d_tag_names[slot], input_channels, output_channels,
        &parsed->b2d_lut[slot], &parsed->b2d_mab[slot], budget, issue);
}

/* Gate header fields that identify the transform model. Some embedded
 * profiles omit these fields altogether. A zero field retains the legacy
 * interpretation, while an explicit declaration still has to be usable. */
static sixel_icc_issue_t
sixel_icc_check_profile_header(unsigned char const *data, int *lut_only)
{
    unsigned char const *profile_class;
    uint32_t version;
    uint32_t signature;
    unsigned int major;
    unsigned int revision;
    sixel_icc_issue_t issue;
    char const *reason;

    version = sixel_icc_read_be32(data + 8u);
    profile_class = data + 12u;
    signature = sixel_icc_read_be32(data + 36u);
    major = data[8];
    revision = data[9];
    issue = SIXEL_ICC_ISSUE_MALFORMED;
    if (signature != 0u && memcmp(data + 36u, "acsp", 4u) != 0) {
        reason = "invalid-profile-signature";
    } else if ((version & 0xffffu) != 0u ||
               (major >> 4) > 9u || (major & 0x0fu) > 9u ||
               (revision >> 4) > 9u || (revision & 0x0fu) > 9u) {
        reason = "invalid-profile-version";
    } else if (version != 0u && major != 2u && major != 4u) {
        issue = SIXEL_ICC_ISSUE_UNSUPPORTED;
        reason = "unsupported-profile-version";
    } else if (memcmp(profile_class, "spac", 4u) == 0) {
        *lut_only = 1;
        return SIXEL_ICC_ISSUE_NONE;
    } else if (sixel_icc_read_be32(profile_class) == 0u ||
               memcmp(profile_class, "scnr", 4u) == 0 ||
               memcmp(profile_class, "mntr", 4u) == 0 ||
               memcmp(profile_class, "prtr", 4u) == 0) {
        return SIXEL_ICC_ISSUE_NONE;
    } else {
        issue = SIXEL_ICC_ISSUE_UNSUPPORTED;
        reason = "unsupported-profile-class";
    }

    loader_trace_message(
        "LSXCMS1|engine=builtin|event=profile-rejected|reason=%s", reason);
    return issue;
}

int
sixel_icc_parse_profile_ex(void const *data, size_t length,
                            sixel_icc_profile_t *out_profile,
                            sixel_icc_issue_t *out_issue)
{
    unsigned char const *profile_data;
    size_t profile_size;
    size_t tag_count;
    unsigned char const *color_space;
    unsigned char const *pcs;
    sixel_icc_profile_t parsed;
    sixel_icc_parse_budget_t budget;
    size_t retained_before_curves;
    sixel_icc_mpet_cache_t mpet_cache;
    sixel_icc_issue_t local_issue;
    sixel_icc_issue_t *issue;

    size_t rxyz_offset;
    size_t rxyz_length;
    size_t gxyz_offset;
    size_t gxyz_length;
    size_t bxyz_offset;
    size_t bxyz_length;
    size_t rtrc_offset;
    size_t rtrc_length;
    size_t gtrc_offset;
    size_t gtrc_length;
    size_t btrc_offset;
    size_t btrc_length;
    size_t wtpt_offset;
    size_t wtpt_length;
    size_t ktrc_offset;
    size_t ktrc_length;
    int has_forward_mpet;
    int has_reverse_mpet;
    double rxyz[3];
    double gxyz[3];
    double bxyz[3];
    unsigned int has_forward_slot;
    int has_reverse_slot;
    int has_matrix_tags;
    int has_white_tag;
    unsigned int slot;
    uint16_t device_channels;
    size_t i;

    budget.remaining = SIXEL_ICC_MAX_PARSE_BYTES;
    budget.retained = 0u;
    budget.limit = NULL;
    local_issue = SIXEL_ICC_ISSUE_NONE;
    issue = &local_issue;
    if (out_issue != NULL) {
        *out_issue = SIXEL_ICC_ISSUE_MALFORMED;
    }
    if (data == NULL || length < 132u || out_profile == NULL) {
        return 0;
    }

    sixel_icc_profile_reset(&parsed);
    memset(rxyz, 0, sizeof(rxyz));
    memset(gxyz, 0, sizeof(gxyz));
    memset(bxyz, 0, sizeof(bxyz));
    has_forward_slot = 0;
    has_reverse_slot = 0;
    has_matrix_tags = 0;
    rxyz_offset = 0u;
    rxyz_length = 0u;
    gxyz_offset = 0u;
    gxyz_length = 0u;
    bxyz_offset = 0u;
    bxyz_length = 0u;
    rtrc_offset = 0u;
    rtrc_length = 0u;
    gtrc_offset = 0u;
    gtrc_length = 0u;
    btrc_offset = 0u;
    btrc_length = 0u;
    wtpt_offset = 0u;
    wtpt_length = 0u;
    ktrc_offset = 0u;
    ktrc_length = 0u;
    slot = 0u;

    profile_data = (unsigned char const *)data;
    profile_size = (size_t)sixel_icc_read_be32(profile_data + 0u);
    if (profile_size < 132u || profile_size > length) {
        return 0;
    }

    local_issue = sixel_icc_check_profile_header(profile_data,
                                                 &parsed.lut_only);
    if (local_issue != SIXEL_ICC_ISSUE_NONE) {
        if (out_issue != NULL) {
            *out_issue = local_issue;
        }
        return 0;
    }

    tag_count = (size_t)sixel_icc_read_be32(profile_data + 128u);
    if (tag_count > (profile_size - 132u) / 12u) {
        sixel_icc_trace_tags(profile_data, profile_size);
        return 0;
    }
    if (tag_count > SIXEL_ICC_MAX_TAGS) {
        loader_trace_message(
            "LSXCMS1|engine=builtin|event=profile-rejected"
            "|reason=resource-limit|limit=tag-count");
        if (out_issue != NULL) {
            *out_issue = SIXEL_ICC_ISSUE_RESOURCE_LIMIT;
        }
        return 0;
    }
    sixel_icc_trace_tags(profile_data, profile_size);

    /* Relative paths already describe PCS colors. Keep the media white
     * separately for the absolute intent, including for LUT profiles. */
    has_white_tag = sixel_icc_find_tag(profile_data, profile_size, "wtpt",
                                      &wtpt_offset, &wtpt_length, issue);
    if (has_white_tag &&
        !sixel_icc_parse_xyz_tag(profile_data, profile_size, wtpt_offset,
                                  wtpt_length, parsed.media_white_xyz)) {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
        has_white_tag = 0;
    }

    color_space = profile_data + 16u;
    pcs = profile_data + 20u;
    if (memcmp(pcs, "XYZ ", 4u) == 0) {
        parsed.pcs = SIXEL_ICC_PROFILE_PCS_XYZ;
    } else if (memcmp(pcs, "Lab ", 4u) == 0) {
        parsed.pcs = SIXEL_ICC_PROFILE_PCS_LAB;
    } else {
        loader_trace_message(
            "LSXCMS1|engine=builtin|event=profile-rejected"
            "|reason=unsupported-pcs");
        if (out_issue != NULL) {
            *out_issue = SIXEL_ICC_ISSUE_UNSUPPORTED;
        }
        return 0;
    }

    if (memcmp(color_space, "RGB ", 4u) == 0) {
        parsed.kind = SIXEL_ICC_PROFILE_KIND_RGB;
        device_channels = 3u;
    } else if (memcmp(color_space, "GRAY", 4u) == 0) {
        parsed.kind = SIXEL_ICC_PROFILE_KIND_GRAY;
        device_channels = 1u;
    } else if (memcmp(color_space, "CMYK", 4u) == 0) {
        parsed.kind = SIXEL_ICC_PROFILE_KIND_CMYK;
        device_channels = 4u;
    } else if (memcmp(color_space, "Lab ", 4u) == 0) {
        parsed.kind = SIXEL_ICC_PROFILE_KIND_LAB;
        device_channels = 3u;
    } else {
        sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_UNSUPPORTED);
        goto fail;
    }

    has_forward_mpet = 0;
    has_reverse_mpet = 0;
    mpet_cache.count = 0u;
    for (slot = 0u; slot < SIXEL_ICC_MPET_SLOT_COUNT &&
         budget.limit == NULL; ++slot) {
        sixel_icc_parse_mpet_slot(profile_data, profile_size,
            sixel_icc_d2b_tag_names[slot], device_channels, 3u,
            &parsed.d2b_mpet[slot], &mpet_cache, &budget, issue);
        has_forward_mpet |= parsed.d2b_mpet[slot].element_count != 0u;
    }
    for (slot = 0u; slot < SIXEL_ICC_MPET_SLOT_COUNT &&
         budget.limit == NULL; ++slot) {
        sixel_icc_parse_mpet_slot(profile_data, profile_size,
            sixel_icc_b2d_tag_names[slot], 3u, device_channels,
            &parsed.b2d_mpet[slot], &mpet_cache, &budget, issue);
        has_reverse_mpet |= parsed.b2d_mpet[slot].element_count != 0u;
    }
    if (budget.limit != NULL) {
        goto fail;
    }

    if (parsed.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        has_forward_slot = 0;
        has_reverse_slot = 0;
        has_matrix_tags = 0;

        for (slot = 0u; slot < SIXEL_ICC_A2B_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_a2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         1,
                                         &parsed, &budget, issue)) {
                has_forward_slot |= 1u << slot;
            }
            if (sixel_icc_parse_d2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_forward_slot |= 1u << slot;
            }
        }
        for (slot = 0u; slot < SIXEL_ICC_B2A_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_b2a_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue) ||
                sixel_icc_parse_b2d_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_reverse_slot = 1;
            }
        }

        if (budget.limit != NULL) {
            goto fail;
        }

        /* A saturation-only LUT does not provide a relative or perceptual
         * transform. Retain matrix/TRC for those intents when available. */
        if ((has_forward_slot & 3u) == 0u && !parsed.lut_only) {
            has_matrix_tags = sixel_icc_find_tag(profile_data,
                                                 profile_size,
                                                 "rXYZ",
                                                 &rxyz_offset,
                                                 &rxyz_length, issue)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "gXYZ",
                                      &gxyz_offset,
                                      &gxyz_length, issue)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "bXYZ",
                                      &bxyz_offset,
                                      &bxyz_length, issue)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "rTRC",
                                      &rtrc_offset,
                                      &rtrc_length, issue)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "gTRC",
                                      &gtrc_offset,
                                      &gtrc_length, issue)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "bTRC",
                                      &btrc_offset,
                                      &btrc_length, issue);
            if (!has_matrix_tags || parsed.pcs != SIXEL_ICC_PROFILE_PCS_XYZ) {
                if (has_forward_slot || has_forward_mpet || has_reverse_mpet) {
                    goto accepted;
                }
                goto fail;
            }

            if (!sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         rxyz_offset,
                                         rxyz_length,
                                         rxyz) ||
                !sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         gxyz_offset,
                                         gxyz_length,
                                         gxyz) ||
                !sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         bxyz_offset,
                                         bxyz_length,
                                         bxyz)) {
                sixel_icc_record_issue(issue, SIXEL_ICC_ISSUE_MALFORMED);
                if (has_forward_slot || has_forward_mpet || has_reverse_mpet) {
                    goto accepted;
                }
                goto fail;
            }

            retained_before_curves = budget.retained;
            if (!sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           rtrc_offset,
                                           rtrc_length,
                                           &parsed.curves[0], &budget, issue) ||
                !sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           gtrc_offset,
                                           gtrc_length,
                                           &parsed.curves[1], &budget, issue) ||
                !sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           btrc_offset,
                                           btrc_length,
                                           &parsed.curves[2], &budget, issue)) {
                if (budget.limit == NULL &&
                    (has_forward_slot || has_forward_mpet ||
                     has_reverse_mpet)) {
                    for (i = 0u; i < 3u; ++i) {
                        sixel_icc_curve_destroy(&parsed.curves[i]);
                    }
                    budget.retained = retained_before_curves;
                    goto accepted;
                }
                goto fail;
            }

            parsed.matrix_to_xyz_d50[0][0] = rxyz[0];
            parsed.matrix_to_xyz_d50[1][0] = rxyz[1];
            parsed.matrix_to_xyz_d50[2][0] = rxyz[2];

            parsed.matrix_to_xyz_d50[0][1] = gxyz[0];
            parsed.matrix_to_xyz_d50[1][1] = gxyz[1];
            parsed.matrix_to_xyz_d50[2][1] = gxyz[2];

            parsed.matrix_to_xyz_d50[0][2] = bxyz[0];
            parsed.matrix_to_xyz_d50[1][2] = bxyz[1];
            parsed.matrix_to_xyz_d50[2][2] = bxyz[2];
        }
    } else if (parsed.kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        has_forward_slot = 0;
        has_reverse_slot = 0;
        has_matrix_tags = 0;

        for (slot = 0u; slot < SIXEL_ICC_A2B_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_a2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         1u,
                                         3u,
                                         1,
                                         &parsed, &budget, issue)) {
                has_forward_slot |= 1u << slot;
            }
            if (sixel_icc_parse_d2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         1u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_forward_slot |= 1u << slot;
            }
        }
        for (slot = 0u; slot < SIXEL_ICC_B2A_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_b2a_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         1u,
                                         &parsed, &budget, issue) ||
                sixel_icc_parse_b2d_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         1u,
                                         &parsed, &budget, issue)) {
                has_reverse_slot = 1;
            }
        }

        if (budget.limit != NULL) {
            goto fail;
        }

        /* A saturation-only LUT leaves relative gray on kTRC. */
        if ((has_forward_slot & 3u) == 0u && !parsed.lut_only) {
            has_matrix_tags = has_white_tag
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "kTRC",
                                      &ktrc_offset,
                                      &ktrc_length, issue);
            if (!has_matrix_tags || parsed.pcs != SIXEL_ICC_PROFILE_PCS_XYZ) {
                if (has_forward_slot || has_forward_mpet || has_reverse_mpet) {
                    goto accepted;
                }
                goto fail;
            }

            retained_before_curves = budget.retained;
            if (!sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           ktrc_offset,
                                           ktrc_length,
                                           &parsed.curves[0], &budget, issue)) {
                if (budget.limit == NULL &&
                    (has_forward_slot || has_forward_mpet ||
                     has_reverse_mpet)) {
                    sixel_icc_curve_destroy(&parsed.curves[0]);
                    budget.retained = retained_before_curves;
                    goto accepted;
                }
                goto fail;
            }
        }
    } else if (parsed.kind == SIXEL_ICC_PROFILE_KIND_CMYK) {
        has_forward_slot = 0;
        has_reverse_slot = 0;
        for (slot = 0u; slot < SIXEL_ICC_A2B_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_a2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         4u,
                                         3u,
                                         0,
                                         &parsed, &budget, issue)) {
                has_forward_slot = 1;
            }
            if (sixel_icc_parse_d2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         4u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_forward_slot = 1;
            }
        }
        for (slot = 0u; slot < SIXEL_ICC_B2A_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_b2a_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         4u,
                                         &parsed, &budget, issue) ||
                sixel_icc_parse_b2d_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         4u,
                                         &parsed, &budget, issue)) {
                has_reverse_slot = 1;
            }
        }
        if (budget.limit != NULL) {
            goto fail;
        }

        if (!parsed.lut_only && !has_forward_slot &&
            !has_forward_mpet && !has_reverse_mpet) {
            goto fail;
        }
    } else if (parsed.kind == SIXEL_ICC_PROFILE_KIND_LAB) {
        has_forward_slot = 0;
        has_reverse_slot = 0;

        for (slot = 0u; slot < SIXEL_ICC_A2B_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_a2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         0,
                                         &parsed, &budget, issue) ||
                sixel_icc_parse_d2b_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_forward_slot = 1;
            }
        }
        for (slot = 0u; slot < SIXEL_ICC_B2A_SLOT_COUNT &&
             budget.limit == NULL; ++slot) {
            if (sixel_icc_parse_b2a_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue) ||
                sixel_icc_parse_b2d_slot(profile_data,
                                         profile_size,
                                         slot,
                                         3u,
                                         3u,
                                         &parsed, &budget, issue)) {
                has_reverse_slot = 1;
            }
        }
        if (!has_forward_slot && !has_reverse_slot &&
            !has_forward_mpet && !has_reverse_mpet) {
            goto fail;
        }
    }

    /* A ColorSpace profile encodes an explicit mapping between its
     * device-side encoding and PCS. Colorants and TRCs cannot supply it. */
    if (budget.limit != NULL ||
        (parsed.lut_only && !has_forward_slot && !has_reverse_slot &&
         !has_forward_mpet && !has_reverse_mpet)) {
        goto fail;
    }
accepted:
    /* Embedded v2 Lab-to-Lab ColorSpace profiles with a usable LUT
     * retain the D50 Lab fallback for other intents. */
    if (parsed.lut_only && profile_data[8] == 2u &&
        parsed.kind == SIXEL_ICC_PROFILE_KIND_LAB &&
        parsed.pcs == SIXEL_ICC_PROFILE_PCS_LAB) {
        parsed.lut_only = 0;
    }
    if (out_issue != NULL) {
        *out_issue = local_issue;
    }
    parsed.allocated_bytes = budget.retained;
    *out_profile = parsed;
    return 1;

fail:
    if (out_issue != NULL) {
        *out_issue = local_issue == SIXEL_ICC_ISSUE_NONE
            ? SIXEL_ICC_ISSUE_NO_PATH : local_issue;
    }
    if (budget.limit != NULL) {
        loader_trace_message(
            "LSXCMS1|engine=builtin|event=profile-rejected"
            "|reason=resource-limit|limit=%s", budget.limit);
    } else {
        loader_trace_message(
            "LSXCMS1|engine=builtin|event=profile-rejected"
            "|reason=no-usable-profile-path");
    }
    sixel_icc_profile_destroy(&parsed);
    return 0;
}

int
sixel_icc_parse_profile(void const *data, size_t length,
                        sixel_icc_profile_t *out_profile)
{
    return sixel_icc_parse_profile_ex(data, length, out_profile, NULL);
}

int
sixel_icc_parse_png_iccp(unsigned char const *png_data,
                         size_t png_size,
                         sixel_icc_profile_t *out_profile)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;

    if (png_data == NULL || out_profile == NULL) {
        return 0;
    }
    if (png_size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(png_data, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= png_size) {
        uint32_t chunk_length_u32;
        size_t chunk_length;
        size_t chunk_total;
        unsigned char const *chunk_type;
        unsigned char const *chunk_data;

        chunk_length_u32 = sixel_icc_read_be32(png_data + offset);
        chunk_length = (size_t)chunk_length_u32;
        chunk_total = chunk_length + 12u;
        if (chunk_total > png_size - offset) {
            return 0;
        }

        chunk_type = png_data + offset + 4u;
        chunk_data = png_data + offset + 8u;

        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            size_t name_len;
            size_t compressed_offset;
            unsigned char compression_method;
            unsigned char *decoded_profile;
            int decoded_length;
            int parsed;

            if (chunk_length < 3u) {
                return 0;
            }

            name_len = 0u;
            while (name_len < chunk_length && chunk_data[name_len] != 0u) {
                ++name_len;
            }
            if (name_len == chunk_length) {
                return 0;
            }

            compressed_offset = name_len + 1u;
            if (compressed_offset >= chunk_length) {
                return 0;
            }

            compression_method = chunk_data[compressed_offset];
            if (compression_method != 0u) {
                return 0;
            }

            ++compressed_offset;
            if (compressed_offset >= chunk_length) {
                return 0;
            }

            decoded_length = 0;
            decoded_profile = (unsigned char *)stbi_zlib_decode_malloc_guesssize_headerflag(
                (char const *)(chunk_data + compressed_offset),
                (int)(chunk_length - compressed_offset),
                16384,
                &decoded_length,
                1);
            if (decoded_profile == NULL || decoded_length <= 0) {
                stbi_image_free(decoded_profile);
                return 0;
            }

            parsed = sixel_icc_parse_profile(decoded_profile,
                                             (size_t)decoded_length,
                                             out_profile);
            stbi_image_free(decoded_profile);
            return parsed;
        }

        if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 0;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
