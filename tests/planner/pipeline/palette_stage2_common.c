/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "palette_stage2_common.h"
#include "src/encoder.h"
#include "src/allocator.h"
#include "src/palette.h"
#include "src/gamut.h"
#include "src/palette-common-cover.h"

int
palette_stage2_request(sixel_allocator_t *allocator, int floating,
    int lookup, int shared, int endpoint,
    sixel_palette_build_request_t **request, sixel_frame_t **frame)
{
    sixel_encoder_t *encoder;
    sixel_palette_frame_state_t state;
    unsigned char *pixels;
    unsigned char bytes[6] = {32, 64, 128, 192, 224, 16};
    float native[6] = {0.125f, 0.25f, 0.5f, 0.75f, 1.125f, -0.125f};
    int format;
    int failed;
    SIXELSTATUS status;

    *request = NULL;
    *frame = NULL;
    encoder = NULL;
    failed = 1;
    format = floating ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
            SIXEL_FAILED(sixel_frame_new(frame, allocator))) {
        goto end;
    }
    pixels = sixel_allocator_malloc(encoder->allocator,
        floating ? sizeof(native) : sizeof(bytes));
    if (pixels == NULL) {
        goto end;
    }
    memcpy(pixels, floating ? (void *)native : (void *)bytes,
        floating ? sizeof(native) : sizeof(bytes));
    if (SIXEL_FAILED(sixel_frame_init(*frame, pixels, 2, 1,
                                       format, NULL, -1))) {
        goto end;
    }
    encoder->reqcolors = 3;
    encoder->quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    encoder->cover_policy_override = 1;
    encoder->cover_policy = SIXEL_PALETTE_COVER_OFF;
    encoder->lut_policy = lookup;
    encoder->lut_policy_override = 1;
    encoder->lut_policy_fhedt_resolution_override = 1;
    encoder->lut_policy_fhedt_resolution = 64;
    encoder->lut_policy_fhedt_shared_override = 1;
    encoder->lut_policy_fhedt_shared = shared;
    encoder->planner.working_colorspace_effective = SIXEL_COLORSPACE_GAMMA;
    encoder->planner.working_pixelformat = format;
    if (endpoint) {
        encoder->output_color_definition = SIXEL_NAMED_COLOR_COUNT;
        encoder->output_icc = sixel_cdef_name("display-p3");
        if (encoder->output_icc == NULL ||
                SIXEL_FAILED(sixel_gamut_new(&encoder->gamut_mapper,
                    encoder->output_icc, SIXEL_COLORSPACE_GAMMA,
                    SIXEL_GAMUT_EDGE_SEEKER, encoder->allocator))) {
            goto end;
        }
    }
    sixel_palette_frame_state_init(&state);
    sixel_palette_policy_resolution_init(&state.quantizer,
        encoder->quantize_model, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_palette_binning_state_init(&state.binning,
        SIXEL_PALETTE_BINNING_AUTO, SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    /* A predicted omission reservation must survive history/config changes. */
    state.delta_alpha_key = 1;
    status = sixel_palette_build_request_new(request, encoder, *frame,
        &state, 0, encoder->planner.working_pixelformat);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "request freeze: %x %s\n", status,
            sixel_helper_get_additional_message());
        goto end;
    }
    encoder->planner.working_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    encoder->planner.working_colorspace_effective = SIXEL_COLORSPACE_DIN99D;
    encoder->reqcolors = 256;
    encoder->working_colorspace = SIXEL_COLORSPACE_OKLAB;
    failed = 0;
end:
    sixel_encoder_unref(encoder);
    if (failed) {
        sixel_frame_unref(*frame);
        *frame = NULL;
    }
    return failed;
}

int
palette_stage2_snapshot(sixel_allocator_t *allocator, int floating,
    int lookup, int shared, sixel_palette_snapshot_t **snapshot)
{
    sixel_palette_build_request_t *request;
    sixel_palette_frame_state_t state;
    sixel_frame_t *frame;
    sixel_dither_t *built;
    SIXELSTATUS status;

    built = NULL;
    *snapshot = NULL;
    if (palette_stage2_request(allocator, floating, lookup, shared, 0,
            &request, &frame)) {
        return 1;
    }
    status = sixel_palette_build_request_execute(request, frame, &built,
                                                 &state, NULL, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_palette_snapshot_take(snapshot, request, &built, &state);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "snapshot build: %x %s\n", status,
            sixel_helper_get_additional_message());
    }
    sixel_dither_unref(built);
    sixel_frame_unref(frame);
    sixel_palette_build_request_free(request);
    return SIXEL_FAILED(status);
}

/* Deliberately exercise mutable compatibility access on application copies. */
static unsigned char *
palette_stage2_bytes(sixel_dither_t *dither)
{
    sixel_palette_entries_view_t view;

    if (SIXEL_FAILED(dither->palette->vtbl->get_entries(dither->palette,
                                                      &view))) {
        return NULL;
    }
    return (unsigned char *)view.entries;
}

int
palette_stage2_isolation(int floating)
{
    sixel_palette_snapshot_t *snapshot;
    sixel_dither_t *first;
    sixel_dither_t *second;
    sixel_dither_t *third;
    sixel_palette_float32_entries_view_t a;
    sixel_palette_float32_entries_view_t b;
    unsigned char expected[9];
    float expected_float[9];
    unsigned char pixel[3] = {32, 64, 128};
    float native_pixel[3] = {0.125f, 0.25f, 0.5f};
    sixel_index_t *indexes;
    int policies[3] = {SIXEL_LUT_POLICY_5BIT, SIXEL_LUT_POLICY_6BIT,
                       SIXEL_LUT_POLICY_VPTREE};
    int policy;
    int backend;
    int channel;
    int extended;
    int failed;

    /* Each mutable lookup family must start from its own palette/cache. */
    for (backend = 0; backend < 3; ++backend) {
        policy = policies[backend];
        snapshot = NULL;
        first = second = third = NULL;
        failed = 1;
        if (palette_stage2_snapshot(NULL, floating, policy, 1, &snapshot) ||
                SIXEL_FAILED(sixel_palette_snapshot_clone(snapshot, &first)) ||
                SIXEL_FAILED(sixel_palette_snapshot_clone(snapshot, &second))) {
            goto end;
        }
        if (first->ncolors != 3 || first->keycolor != 2 ||
                !first->keycolor_reserved || first->lookup_policy != NULL ||
                second->lookup_policy != NULL ||
                first->palette == second->palette ||
                (first->dither_policy != NULL &&
                 first->dither_policy == second->dither_policy)) {
            goto end;
        }
        memcpy(expected, palette_stage2_bytes(first), sizeof(expected));
        if (floating) {
            first->palette->vtbl->get_entries_float32(first->palette, &a);
            second->palette->vtbl->get_entries_float32(second->palette, &b);
            if (a.entries == NULL || b.entries == NULL ||
                    a.entries == b.entries || a.entry_count != 3) {
                goto end;
            }
            extended = 0;
            for (channel = 0; channel < 9; ++channel) {
                extended |= a.entries[channel] < 0.0f ||
                    a.entries[channel] > 1.0f;
            }
            if (!extended) {
                goto end;
            }
            memcpy(expected_float, a.entries, sizeof(expected_float));
            if (memcmp(a.entries, b.entries, sizeof(expected_float))) {
                goto end;
            }
        }
        sixel_dither_set_diffusion_type(first, SIXEL_DIFFUSE_NONE);
        indexes = sixel_dither_apply_palette(first,
            floating ? (unsigned char *)native_pixel : pixel, 1, 1);
        if (indexes == NULL) {
            goto end;
        }
        failed = indexes[0] >= 2 || second->lookup_policy != NULL;
        sixel_allocator_free(first->allocator, indexes);
        if (failed) {
            goto end;
        }
        memset(palette_stage2_bytes(first), 0, sizeof(expected));
        if (memcmp(palette_stage2_bytes(second), expected,
                   sizeof(expected)) ||
                SIXEL_FAILED(sixel_palette_snapshot_clone(snapshot, &third)) ||
                memcmp(palette_stage2_bytes(third), expected,
                       sizeof(expected)) ||
                third->apply_count != 0 ||
                third->pipeline_index_buffer != NULL ||
                third->pipeline_accumulation_pixels != NULL ||
                third->bluenoise_gradient_map != NULL) {
            goto end;
        }
        if (floating) {
            third->palette->vtbl->get_entries_float32(third->palette, &a);
            if (memcmp(a.entries, expected_float, sizeof(expected_float))) {
                goto end;
            }
        }
        failed = 0;
end:
        sixel_palette_snapshot_unref(snapshot);
        sixel_dither_unref(first);
        sixel_dither_unref(second);
        sixel_dither_unref(third);
        if (failed) {
            fprintf(stderr, "snapshot isolation: float=%d policy=%d\n",
                floating, policy);
            return 1;
        }
    }
    return 0;
}

/* A 64^3 FHEDT index table needs at least 256 KiB. Count allocations at
 * that boundary to distinguish retaining a table from silently rebuilding. */
static int palette_stage2_observe;
static int palette_stage2_large_allocations;
static int palette_stage2_live_allocations;
static int palette_stage2_fail_after = -1;

static int
palette_stage2_allocation(size_t size)
{
    if (palette_stage2_observe && size >= 262144u) {
        ++palette_stage2_large_allocations;
    }
    if (palette_stage2_fail_after == 0) {
        return 0;
    }
    if (palette_stage2_fail_after > 0) {
        --palette_stage2_fail_after;
    }
    return 1;
}

static void *
palette_stage2_malloc(size_t size)
{
    void *pointer;

    if (!palette_stage2_allocation(size)) {
        return NULL;
    }
    pointer = malloc(size);
    if (pointer != NULL) {
        ++palette_stage2_live_allocations;
    }
    return pointer;
}

static void *
palette_stage2_calloc(size_t count, size_t size)
{
    void *pointer;

    if (!palette_stage2_allocation(count * size)) {
        return NULL;
    }
    pointer = calloc(count, size);
    if (pointer != NULL) {
        ++palette_stage2_live_allocations;
    }
    return pointer;
}

static void *
palette_stage2_realloc(void *pointer, size_t size)
{
    void *result;
    int was_null;

    was_null = pointer == NULL;
    if (!palette_stage2_allocation(size)) {
        return NULL;
    }
    result = realloc(pointer, size);
    if (result != NULL && was_null) {
        ++palette_stage2_live_allocations;
    }
    return result;
}

static void
palette_stage2_free(void *pointer)
{
    if (pointer != NULL) {
        --palette_stage2_live_allocations;
    }
    free(pointer);
}

int
palette_stage2_fhedt(int floating)
{
    sixel_allocator_t *allocator;
    sixel_palette_snapshot_t *snapshot;
    sixel_dither_t *a;
    sixel_dither_t *b;
    unsigned char query[3] = {32, 64, 128};
    float native_query[3] = {0.125f, 0.25f, 0.5f};
    unsigned char const *pixel;
    int expected;
    int shared;
    int failed;
    int attempt;
    unsigned int references;
    SIXELSTATUS status;

    pixel = floating ? (unsigned char const *)native_query : query;
    for (shared = 0; shared <= 1; ++shared) {
        allocator = NULL;
        snapshot = NULL;
        a = b = NULL;
        failed = 1;
        if (SIXEL_FAILED(sixel_allocator_new(&allocator,
                palette_stage2_malloc, palette_stage2_calloc,
                palette_stage2_realloc, palette_stage2_free)) ||
                palette_stage2_snapshot(allocator, floating,
                    SIXEL_LUT_POLICY_FHEDT, shared, &snapshot)) {
            goto end;
        }
        palette_stage2_observe = 1;
        palette_stage2_large_allocations = 0;
        status = sixel_palette_snapshot_clone(snapshot, &a);
        palette_stage2_observe = 0;
        if (SIXEL_FAILED(status) || a->lookup_policy == NULL ||
                (palette_stage2_large_allocations == 0) != shared ||
                SIXEL_FAILED(sixel_palette_snapshot_clone(snapshot, &b)) ||
                a->lookup_policy == b->lookup_policy) {
            goto end;
        }
        expected = a->lookup_policy->vtbl->map_pixel(a->lookup_policy, pixel);
        if (expected < 0 || expected >= 2 ||
                b->lookup_policy->vtbl->map_pixel(b->lookup_policy, pixel)
                    != expected) {
            goto end;
        }
        sixel_dither_unref(a);
        a = NULL;
        /* Fail each allocation in the private clone, then retry from the
         * same immutable snapshot. No partial result may escape. */
        if (shared) {
            for (attempt = 0; attempt < 128; ++attempt) {
                references = allocator->ref;
                palette_stage2_fail_after = attempt;
                status = sixel_palette_snapshot_clone(snapshot, &a);
                palette_stage2_fail_after = -1;
                if (SIXEL_SUCCEEDED(status)) {
                    break;
                }
                if (a != NULL || allocator->ref != references) {
                    goto end;
                }
            }
            if (a == NULL) {
                goto end;
            }
        }
        sixel_palette_snapshot_unref(snapshot);
        snapshot = NULL;
        if (b->lookup_policy->vtbl->map_pixel(b->lookup_policy, pixel)
                != expected) {
            goto end;
        }
        failed = 0;
end:
        palette_stage2_observe = 0;
        palette_stage2_fail_after = -1;
        sixel_palette_snapshot_unref(snapshot);
        sixel_dither_unref(a);
        sixel_dither_unref(b);
        sixel_allocator_unref(allocator);
        if (failed || palette_stage2_live_allocations != 0) {
            fprintf(stderr, "FHEDT float=%d shared=%d live=%d\n",
                floating, shared, palette_stage2_live_allocations);
            return 1;
        }
    }
    return 0;
}
