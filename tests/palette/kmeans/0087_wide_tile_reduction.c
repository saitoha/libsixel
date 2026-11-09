/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0087_wide_tile_reduction(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    double points[2051 * 3];
    double centers[1024 * 3];
    unsigned int membership[2051];
    unsigned int iterations, i;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    if (!sixel_kmeans_gpu_is_available()) return 77;
    memset(&request, 0, sizeof(request));
    /* Widely separated columns have independent means: the first three
     * columns contain y={0,1,2}, all others y={0,1}. Every sum and mean is
     * exact in FP32. Exercise more centers than lanes, centers absent from
     * individual tiles, signed coordinates and a three-point tail tile. */
    for (i = 0; i < 2051; ++i) {
        points[i * 3] = ((int)(i % 1024) - 512) * 4;
        points[i * 3 + 1] = i / 1024;
        points[i * 3 + 2] = -1;
    }
    memcpy(centers, points, sizeof(centers));
    request.points = points;
    request.centers = centers;
    request.membership = membership;
    request.point_count = 2051;
    request.center_count = 1024;
    request.max_iterations = request.min_iterations = 1;
    iterations = 0;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 1) return 1;
    for (i = 0; i < 1024; ++i) {
        if (centers[i * 3] != points[i * 3] ||
                centers[i * 3 + 1] != (i < 3 ? 1.0 : 0.5) ||
                centers[i * 3 + 2] != -1) return 1;
    }
    for (i = 0; i < 2051; ++i) {
        if (membership[i] != i % 1024) return 1;
    }
    return 0;
}
