/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0004_iteration_floor(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;
    unsigned int iterations;
    double points[3] = { 0.5, 0.25, 0.125 };
    double centers[3] = { 0.5, 0.25, 0.125 };

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    iterations = 0;
    if (!sixel_kmeans_gpu_is_available()) {
        return 77;
    }

    /* Zero movement from the first pass must still honor miniter. */
    request.points = points;
    request.centers = centers;
    request.point_count = 1;
    request.center_count = 1;
    request.max_iterations = 10;
    request.min_iterations = 4;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 4 ||
            memcmp(centers, points, sizeof(points)) != 0) {
        fprintf(stderr, "GPU iteration floor failed: %x\n", status);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
