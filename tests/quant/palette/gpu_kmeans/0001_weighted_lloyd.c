/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0001_weighted_lloyd(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;
    unsigned int iterations;
    double points[1028 * 3];
    double weights[1028];
    double colors[12] = { -2, -1, 0, -1, -1, 0, 4, 3, 2, 6, 3, 2 };
    double mass[4] = { 1, 3, 2, 2 };
    double centers[6] = { -2, -1, 0, 6, 3, 2 };
    double expected[6] = { -1.25, -1, 0, 5, 3, 2 };
    unsigned int i;

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    iterations = 0;
    if (!sixel_kmeans_gpu_is_available()) {
        return 77;
    }

    /* 257 copies cross three reduction tiles. All weighted sums and means
     * are exactly representable in FP32, with a large assignment margin. */
    for (i = 0; i < 1028; ++i) {
        memcpy(points + i * 3, colors + (i % 4) * 3, 3 * sizeof(double));
        weights[i] = mass[i % 4];
    }
    request.points = points;
    request.weights = weights;
    request.centers = centers;
    request.point_count = 1028;
    request.center_count = 2;
    request.max_iterations = 10;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 2 ||
            memcmp(centers, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "weighted GPU Lloyd: status=%x iterations=%u\n",
                status, iterations);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
