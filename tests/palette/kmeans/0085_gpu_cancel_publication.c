/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "src/palette-kmeans-gpu.h"
int test_palette_store_0034(int argc, char **argv);

int
test_kmgpu_0085_gpu_cancel_publication(int argc, char **argv)
{
    if (!sixel_kmeans_gpu_is_available()) return 77;
    return test_palette_store_0034(argc, argv);
}
