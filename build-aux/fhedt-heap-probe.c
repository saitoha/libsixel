/* SPDX-License-Identifier: MIT */
/* FHEDT's R=256 distance grid holds one double per voxel. */
#include <stdlib.h>

int
main(void)
{
    void *volatile grid;

    grid = malloc((size_t)256 * 256 * 256 * sizeof(double));
    if (grid == NULL) {
        return 1;
    }
    free(grid);
    return 0;
}
