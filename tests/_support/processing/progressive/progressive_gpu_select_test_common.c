/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/progressive-select.h"
#include "src/palette-factory.h"

#define SELECT_GPU_COUNT 527
#define SELECT_GPU_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "GPU selection case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_gpu_strata_case(int mode)
{
    sixel_repair_gpu_t *gpu;
    sixel_repair_gpu_request_t request;
    sixel_repair_gpu_request_t invalid;
    sixel_repair_gpu_result_t result;
    sixel_repair_gpu_result_t zero;
    float source[SELECT_GPU_COUNT * 3];
    float retained[SELECT_GPU_COUNT * 3] = {0};
    unsigned char domain[SELECT_GPU_COUNT];
    unsigned char filter[SELECT_GPU_COUNT];
    size_t positions[SELECT_GPU_COUNT];
    size_t population;
    size_t first;
    size_t consumed;
    size_t sample;
    size_t begin;
    size_t finish;
    size_t mass;
    size_t i;
    size_t j;
    int ok;
    SIXELSTATUS status;

    gpu = NULL;
    ok = 0;
    population = consumed = sample = mass = 0;
    memset(&request, 0, sizeof(request));
    memset(&zero, 0, sizeof(zero));
    for (i = 0; i < SELECT_GPU_COUNT; ++i) {
        source[i * 3] = source[i * 3 + 1] = source[i * 3 + 2] = .5f;
        domain[i] = i % 7 != 0;
        filter[i] = i % 5 != 0;
        if (domain[i] && filter[i]) positions[population++] = i;
    }
    request.view.source = source;
    request.view.retained = retained;
    request.view.domain = request.view.valid = domain;
    request.view.count = request.view.width = SELECT_GPU_COUNT;
    request.view.height = 1;
    request.view.plane_width = SELECT_GPU_COUNT + 3;
    request.view.plane_height = 3;
    request.view.origin_x = 2;
    request.view.origin_y = 1;
    request.view.workspace = SIXEL_COLORSPACE_GAMMA;
    request.count = 73;
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.filter = filter;
    request.preselected = 1;
    request.sample_limit = SIXEL_REPAIR_GPU_SAMPLE_MAX;
    request.sample_total = 137;
    request.sample_population = population;
    SELECT_GPU_REQUIRE(sixel_repair_gpu_new(&gpu, NULL) == SIXEL_OK);
    if (mode == 183) {
        for (i = 0; i < 7; ++i) {
            invalid = request;
            if (i == 0) invalid.filter = NULL;
            if (i == 1) invalid.sample_total = (unsigned int)population + 1;
            if (i == 2) invalid.sample_total = 65537;
            if (i == 3) invalid.sample_population = SELECT_GPU_COUNT + 1;
            if (i == 4) invalid.sample_offset = population + 1;
            if (i == 5) invalid.sample_first = request.sample_total + 1;
            if (i == 6) invalid.sample_limit = 72;
            memset(&result, 0x5a, sizeof(result));
            SELECT_GPU_REQUIRE(sixel_repair_gpu_scan(gpu, &invalid,
                &result) == SIXEL_BAD_ARGUMENT);
            SELECT_GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        }
        ok = 1;
        goto end;
    }
    for (first = 0; first < SELECT_GPU_COUNT; first += request.count) {
        request.first = first;
        request.count = SELECT_GPU_COUNT - first;
        if (request.count > 73) request.count = 73;
        request.filter = filter + first;
        request.sample_offset = consumed;
        request.sample_first = (unsigned int)sample;
        status = sixel_repair_gpu_scan(gpu, &request, &result);
        if (status == SIXEL_FEATURE_ERROR) {
            ok = 77;
            goto end;
        }
        SELECT_GPU_REQUIRE(status == SIXEL_OK);
        for (j = 0; j < result.summary.sample_count; ++j, ++sample) {
            /* Literal whole-population strata, not the GPU implementation's
             * batch reductions, are the provenance and mass oracle. */
            begin = sample * population / request.sample_total;
            finish = (sample + 1) * population / request.sample_total;
            i = positions[begin + (finish - begin - 1) / 2];
            SELECT_GPU_REQUIRE(result.samples[j].position == i &&
                result.samples[j].plane_position == i +
                    SELECT_GPU_COUNT + 5 &&
                result.samples[j].population == finish - begin &&
                result.samples[j].error == .75f &&
                memcmp(result.samples[j].source, source + i * 3,
                    3 * sizeof(float)) == 0);
            mass += result.samples[j].population;
        }
        for (i = first; i < first + request.count; ++i) {
            consumed += domain[i] && filter[i];
        }
    }
    SELECT_GPU_REQUIRE(sample == request.sample_total && mass == population);
    ok = 1;
end:
    sixel_repair_gpu_free(gpu);
    return ok == 77 ? 77 : ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_progressive_gpu_selection_case(int mode)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_repair_gpu_t *gpu;
    sixel_repair_selection_t *selections[2] = {NULL, NULL};
    sixel_repair_selection_t *selection;
    sixel_repair_ladder_t ladders[2];
    sixel_repair_gpu_lookup_t probe;
    sixel_repair_gpu_match_t match;
    sixel_repair_work_t work;
    sixel_palette_entries_request_t bytes;
    sixel_palette_float32_entries_request_t floats;
    unsigned char black[3] = {0};
    float source[SELECT_GPU_COUNT * 3];
    float retained[SELECT_GPU_COUNT * 3] = {0};
    double errors[SELECT_GPU_COUNT];
    double priority[SELECT_GPU_COUNT];
    unsigned char domain[SELECT_GPU_COUNT];
    size_t positions[SELECT_GPU_COUNT];
    size_t population;
    size_t count;
    size_t quantum;
    size_t samples;
    size_t i;
    size_t pass;
    size_t calls;
    size_t begin;
    size_t finish;
    size_t position;
    size_t weight;
    double stop;
    double value;
    int done;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    palette = NULL;
    gpu = NULL;
    ok = 0;
    count = mode == 185 || mode == 186 ? 19 : SELECT_GPU_COUNT;
    samples = mode == 189 ? 137 : 257;
    quantum = mode == 185 || mode == 188 ? 1 : 73;
    stop = 0;
    population = 0;
    memset(ladders, 0, sizeof(ladders));
    for (i = 0; i < count; ++i) {
        source[i * 3] = mode == 186 ? .1f : .2f + (i % 11) * .06f;
        source[i * 3 + 1] = source[i * 3 + 2] = 0;
        if (mode == 188) source[i * 3] = FLT_MAX / 4;
        value = source[i * 3];
        errors[i] = value * value;
        domain[i] = i % 7 != 0;
        priority[i] = i % 5 == 0 ? 0 : i % 3 + 1;
        if (domain[i] && (mode != 187 || priority[i] > 0)) {
            positions[population++] = i;
        }
    }
    if (mode == 186) stop = errors[0] * (1 - 1e-9);
    SELECT_GPU_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL,
        NULL) == SIXEL_OK);
    SELECT_GPU_REQUIRE(sixel_repair_gpu_new(&gpu, allocator) == SIXEL_OK);
    probe.queries = probe.palette = retained;
    probe.count = probe.colors = 1;
    probe.policy = SIXEL_GPU_POLICY_FORCE;
    status = sixel_repair_gpu_lookup(gpu, &probe, &match);
    if (status == SIXEL_FEATURE_ERROR) {
        ok = 77;
        goto end;
    }
    SELECT_GPU_REQUIRE(status == SIXEL_OK);
    SELECT_GPU_REQUIRE(sixel_palette_create_from_factory(&palette,
        allocator) == SIXEL_OK);
    bytes.entries = black;
    bytes.colors = 1;
    bytes.depth = 3;
    floats.entries = retained;
    floats.colors = 1;
    floats.depth = 3 * sizeof(float);
    SELECT_GPU_REQUIRE(palette->vtbl->init_entries(palette, &bytes)
        == SIXEL_OK);
    SELECT_GPU_REQUIRE(palette->vtbl->init_entries_float32(palette, &floats)
        == SIXEL_OK);
    SELECT_GPU_REQUIRE(sixel_palette_prepare_output(palette,
        SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY,
        SIXEL_COLORSPACE_GAMMA) == SIXEL_OK);
    for (pass = 0; pass < 2; ++pass) {
        SELECT_GPU_REQUIRE(sixel_repair_selection_new(&selections[pass],
            source, errors, domain, count, palette, SIXEL_COLORSPACE_GAMMA,
            SIXEL_PALETTETYPE_RGB, stop, .4, mode != 189, samples, 8,
            allocator) == SIXEL_OK);
        selection = selections[pass];
        if (mode == 187) {
            selection->priority = priority;
            selection->priority_maximum = 3;
        }
        if (mode == 189) {
            ladders[pass].allowance = 2;
            selection->ladder = ladders + pass;
        }
        if (pass == 0) {
            selection->gpu = gpu;
            selection->gpu_policy = mode == 188 ? SIXEL_GPU_POLICY_AUTO
                : SIXEL_GPU_POLICY_FORCE;
            selection->gpu_view.source = source;
            selection->gpu_view.retained = retained;
            selection->gpu_view.domain = selection->gpu_view.valid = domain;
            selection->gpu_view.count = count;
            selection->gpu_view.width = (int)count;
            selection->gpu_view.height = 1;
            selection->gpu_view.plane_width = (int)count + 3;
            selection->gpu_view.plane_height = 3;
            selection->gpu_view.origin_x = 2;
            selection->gpu_view.origin_y = 1;
            selection->gpu_view.workspace = SIXEL_COLORSPACE_GAMMA;
        }
        done = 0;
        for (calls = 0; calls < 100000 && !done; ++calls) {
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = pass ? 100000 : quantum;
            SELECT_GPU_REQUIRE(sixel_repair_selection_advance(selection,
                &work, &done) == SIXEL_OK &&
                work.pixel_visits <= work.pixel_limit &&
                work.distance_evaluations <= work.distance_limit);
            if (mode == 188 && selection->gpu_policy ==
                    SIXEL_GPU_POLICY_OFF) {
                /* The transparent position zero already completed on GPU;
                 * failure at position one must preserve that exact cursor. */
                SELECT_GPU_REQUIRE(work.pixel_visits == quantum &&
                    work.distance_evaluations == quantum &&
                    selection->cursor == 1 && selection->gpu_used);
                ok = 1;
                goto end;
            }
        }
        SELECT_GPU_REQUIRE(done && (pass || selection->gpu_used));
    }
    SELECT_GPU_REQUIRE(selections[0]->sample_count ==
        selections[1]->sample_count &&
        selections[0]->protected_position ==
            selections[1]->protected_position &&
        selections[0]->threshold == selections[1]->threshold &&
        memcmp(selections[0]->selected, selections[1]->selected, count) == 0);
    SELECT_GPU_REQUIRE(memcmp(ladders, ladders + 1, sizeof(ladders[0])) == 0);
    for (i = 0; i < selections[0]->sample_count; ++i) {
        SELECT_GPU_REQUIRE(memcmp(selections[0]->samples + i,
            selections[1]->samples + i, sizeof(sixel_repair_sample_t)) == 0);
        if (mode == 189) continue;
        begin = i * population / selections[0]->sample_count;
        finish = (i + 1) * population / selections[0]->sample_count;
        position = positions[begin + (finish - begin - 1) / 2];
        weight = finish - begin;
        if (mode == 187) weight *= 1 + (size_t)(15 * priority[position] / 3);
        SELECT_GPU_REQUIRE(selections[0]->samples[i].position == position &&
            selections[0]->samples[i].population == weight &&
            selections[0]->samples[i].error == errors[position]);
    }
    ok = 1;
end:
    sixel_repair_selection_free(selections[0]);
    sixel_repair_selection_free(selections[1]);
    sixel_repair_gpu_free(gpu);
    if (palette != NULL) palette->vtbl->unref(palette);
    sixel_allocator_unref(allocator);
    return ok == 77 ? 77 : ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
