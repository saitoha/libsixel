/* SPDX-License-Identifier: MIT */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/progressive-apply.h"

#define APPLY_GPU_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "GPU application case %d, line %d\n", mode, \
            __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_gpu_lookup_case(int mode)
{
    sixel_repair_gpu_t *cache;
    sixel_repair_gpu_t *other;
    sixel_repair_gpu_lookup_t request;
    sixel_repair_gpu_lookup_t invalid;
    sixel_repair_gpu_match_t matches[3];
    sixel_repair_gpu_match_t saved[3];
    float queries[9] = { -.5f, 2, 4, .5f, 2, 4, 1, 2, 4 };
    float palette[9] = { -1, 2, 4, 0, 2, 4, 1, 2, 4 };
    unsigned int i;
    int rc;
    SIXELSTATUS status;

    cache = other = NULL;
    rc = 1;
    request.queries = queries;
    request.palette = palette;
    request.count = request.colors = 3;
    request.policy = SIXEL_GPU_POLICY_FORCE;
    memset(matches, 0xa5, sizeof(matches));
    memcpy(saved, matches, sizeof(saved));
    APPLY_GPU_REQUIRE(sixel_repair_gpu_new(&cache, NULL) == SIXEL_OK);
    if (mode == 168) {
        for (i = 0; i < 7; ++i) {
            invalid = request;
            switch (i) {
            case 0: invalid.count = 0; break;
            case 1: invalid.count = SIXEL_REPAIR_GPU_BATCH_MAX + 1; break;
            case 2: invalid.colors = 256; break;
            case 3: invalid.colors = 0; break;
            case 4: invalid.queries = NULL; break;
            case 5: invalid.palette = NULL; break;
            default: invalid.policy = 3; break;
            }
            APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &invalid,
                matches) == SIXEL_BAD_ARGUMENT);
            APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        }
        request.policy = SIXEL_GPU_POLICY_OFF;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_FALSE);
        APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        rc = 0;
        goto end;
    }
    status = sixel_repair_gpu_lookup(cache, &request, matches);
    if (status == SIXEL_FEATURE_ERROR) {
        request.policy = SIXEL_GPU_POLICY_AUTO;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_FALSE);
        APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        rc = mode == 171 ? 0 : 77;
        goto end;
    }
    APPLY_GPU_REQUIRE(status == SIXEL_OK);
    /* Extended, signed coordinates and exact ties must use ordered slots,
     * not a gamma-RGB interpretation or the source's nearest byte color. */
    APPLY_GPU_REQUIRE(matches[0].index == 0 && matches[0].distance == .25f &&
        matches[1].index == 1 && matches[1].distance == .25f &&
        matches[2].index == 2 && matches[2].distance == 0);
    memcpy(saved, matches, sizeof(saved));
    if (mode == 170) {
        APPLY_GPU_REQUIRE(sixel_repair_gpu_new(&other, NULL) == SIXEL_OK);
        palette[0] = -.5f;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(other, &request, matches)
            == SIXEL_OK && matches[0].distance == 0);
        palette[0] = -1;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_OK && memcmp(matches, saved, sizeof(matches)) == 0);
        /* Reusing an address never reuses the old palette or query data. */
        queries[0] = 1;
        palette[6] = 2;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_OK && matches[0].index == 1 &&
            matches[0].distance == 1);
    } else if (mode == 171) {
        queries[0] = FLT_MAX;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_BAD_INPUT);
        APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        request.policy = SIXEL_GPU_POLICY_AUTO;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_FALSE);
        APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        queries[0] = -.5f;
        palette[6] = NAN;
        request.policy = SIXEL_GPU_POLICY_FORCE;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_BAD_INPUT);
        APPLY_GPU_REQUIRE(memcmp(matches, saved, sizeof(matches)) == 0);
        palette[6] = 1;
        APPLY_GPU_REQUIRE(sixel_repair_gpu_lookup(cache, &request, matches)
            == SIXEL_OK && memcmp(matches, saved, sizeof(matches)) == 0);
    }
    rc = 0;
end:
    sixel_repair_gpu_free(cache);
    sixel_repair_gpu_free(other);
    return rc;
}

static SIXELSTATUS
apply_gpu_run(sixel_repair_application_t *application, size_t quantum,
    size_t totals[2])
{
    sixel_repair_work_t work;
    size_t calls;
    int done;
    SIXELSTATUS status;

    status = sixel_repair_application_begin(application);
    if (status != SIXEL_OK) return status;
    done = 0;
    totals[0] = totals[1] = 0;
    for (calls = 0; calls < 10000 && !done; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        status = sixel_repair_application_advance(application, &work, &done);
        if (status != SIXEL_OK) return status;
        if (work.pixel_visits > quantum || work.distance_evaluations >
                quantum) return SIXEL_LOGIC_ERROR;
        totals[0] += work.pixel_visits;
        totals[1] += work.distance_evaluations;
    }
    return done ? SIXEL_OK : SIXEL_LOGIC_ERROR;
}

int
test_progressive_gpu_application_case(int mode)
{
    sixel_repair_gpu_t *cache;
    sixel_repair_application_t application;
    sixel_palette_emitted_color_t colors[2];
    float source[527 * 3];
    double errors[527];
    double gains[2][527];
    unsigned char selected[527];
    unsigned char indices[2][527];
    unsigned char paint[2][527];
    unsigned int palette[2] = { 1, 0 };
    size_t totals[2][2];
    size_t painted;
    size_t i;
    int rc;
    SIXELSTATUS status;

    cache = NULL;
    rc = 1;
    memset(&application, 0, sizeof(application));
    memset(colors, 0, sizeof(colors));
    for (i = 0; i < 3; ++i) colors[1].working[i] = 1;
    for (i = 0; i < 527 * 3; ++i) source[i] = .25f;
    for (i = 0; i < 527; ++i) {
        selected[i] = i % 7 != 0;
        errors[i] = i % 5 ? .3 : .1875;
    }
    application.source = source;
    application.errors = errors;
    application.selected = selected;
    application.colors = colors;
    application.palette = palette;
    application.color_count = 2;
    application.count = mode == 174 || mode == 181 ? 17 : 527;
    application.width = 17;
    application.origin_x = 4;
    application.origin_y = 7;
    application.protected_position = 1;
    application.strength = mode == 172 ? 0 : 10;
    application.phase_x = 31;
    application.phase_y = 7;
    application.channel_rgb = 1;
    if (mode == 175) {
        /* A GPU float distance rounds this candidate to an apparent gain.
         * The CPU's unperturbed double distance must reject it everywhere. */
        application.strength = 0;
        for (i = 0; i < 527 * 3; ++i) source[i] = .3f;
        for (i = 0; i < 527; ++i) {
            errors[i] = sixel_repair_distance2(source + i * 3,
                colors[0].working);
        }
    } else if (mode == 181) {
        /* Finite CPU doubles can represent distances beyond GPU float range.
         * AUTO must retry on CPU without reusing the failed batch's budget. */
        application.strength = 0;
        for (i = 0; i < 527 * 3; ++i) source[i] = FLT_MAX;
        for (i = 0; i < 527; ++i) errors[i] = 1e79;
    }
    application.indices = indices[0];
    application.paint = paint[0];
    application.gains = gains[0];
    APPLY_GPU_REQUIRE(apply_gpu_run(&application, 100000, totals[0])
        == SIXEL_OK);
    painted = application.painted;
    APPLY_GPU_REQUIRE(sixel_repair_gpu_new(&cache, NULL) == SIXEL_OK);
    application.gpu = cache;
    application.gpu_policy = mode == 181 ? SIXEL_GPU_POLICY_AUTO
        : SIXEL_GPU_POLICY_FORCE;
    application.indices = indices[1];
    application.paint = paint[1];
    application.gains = gains[1];
    status = apply_gpu_run(&application, mode == 174 ? 1 :
        mode == 181 ? 7 : 100000, totals[1]);
    if (status == SIXEL_FEATURE_ERROR) {
        rc = 77;
        goto end;
    }
    APPLY_GPU_REQUIRE(status == SIXEL_OK &&
        application.gpu_used == (mode != 181));
    APPLY_GPU_REQUIRE(application.painted == painted);
    APPLY_GPU_REQUIRE(memcmp(paint[0], paint[1], application.count) == 0);
    APPLY_GPU_REQUIRE(memcmp(indices[0], indices[1], application.count) == 0);
    APPLY_GPU_REQUIRE(memcmp(gains[0], gains[1], application.count *
        sizeof(double)) == 0);
    if (mode == 181) {
        APPLY_GPU_REQUIRE(application.gpu_policy == SIXEL_GPU_POLICY_OFF &&
            totals[1][0] > totals[0][0] && totals[1][1] > totals[0][1]);
    } else {
        APPLY_GPU_REQUIRE(memcmp(totals[0], totals[1], sizeof(totals[0])) == 0);
    }
    if (mode == 181) {
        APPLY_GPU_REQUIRE(painted > 0 && paint[1][0] == 0);
    } else if (mode == 175) {
        APPLY_GPU_REQUIRE(painted == 0);
    } else {
        APPLY_GPU_REQUIRE(painted > 0 && paint[1][1] == 1 &&
            indices[1][1] == 1 && paint[1][0] == 0 && paint[1][5] == 0);
    }
    rc = 0;
end:
    sixel_repair_gpu_free(cache);
    return rc;
}
