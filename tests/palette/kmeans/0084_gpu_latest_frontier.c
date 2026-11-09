/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "src/palette-kmeans-gpu.h"
int test_palette_store_0033(int argc, char **argv);

int
test_kmgpu_0084_gpu_latest_frontier(int argc, char **argv)
{
    if (!sixel_kmeans_gpu_is_available()) return 77;
    return test_palette_store_0033(argc, argv);
}
