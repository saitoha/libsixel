/* SPDX-License-Identifier: MIT */
/* Keep an externally rejected cHRM unavailable to the loader matrix. */
#include "loader_libpng_chrm_test_common.h"
#include <stdio.h>
#include <stdlib.h>

int
test_loader_0199_png_chrm_getter_reject(int argc, char **argv)
{
#if HAVE_LIBPNG
    png_chrm_metadata_t metadata;
    int fixture;

    if (argc != 2) {
        return 1;
    }
    fixture = atoi(argv[1]);
    if (fixture < 0 || fixture >=
        (int)(sizeof(color_png_chrm) / sizeof(color_png_chrm[0])) ||
        png_chrm_read_metadata(&color_png_chrm[fixture], &metadata) ||
        metadata.chrm) {
        return 1;
    }
    fprintf(stderr, "PNG_CHRM_GETTER|fixture=%d|rejected=1\n", fixture);
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
