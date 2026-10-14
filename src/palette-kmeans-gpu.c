/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stddef.h>

#include "palette-kmeans-gpu.h"
#include "compat_stub.h"

#if defined(HAVE_METAL)
int sixel_kmeans_metal_is_available(void);
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

SIXELSTATUS
sixel_kmeans_gpu_refine(sixel_kmeans_gpu_request_t const *request,
                        unsigned int *iterations)
{
    size_t index;
    double weight;
    double value;
    int positive;

    if (request == NULL || iterations == NULL || request->points == NULL ||
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
            request->threshold > 4294967295.0) {
        return SIXEL_FALSE;
    }
    positive = 0;
    for (index = 0u; index < request->point_count; ++index) {
        weight = request->weights == NULL ? 1.0 : request->weights[index];
        if (!isfinite(weight) || weight < 0.0 || weight > 4294967295.0 ||
                (weight > 0.0 && weight < FLT_MIN)) {
            return SIXEL_FALSE;
        }
        positive |= weight > 0.0;
    }
    if (!positive) {
        return SIXEL_FALSE;
    }
    for (index = 0u; index < (size_t)request->point_count * 3u; ++index) {
        value = request->points[index];
        if (!isfinite(value) || fabs(value) > 65536.0 ||
                (value != 0.0 && fabs(value) < FLT_MIN)) {
            return SIXEL_FALSE;
        }
    }
    for (index = 0u; index < (size_t)request->center_count * 3u; ++index) {
        value = request->centers[index];
        if (!isfinite(value) || fabs(value) > 65536.0 ||
                (value != 0.0 && fabs(value) < FLT_MIN)) {
            return SIXEL_FALSE;
        }
    }
#if defined(HAVE_METAL)
    return sixel_kmeans_metal_refine(request, iterations);
#else
    return SIXEL_FALSE;
#endif
}
