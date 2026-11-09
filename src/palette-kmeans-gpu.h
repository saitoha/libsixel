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
    /* Optional final ownership; published only with a successful result. */
    unsigned int *membership;
    unsigned int point_count;
    unsigned int center_count;
    unsigned int max_iterations;
    unsigned int min_iterations;
    double threshold;
    int round_centers;
    /* Internal, one-shot failure selected by the test hook. */
    int test_failure;
} sixel_kmeans_gpu_request_t;

/* Availability probes the device, not shader compilation. Tests must fail,
 * rather than skip, when an available device cannot execute the kernels. */
SIXEL_INTERNAL_API int
sixel_kmeans_gpu_is_available(void);

/* Observe already published pipelines without starting or waiting for cold
 * initialization. AUTO must not charge compilation to the current request. */
SIXEL_INTERNAL_API int
sixel_kmeans_gpu_is_initialized(void);

/* Validate the FP32 arithmetic domain without probing a device. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmeans_gpu_check(sixel_kmeans_gpu_request_t const *request);

/* Construction work is independent of the palette-application threshold.
 * iterations is the guaranteed minimum, not the convergence-dependent cap.
 * AUTO also requires sixel_kmeans_gpu_is_initialized() to admit execution. */
SIXEL_INTERNAL_API int
sixel_kmeans_gpu_worthwhile(unsigned int points, unsigned int centers,
                             unsigned int iterations);

#if defined(HAVE_TESTS)
/* Fail exactly the numbered subsequent GPU attempt on the calling thread.
 * Stages: 1 initialization, 2 buffer allocation, 3 completed command discard.
 * A zero stage resets the hook; a failed attempt cannot poison later calls. */
SIXEL_INTERNAL_API void
sixel_kmeans_gpu_test_fail(int stage, unsigned int attempt);
#endif

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
