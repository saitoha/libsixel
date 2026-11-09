/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_kmeans_contract.h"
#include "src/palette.h"
#include "src/palette-kmeans.h"
#include "src/palette-kmeans-gpu.h"
#include "src/palette-kmeans-assign.h"
#include "src/palette-common-snap.h"
#include "src/palette-common-merge.h"
#include "src/timer.h"
#include "src/loader.h"
#include "src/compat_stub.h"

/* This oracle scores final centers in double on exactly the same unbinned
 * samples. No decoded/output-space image metric substitutes for the loss. */
static double km_contract_sse(float const *points, unsigned int count,
                              float const *centers, unsigned int k)
{
    unsigned int i, c, channel;
    double best, distance, diff, total;

    total = 0.0;
    for (i = 0; i < count; ++i) {
        best = DBL_MAX;
        for (c = 0; c < k; ++c) {
            distance = 0.0;
            for (channel = 0; channel < 3; ++channel) {
                diff =
                    (double)points[i * 3 + channel] - centers[c * 3 + channel];
                distance += diff * diff;
            }
            if (distance < best)
                best = distance;
        }
        total += best;
    }
    return total;
}

static SIXELSTATUS km_contract_build(float const *points, unsigned int count,
                                     unsigned int k, int backend, int policy,
                                     int byte, unsigned int restarts,
                                     unsigned int seed, unsigned int limit,
                                     unsigned int minimum, float *centers,
                                     unsigned int *ncolors)
{
    sixel_palette_t *palette;
    sixel_allocator_t *allocator;
    unsigned char *bytes;
    sixel_palette_entries_view_t byte_view;
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t view;
    sixel_palette_snap_options_t snap;
    SIXELSTATUS status, finish;
    unsigned int i;

    palette = NULL;
    allocator = NULL;
    bytes = NULL;
    memset(&request, 0, sizeof(request));
    memset(&attempt, 0, sizeof(attempt));
    memset(&snap, 0, sizeof(snap));
    snap.policy_override = 1;
    snap.policy = SIXEL_PALETTE_SNAP_POLICY_NONE;
    sixel_set_palette_snap_override(&snap);
    sixel_set_kmeans_backend_override(1, backend);
    sixel_set_kmeans_seed_override(1, seed);
    sixel_set_kmeans_iter_override(1, limit);
    sixel_set_kmeans_miniter_override(1, minimum);
    sixel_set_kmeans_polish_iter_override(1, 0);
    sixel_set_kmeans_restarts_override(1, restarts);
    sixel_set_kmeans_init_type_override(1, SIXEL_PALETTE_KMEANS_INIT_NONE);
    sixel_set_kmeans_feedback_mode_override(1,
                                            SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kmeans_prune_policy_override(1,
                                           SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (status != SIXEL_OK)
        goto cleanup;
    status = sixel_palette_factory_new(allocator, (void **)&palette);
    if (status != SIXEL_OK)
        goto cleanup;
    request.data = (unsigned char const *)points;
    request.length = count * 3u * sizeof(float);
    request.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    request.requested_colors = k;
    request.method_for_rep = SIXEL_REP_AUTO;
    request.method_for_largest = SIXEL_LARGE_NORM;
    request.prefer_float32 = !byte;
    if (byte) {
        bytes = malloc((size_t)count * 3u);
        if (bytes == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (i = 0; i < count * 3u; ++i)
            bytes[i] = (unsigned char)floor(points[i] * 255 + 0.5);
        request.data = bytes;
        request.length = count * 3u;
        request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    }
    request.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.quality_mode = SIXEL_QUALITY_FULL;
    request.gpu_policy = policy;
    sixel_palette_binning_state_init(&attempt.binning,
                                     SIXEL_PALETTE_BINNING_NONE,
                                     SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_build_attempt_begin(palette, &attempt);
    if (status != SIXEL_OK)
        goto cleanup;
    status = palette->vtbl->generate(palette, &request);
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    if (status != SIXEL_OK)
        goto cleanup;
    if (finish != SIXEL_OK ||
        attempt.quantize_model != SIXEL_QUANTIZE_MODEL_KMEANS ||
        attempt.quantizer_retry_count != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    if (byte) {
        status = palette->vtbl->get_entries(palette, &byte_view);
        if (status != SIXEL_OK)
            goto cleanup;
        if (byte_view.entry_count == 0 || byte_view.entry_count > k ||
            byte_view.depth != 3) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        for (i = 0; i < byte_view.entry_count * 3u; ++i)
            centers[i] = byte_view.entries[i] / 255.0f;
        *ncolors = byte_view.entry_count;
        goto cleanup;
    }
    status = palette->vtbl->get_entries_float32(palette, &view);
    if (status != SIXEL_OK)
        goto cleanup;
    if (view.entry_count == 0 || view.entry_count > k ||
        view.depth != 3 * (int)sizeof(float)) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    for (i = 0; i < view.entry_count * 3u; ++i) {
        if (!isfinite(view.entries[i])) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
    }
    memcpy(centers, view.entries, view.entry_count * 3u * sizeof(float));
    *ncolors = view.entry_count;
cleanup:
    if (status != SIXEL_OK) {
        fprintf(stderr, "palette contract: %04x %s\n", status,
                sixel_helper_get_additional_message());
    }
    free(bytes);
    if (palette != NULL)
        palette->vtbl->unref(palette);
    sixel_allocator_unref(allocator);
    sixel_set_palette_snap_override(NULL);
    sixel_set_kmeans_backend_override(0, SIXEL_PALETTE_KMEANS_BACKEND_CPU);
    sixel_set_kmeans_seed_override(0, 0);
    sixel_set_kmeans_iter_override(0, 0);
    sixel_set_kmeans_miniter_override(0, 0);
    sixel_set_kmeans_polish_iter_override(0, 0);
    sixel_set_kmeans_restarts_override(0, 0);
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_feedback_mode_override(0,
                                            SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kmeans_prune_policy_override(0, SIXEL_PALETTE_KMEANS_PRUNE_AUTO);
    return status;
}

int kmgpu_contract_ties(int mode)
{
    double points[18] = {-3, 0, 0, -1, 0, 0, 0, 0, 0,
                         1,  0, 0, 2,  0, 0, 3, 0, 0};
    double centers[6] = {-3, 0, 0, 1, 0, 0};
    double distances[6], weights[2], sums[6], upper[6], lower[6];
    double matrix[12], half[2], pairs[4], groups[12];
    unsigned int membership[6] = {0, 1, 1, 1, 1, 1};
    unsigned int offsets[3] = {0, 1, 2}, center_groups[2] = {0, 1};
    sixel_kmeans_gpu_request_t request;
    unsigned int iterations, i;
    SIXELSTATUS status;

    memset(upper, 0, sizeof(upper));
    memset(lower, 0, sizeof(lower));
    memset(matrix, 0, sizeof(matrix));
    memset(groups, 0, sizeof(groups));
    sixel_kmeans_compute_half_center_distance_matrix(centers, 2, half, pairs);
    if (mode == 0) {
        sixel_kmeans_assign_samples(centers, 2, points, NULL, 6, membership,
                                    distances, weights, sums);
    } else if (mode == 1) {
        sixel_kmeans_assign_samples_hamerly(centers, 2, points, NULL, 6,
                                            membership, distances, weights,
                                            sums, upper, lower, half);
    } else if (mode == 2) {
        sixel_kmeans_assign_samples_elkan(centers, 2, points, NULL, 6,
                                          membership, distances, weights, sums,
                                          upper, lower, matrix, half, pairs);
    } else if (mode == 3) {
        sixel_kmeans_assign_samples_yinyang(
            centers, 2, points, NULL, 6, membership, distances, weights, sums,
            upper, lower, matrix, half, pairs, 2, offsets, center_groups,
            groups);
    } else {
        if (!sixel_kmeans_gpu_is_available())
            return 77;
        memset(&request, 0, sizeof(request));
        /* The first update moves center 1 from 0 to 1. Sample -1 then
         * becomes equidistant but must keep owner 1 on the second pass. */
        centers[3] = 0;
        request.points = points;
        request.centers = centers;
        request.membership = membership;
        request.point_count = 6;
        request.center_count = 2;
        request.max_iterations = 2;
        request.min_iterations = 2;
        iterations = 0;
        status = sixel_kmeans_gpu_refine(&request, &iterations);
        if (status != SIXEL_OK || iterations != 2 || centers[0] != -3 ||
            centers[3] != 1)
            return 1;
    }
    for (i = 0; i < 6; ++i) {
        if (membership[i] != (i == 0 ? 0U : 1U))
            return 1;
    }
    return 0;
}

int kmgpu_contract_numeric(int mode)
{
    double points[6] = {1, 0, 0, 2, 0, 0};
    double weights[2] = {1, 1};
    double centers[3] = {1, 0, 0};
    double saved[3];
    sixel_kmeans_gpu_request_t request;
    unsigned int iterations;
    SIXELSTATUS status;

    if (mode == 0)
        points[0] = 0x1p-60;
    if (mode == 1)
        weights[0] = 0x1p-30;
    if (mode == 2)
        points[3] = 1 + 0x1p-30;
    if (mode == 3)
        weights[0] = weights[1] = 0;
    if (mode == 4)
        weights[0] = 0x1p30;
    memset(&request, 0, sizeof(request));
    request.points = points;
    request.weights = weights;
    request.centers = centers;
    request.point_count = 2;
    request.center_count = 1;
    request.max_iterations = 2;
    memcpy(saved, centers, sizeof(saved));
    iterations = 99;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    return status != SIXEL_FALSE || iterations != 99 ||
           memcmp(saved, centers, sizeof(saved)) != 0;
}

int kmgpu_contract_degenerate(int mode)
{
    double points[24], weights[8], centers[24];
    unsigned int membership[8], iterations, i;
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;

    if (!sixel_kmeans_gpu_is_available())
        return 77;
    memset(points, 0, sizeof(points));
    memset(centers, 0, sizeof(centers));
    memset(&request, 0, sizeof(request));
    for (i = 0; i < 8; ++i) {
        weights[i] = mode == 1 && i % 2 ? 0 : 1;
        points[i * 3] = mode == 2 ? (double)(i % 2) : 0;
    }
    request.points = points;
    request.weights = weights;
    request.centers = centers;
    request.membership = membership;
    request.point_count = request.center_count = 8;
    request.max_iterations = request.min_iterations = 4;
    iterations = 0;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 4)
        return 1;
    for (i = 0; i < 24; ++i) {
        if (!isfinite(centers[i]) || centers[i] < 0 || centers[i] > 1)
            return 1;
    }
    for (i = 0; i < 8; ++i) {
        if (membership[i] >= 8)
            return 1;
    }
    return 0;
}

int kmgpu_contract_fault(int stage, int force, int mixed)
{
    float points[192], cpu[24], result[24], recovered[24];
    unsigned int i, cpu_count, count, recovered_count, restarts;
    SIXELSTATUS status;
    int ok;

    if (!sixel_kmeans_gpu_is_available())
        return 77;
    /* Binary fractions avoid numerical noise in the fallback equality
     * oracle, including mixed CPU/GPU restarts with equal objective scores. */
    for (i = 0; i < 192; ++i)
        points[i] = (float)(i % 4) / 4;
    restarts = mixed ? 3 : 1;
    status = km_contract_build(points, 64, 8, SIXEL_PALETTE_KMEANS_BACKEND_CPU,
                               SIXEL_GPU_POLICY_OFF, 1, restarts, 7, 4, 4, cpu,
                               &cpu_count);
    if (status != SIXEL_OK)
        return 1;
    sixel_kmeans_gpu_test_fail(stage, mixed ? 2 : 1);
    count = 99;
    status = km_contract_build(points, 64, 8, SIXEL_PALETTE_KMEANS_BACKEND_GPU,
                               force ? SIXEL_GPU_POLICY_FORCE
                                     : SIXEL_GPU_POLICY_AUTO,
                               1, restarts, 7, 4, 4, result, &count);
    ok = force ? status != SIXEL_OK && count == 99
               : status == SIXEL_OK && count == cpu_count &&
                     memcmp(cpu, result, count * 3 * sizeof(float)) == 0;
    status = km_contract_build(points, 64, 8, SIXEL_PALETTE_KMEANS_BACKEND_GPU,
                               SIXEL_GPU_POLICY_FORCE, 1, restarts, 7, 4, 4,
                               recovered, &recovered_count);
    sixel_kmeans_gpu_test_fail(0, 0);
    return !ok || status != SIXEL_OK || recovered_count != cpu_count ||
           memcmp(cpu, recovered, cpu_count * 3 * sizeof(float)) != 0;
}

int kmgpu_contract_workload(void)
{
    return sixel_kmeans_gpu_worthwhile(12287, 1024, 100) ||
           sixel_kmeans_gpu_worthwhile(1048576, 63, 100) ||
           sixel_kmeans_gpu_worthwhile(1048576, 1024, 11) ||
           sixel_kmeans_gpu_worthwhile(65535, 64, 16) ||
           !sixel_kmeans_gpu_worthwhile(65536, 64, 16) ||
           !sixel_kmeans_gpu_worthwhile(12288, 256, 24) ||
           !sixel_kmeans_gpu_worthwhile(270000, 64, 12) ||
           !sixel_kmeans_gpu_worthwhile(270000, 256, 24) ||
           sixel_kmeans_gpu_worthwhile(12288, 64, 12) ||
           sixel_kmeans_gpu_worthwhile(4096, 256, 24) ||
           sixel_kmeans_gpu_worthwhile(270000, 16, 4);
}

/* Exercise the production AUTO route, including resolved iteration bounds.
 * Each test runs in a fresh process. The shell checks the execution trace,
 * so a successful CPU fallback cannot masquerade as a warm GPU dispatch. */
int kmgpu_contract_auto(int mode)
{
    float *points;
    float centers[768];
    double warm_points[6] = {0, 0, 0, 1, 1, 1};
    double warm_center[3] = {0, 0, 0};
    sixel_kmeans_gpu_request_t request;
    unsigned int i, count, iterations;
    SIXELSTATUS status;
    int available;

    if (sixel_kmeans_gpu_is_initialized())
        return 1;
    available = sixel_kmeans_gpu_is_available();
    /* Merely probing the device must not create pipelines. */
    if (sixel_kmeans_gpu_is_initialized())
        return 1;
    if (mode == 1 && !available)
        return 77;
    if (mode != 0 && available) {
        memset(&request, 0, sizeof(request));
        request.points = warm_points;
        request.centers = warm_center;
        request.point_count = 2;
        request.center_count = 1;
        request.min_iterations = request.max_iterations = 1;
        status = sixel_kmeans_gpu_refine(&request, &iterations);
        if (status != SIXEL_OK || !sixel_kmeans_gpu_is_initialized())
            return 1;
    }
    points = malloc(12288u * 3u * sizeof(float));
    if (points == NULL)
        return 1;
    /* Binary fractions keep numeric-domain rejection out of this test.
     * The full, unbinned sample count passes the M/K/product floors. */
    for (i = 0; i < 12288u; ++i) {
        points[i * 3] = (float)(i % 32u) / 32;
        points[i * 3 + 1] = (float)((i / 32u) % 32u) / 32;
        points[i * 3 + 2] = (float)(i / 1024u) / 16;
    }
    status =
        km_contract_build(points, 12288, 256, SIXEL_PALETTE_KMEANS_BACKEND_AUTO,
                          SIXEL_GPU_POLICY_FORCE, 0, 1, 42, 24,
                          mode == 2 ? 1 : 24, centers, &count);
    free(points);
    /* AUTO must leave cold state untouched even under FORCE. */
    return status != SIXEL_OK ||
           (mode == 0 && sixel_kmeans_gpu_is_initialized());
}

/* A repository image supplies exactly the same full-frame, no-binning point
 * set to both production solvers. Timings include preprocessing and export;
 * the first Metal call is cold and later calls in this process are warm. */
static SIXELSTATUS km_contract_image_frame(sixel_frame_t *frame, void *context)
{
    float *points;
    float cpu[768], gpu[768], repeat[768];
    unsigned char const *pixels;
    unsigned int count, i, ncpu, ngpu, nrepeat, run, k, limit, seed;
    double cpu_sse, gpu_sse, cpu_ms, gpu_ms, warm_ms, start;
    SIXELSTATUS status;
    int format;

    count = (unsigned int)sixel_frame_get_width(frame) *
            (unsigned int)sixel_frame_get_height(frame);
    format = sixel_frame_get_pixelformat(frame);
    if (format != SIXEL_PIXELFORMAT_RGB888 &&
        format != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        format != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)
        return SIXEL_BAD_INPUT;
    points = malloc((size_t)count * 3 * sizeof(float));
    if (points == NULL)
        return SIXEL_BAD_ALLOCATION;
    pixels = sixel_frame_get_pixels(frame);
    for (i = 0; i < count * 3; ++i) {
        points[i] = format == SIXEL_PIXELFORMAT_RGB888
                        ? pixels[i] / 255.0f
                        : ((float const *)pixels)[i];
    }
    status = SIXEL_OK;
    for (run = 0; run < 3; ++run) {
        k = run == 0 ? 16 : run == 1 ? 64 : 256;
        limit = run == 0 ? 4 : run == 1 ? 12 : 24;
        seed = run == 0 ? 1 : run == 1 ? 7 : 42;
        start = sixel_timer_now();
        status = km_contract_build(
            points, count, k, SIXEL_PALETTE_KMEANS_BACKEND_CPU,
            SIXEL_GPU_POLICY_OFF, 0, 1, seed, limit, limit, cpu, &ncpu);
        cpu_ms = (sixel_timer_now() - start) * 1000;
        if (status != SIXEL_OK)
            break;
        start = sixel_timer_now();
        status = km_contract_build(
            points, count, k, SIXEL_PALETTE_KMEANS_BACKEND_GPU,
            SIXEL_GPU_POLICY_FORCE, 0, 1, seed, limit, limit, gpu, &ngpu);
        gpu_ms = (sixel_timer_now() - start) * 1000;
        if (status != SIXEL_OK)
            break;
        cpu_sse = km_contract_sse(points, count, cpu, ncpu);
        gpu_sse = km_contract_sse(points, count, gpu, ngpu);
        start = sixel_timer_now();
        status = km_contract_build(
            points, count, k, SIXEL_PALETTE_KMEANS_BACKEND_GPU,
            SIXEL_GPU_POLICY_FORCE, 0, 1, seed, limit, limit, repeat, &nrepeat);
        warm_ms = (sixel_timer_now() - start) * 1000;
        fprintf(stderr,
                "KMCAL|M=%u|K=%u|I=%u|seed=%u|cpu_sse=%.17g|"
                "gpu_sse=%.17g|relative=%.9g|cpu_ms=%.3f|gpu_ms=%.3f|"
                "warm_ms=%.3f|\n",
                count, k, limit, seed, cpu_sse, gpu_sse,
                cpu_sse > 0 ? gpu_sse / cpu_sse - 1 : gpu_sse, cpu_ms, gpu_ms,
                warm_ms);
        /* M3 Max calibration: max absolute relative SSE was 0.180185%.
         * The 0.25% gate adds 0.069815 percentage points of margin and
         * still requires exact zero when the CPU score is zero. This is
         * a fixture validation budget, not an all-input error bound. */
        if (!isfinite(gpu_sse) || gpu_sse > cpu_sse * 1.0025 ||
            status != SIXEL_OK || nrepeat != ngpu ||
            memcmp(gpu, repeat, ngpu * 3 * sizeof(float)) != 0) {
            status = SIXEL_LOGIC_ERROR;
            break;
        }
    }
    free(points);
    *(int *)context = status == SIXEL_OK;
    return status;
}

int kmgpu_contract_image(char const *filename)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char path[4096];
    char const *root;
    int ok;

    if (!sixel_kmeans_gpu_is_available())
        return 77;
    root = sixel_compat_getenv("TOP_SRCDIR");
    if (root == NULL)
        return 1;
    if (snprintf(path, sizeof(path), "%s/%s", root, filename) >=
        (int)sizeof(path))
        return 1;
    loader = NULL;
    ok = 0;
    status = sixel_loader_new(&loader, NULL);
    if (status != SIXEL_OK)
        return 1;
    sixel_loader_set_prefer_float32(loader, 0);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &ok);
    if (status == SIXEL_OK)
        status = sixel_loader_load_file(loader, path, km_contract_image_frame);
    sixel_loader_unref(loader);
    return status != SIXEL_OK || !ok;
}

int kmgpu_contract_numeric_fallback(int force)
{
    float points[12] = {0x1p-60f, 0, 0, 0x1p-59f, 0, 0,
                        0x1p-58f, 0, 0, 0x1p-57f, 0, 0};
    float cpu[6], result[6];
    unsigned int ncpu, nresult;
    SIXELSTATUS status;

    status = km_contract_build(points, 4, 2, SIXEL_PALETTE_KMEANS_BACKEND_CPU,
                               SIXEL_GPU_POLICY_OFF, 0, 1, 1, 4, 4, cpu, &ncpu);
    if (status != SIXEL_OK)
        return 1;
    nresult = 99;
    status = km_contract_build(points, 4, 2, SIXEL_PALETTE_KMEANS_BACKEND_GPU,
                               force ? SIXEL_GPU_POLICY_FORCE
                                     : SIXEL_GPU_POLICY_AUTO,
                               0, 1, 1, 4, 4, result, &nresult);
    if (force)
        return status == SIXEL_OK || nresult != 99;
    return status != SIXEL_OK || nresult != ncpu ||
           memcmp(cpu, result, ncpu * 3 * sizeof(float)) != 0;
}
