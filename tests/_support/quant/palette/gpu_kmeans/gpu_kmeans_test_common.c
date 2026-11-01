/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_available(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_kmeans_gpu_is_available() ? EXIT_SUCCESS : 77;
}
