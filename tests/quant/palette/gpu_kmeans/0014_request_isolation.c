/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0014_request_isolation(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    double points[3] = { 0.25, -0.5, 0.75 };
    double centers[3] = { 0, 0, 0 };
    unsigned int iterations;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    if (!sixel_kmeans_gpu_is_available()) {
        return 77;
    }
    memset(&request, 0, sizeof(request));
    request.points = points;
    request.centers = centers;
    request.point_count = 1;
    request.center_count = 1;
    request.max_iterations = 10;
    iterations = 0;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 2) {
        return EXIT_FAILURE;
    }
    /* A new request must not inherit the previous GPU convergence flag. */
    points[0] = -0.75;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    return status == SIXEL_OK && iterations == 2 &&
        memcmp(centers, points, sizeof(points)) == 0 ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
