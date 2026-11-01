/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_0002_byte_rounding(int argc, char **argv)
{
    sixel_kmeans_gpu_request_t request;
    SIXELSTATUS status;
    unsigned int iterations;
    double points[6] = { 0, 0, 0, 1, 1, 1 };
    double centers[3] = { 0, 0, 0 };

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    iterations = 0;
    if (!sixel_kmeans_gpu_is_available()) {
        return 77;
    }

    /* The exactly representable half-integer must round upward, matching
     * the byte solver even when SIXEL output-grid snap is disabled. */
    request.points = points;
    request.centers = centers;
    request.point_count = 2;
    request.center_count = 1;
    request.max_iterations = 10;
    request.round_centers = 1;
    status = sixel_kmeans_gpu_refine(&request, &iterations);
    if (status != SIXEL_OK || iterations != 2 || centers[0] != 1.0 ||
            centers[1] != 1.0 || centers[2] != 1.0) {
        fprintf(stderr, "GPU byte rounding failed: %x\n", status);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
