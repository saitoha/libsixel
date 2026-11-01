/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0005_range_fallback_atomicity(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;
    unsigned int iterations;
    double points[3] = { 70000, 0, 0 };
    double centers[3] = { 0.25, 0.5, 0.75 };
    double original[3] = { 0.25, 0.5, 0.75 };

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    iterations = 0;

    /* Unsupported extended coordinates must not clamp or partially publish
     * centers before the caller resumes CPU Lloyd from the original seeds. */
    request.points = points;
    request.centers = centers;
    request.point_count = 1;
    request.center_count = 1;
    request.max_iterations = 10;
    iterations = 99;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_FALSE || iterations != 99 ||
            memcmp(centers, original, sizeof(original)) != 0) {
        fprintf(stderr, "GPU range fallback changed output: %x\n", status);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
