/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_KMEANS_GPU_H
#define LIBSIXEL_PALETTE_KMEANS_GPU_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This boundary consumes the weighted clustering coordinates, independent of
 * how sampling, color conversion and binning produced them. Centers are both
 * the initial seeds and the result. A failed attempt never changes them. */
typedef struct sixel_kmeans_gpu_request {
    double const *points;
    double const *weights;
    double *centers;
    unsigned int point_count;
    unsigned int center_count;
    unsigned int max_iterations;
    unsigned int min_iterations;
    double threshold;
    int round_centers;
} sixel_kmeans_gpu_request_t;

/* Availability probes the device, not shader compilation. Tests must fail,
 * rather than skip, when an available device cannot execute the kernels. */
SIXEL_INTERNAL_API int
sixel_kmeans_gpu_is_available(void);

/* SIXEL_FALSE means unsupported input or platform; other failures describe a
 * failed GPU attempt. Both leave centers and iterations untouched, allowing
 * the caller to restart the complete CPU iteration from the original seeds.
 * Success publishes finite centers and the number of executed iterations. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmeans_gpu_refine(sixel_kmeans_gpu_request_t const *request,
                        unsigned int *iterations);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEANS_GPU_H */
