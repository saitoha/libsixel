/*
 * Helper exposing sixel_get_kmeans_init_type for TAP shell wrappers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "compat_stub.h"
#include "palette-kmeans.h"

static char const *
sixel_kmeans_init_type_to_string(sixel_kmeans_init_type init_type)
{
    switch (init_type) {
    case SIXEL_PALETTE_KMEANS_INIT_PCA:
        return "pca";
    case SIXEL_PALETTE_KMEANS_INIT_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_INIT_AUTO:
    default:
        return "auto";
    }
}

int
main(int argc, char **argv)
{
    sixel_kmeans_init_type first;
    sixel_kmeans_init_type second;
    int run_cache_check;

    run_cache_check = 0;
    if (argc > 1 && strcmp(argv[1], "--cache") == 0) {
        run_cache_check = 1;
    }

    first = sixel_get_kmeans_init_type();
    if (!run_cache_check) {
        printf("%s\n", sixel_kmeans_init_type_to_string(first));

        return 0;
    }

    if (sixel_compat_setenv("SIXEL_PALETTE_KMEANS_INITTYPE", "none")
            != 0) {
        fprintf(stderr, "failed to set environment override\n");

        return 1;
    }

    second = sixel_get_kmeans_init_type();
    printf("%s %s\n",
           sixel_kmeans_init_type_to_string(first),
           sixel_kmeans_init_type_to_string(second));

    return 0;
}
