/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "palette-kmeans-gpu.h"
#include "compat_stub.h"
#include "options.h"
#include "timer.h"

#if defined(HAVE_METAL)
int sixel_kmeans_metal_is_available(void);
int sixel_kmeans_metal_is_initialized(void);
SIXELSTATUS sixel_kmeans_metal_refine(
    sixel_kmeans_gpu_request_t const *request, unsigned int *iterations);
#endif

int
sixel_kmeans_gpu_is_available(void)
{
#if defined(HAVE_METAL)
    return sixel_kmeans_metal_is_available();
#else
    return 0;
#endif
}

int
sixel_kmeans_gpu_is_initialized(void)
{
#if defined(HAVE_METAL)
    return sixel_kmeans_metal_is_initialized();
#else
    return 0;
#endif
}

SIXELSTATUS
sixel_kmeans_gpu_check(sixel_kmeans_gpu_request_t const *request)
{
    size_t index;
    double weight;
    double value;
    int positive;
    unsigned int channel;
    double minimum[3];
    double maximum[3];
    double minimum_weight;
    double total_weight;
    double extent;
    double magnitude;

    if (request == NULL || request->points == NULL ||
            request->centers == NULL || request->point_count == 0u ||
            request->center_count == 0u ||
            request->center_count > request->point_count ||
            request->max_iterations == 0u ||
            request->max_iterations > 116u ||
            request->min_iterations > request->max_iterations ||
            !isfinite(request->threshold) || request->threshold < 0.0) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Bound temporary storage and FP32 arithmetic before allocating buffers.
     * With these limits, even an unnormalized weighted moment stays below
     * 2^68 and a weighted squared distance below 2^69. Extended signed color
     * coordinates are allowed; unsupported ranges retain the CPU path. */
    if (request->point_count > 1048576u ||
            request->center_count > 1024u ||
            request->threshold > 4294967295.0 ||
            (request->threshold > 0.0 && request->threshold < FLT_MIN)) {
        return SIXEL_FALSE;
    }
    positive = 0;
    minimum_weight = DBL_MAX;
    total_weight = 0.0;
    for (channel = 0u; channel < 3u; ++channel) {
        minimum[channel] = DBL_MAX;
        maximum[channel] = -DBL_MAX;
    }
    for (index = 0u; index < request->point_count; ++index) {
        weight = request->weights == NULL ? 1.0 : request->weights[index];
        if (!isfinite(weight) || weight < 0.0 || weight > 4294967295.0 ||
                (weight > 0.0 && weight < 0x1p-16)) {
            return SIXEL_FALSE;
        }
        positive |= weight > 0.0;
        total_weight += weight;
        if (weight > 0.0 && weight < minimum_weight) {
            minimum_weight = weight;
        }
    }
    /* Do not silently lose a small positive population during reduction. */
    if (!positive || total_weight / minimum_weight > 0x1p24) {
        return SIXEL_FALSE;
    }
    for (index = 0u; index < (size_t)request->point_count * 3u; ++index) {
        value = request->points[index];
        if (!isfinite(value) || fabs(value) > 65536.0 ||
                (value != 0.0 && fabs(value) < 0x1p-24)) {
            return SIXEL_FALSE;
        }
        channel = (unsigned int)(index % 3u);
        if (value < minimum[channel]) minimum[channel] = value;
        if (value > maximum[channel]) maximum[channel] = value;
    }
    /* A narrow cloud at a large offset can collapse on conversion to FP32.
     * Keep at least six guard bits beyond the cloud's relative extent. The
     * lower magnitude/weight bounds also keep squared FP32 differences and
     * weighted residuals normal, even for adjacent representable inputs. */
    for (channel = 0u; channel < 3u; ++channel) {
        extent = maximum[channel] - minimum[channel];
        magnitude = fmax(fabs(maximum[channel]), fabs(minimum[channel]));
        if (extent > 0.0 && extent < 64.0 * FLT_EPSILON * magnitude) {
            return SIXEL_FALSE;
        }
    }
    for (index = 0u; index < (size_t)request->center_count * 3u; ++index) {
        value = request->centers[index];
        if (!isfinite(value) || fabs(value) > 65536.0 ||
                (value != 0.0 && fabs(value) < 0x1p-24)) {
            return SIXEL_FALSE;
        }
    }
    return SIXEL_OK;
}

int
sixel_kmeans_gpu_worthwhile(unsigned int points, unsigned int centers,
                             unsigned int iterations)
{
    /* M3 Max calibration excludes small or marginal wins and uses the
     * guaranteed iteration count, including primary polish. Cold pipeline
     * initialization is gated separately. This opt-in floor is empirical,
     * not a universal speed guarantee; see the GPU k-means reference. */
    return points >= 12288u && centers >= 64u && iterations >= 12u &&
        (double)points * centers * iterations >= 67108864.0;
}

#if defined(HAVE_TESTS)
#if defined(_MSC_VER)
#define SIXEL_KM_TEST_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define SIXEL_KM_TEST_TLS __thread
#else
#define SIXEL_KM_TEST_TLS
#endif
static SIXEL_KM_TEST_TLS int sixel_km_test_failure;
static SIXEL_KM_TEST_TLS unsigned int sixel_km_test_attempt;

void
sixel_kmeans_gpu_test_fail(int stage, unsigned int attempt)
{
    sixel_km_test_failure = stage;
    sixel_km_test_attempt = attempt;
}
#endif

SIXELSTATUS
sixel_kmeans_gpu_refine(sixel_kmeans_gpu_request_t const *request,
                        unsigned int *iterations)
{
    SIXELSTATUS status;
    sixel_kmeans_gpu_request_t attempt;
    double start;
    double checked;
    int trace;

    if (iterations == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    trace = sixel_diagnostics_trace_topic_is_enabled("kmeans_backend");
    start = trace ? sixel_timer_now() : 0.0;
    status = sixel_kmeans_gpu_check(request);
    if (status != SIXEL_OK) {
        return status;
    }
    checked = trace ? sixel_timer_now() : 0.0;
    attempt = *request;
    attempt.test_failure = 0;
#if defined(HAVE_TESTS)
    if (sixel_km_test_attempt > 0u && --sixel_km_test_attempt == 0u) {
        attempt.test_failure = sixel_km_test_failure;
        sixel_km_test_failure = 0;
    }
#endif
#if defined(HAVE_METAL)
    status = sixel_kmeans_metal_refine(&attempt, iterations);
#else
    (void)attempt;
    status = SIXEL_FALSE;
#endif
    if (trace) {
        fprintf(stderr, "LSXKMCALL1|M=%u|K=%u|check_ms=%.6f|"
            "metal_wall_ms=%.6f|status=0x%04x|\n",
            request->point_count, request->center_count,
            (checked - start) * 1000.0,
            (sixel_timer_now() - checked) * 1000.0, (unsigned int)status);
    }
    return status;
}
