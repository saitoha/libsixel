/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0086_repair_tile_ties(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    double points[1031 * 3];
    double centers[8 * 3];
    double expected[8] = {0, 8, -8, 8, 7, -7, 0, 0};
    unsigned int membership[1031];
    unsigned int iterations, i, owner;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    if (!sixel_kmeans_gpu_is_available()) return 77;
    memset(&request, 0, sizeof(request));
    memset(points, 0, sizeof(points));
    memset(centers, 0, sizeof(centers));
    /* Duplicate seeds assign everything to zero. Equal maximum residuals
     * cross tile boundaries; removing one must expose the next candidate.
     * Once the five nonzero donors are exhausted, zero residual ties keep
     * selecting sample zero, including after a previous empty repair. */
    points[511 * 3] = points[1024 * 3] = 8;
    points[512 * 3] = -8;
    points[5 * 3] = 7;
    points[513 * 3] = -7;
    request.points = points;
    request.centers = centers;
    request.membership = membership;
    request.point_count = 1031;
    request.center_count = 8;
    request.max_iterations = request.min_iterations = 1;
    iterations = 0;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 1) return 1;
    for (i = 0; i < 8; ++i) {
        if (centers[i * 3] != expected[i] || centers[i * 3 + 1] != 0 ||
                centers[i * 3 + 2] != 0) return 1;
    }
    for (i = 0; i < 1031; ++i) {
        owner = i == 511 ? 1 : i == 512 ? 2 : i == 1024 ? 3 :
            i == 5 ? 4 : i == 513 ? 5 : i == 0 ? 7 : 0;
        if (membership[i] != owner) return 1;
    }
    return 0;
}
