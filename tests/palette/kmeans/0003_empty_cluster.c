/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0003_empty_cluster(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;
    unsigned int iterations;
    double points[12] = { 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10 };
    double centers[6] = { 0, 0, 0, 0, 0, 0 };
    double expected[6] = { 0, 0, 0, 10, 10, 10 };

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    iterations = 0;
    if (!sixel_kmeans_gpu_is_available()) {
        return 77;
    }

    /* Duplicate seeds leave the second cluster empty. Farthest-point repair
     * must move a bright sample and converge to both distinct input colors. */
    request.points = points;
    request.centers = centers;
    request.point_count = 4;
    request.center_count = 2;
    request.max_iterations = 10;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 3 ||
            memcmp(centers, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "GPU empty-cluster repair failed: %x\n", status);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
