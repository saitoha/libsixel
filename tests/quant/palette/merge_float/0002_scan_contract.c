/* SPDX-License-Identifier: MIT */
/* Exercise the real scan fallback by denying the dense Ward allocation. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>

static int ward_scan_denied;

static void *
ward_scan_malloc(size_t size)
{
    (void)size;
    ++ward_scan_denied;
    return NULL;
}

/* Isolate this copy's symbols for both ordinary and amalgamated builds.
 * Only malloc inside the reducer is replaced; its cluster workspace still
 * goes through the ordinary allocator so this reaches the scan fallback. */
#include "src/palette-common-merge.h"
typedef struct ward_scan_cluster ward_scan_cluster_t;
#define sixel_resolve_final_merge_mode ward_scan_resolve_mode
#define sixel_final_merge_load_env ward_scan_load_env
#define sixel_set_final_merge_channel_factor_override ward_scan_set_channel
#define sixel_palette_kmeans_iter_max ward_scan_get_iter
#define sixel_set_kmeans_iter_max_override ward_scan_set_iter
#define sixel_set_kmeans_threshold_override ward_scan_set_threshold
#define sixel_palette_kmeans_threshold ward_scan_get_threshold
#define sixel_set_final_merge_target_factor_override ward_scan_set_target
#define sixel_set_final_merge_lloyd_iterations_override ward_scan_set_lloyd
#define sixel_final_merge_lloyd_iterations ward_scan_get_lloyd
#define sixel_final_merge_target ward_scan_target
#define sixel_palette_apply_merge ward_scan_merge_integer
#define sixel_palette_merge_weighted ward_scan_merge_weighted
#define malloc ward_scan_malloc
#define env_final_merge_additional_lloyd ward_scan_local_0
#define env_final_merge_additional_lloyd_overridden ward_scan_local_1
#define env_final_merge_channel_factor_l ward_scan_local_2
#define env_final_merge_env_loaded ward_scan_local_3
#define env_final_merge_target_factor ward_scan_local_4
#define env_kmeans_iter_max ward_scan_local_5
#define env_kmeans_threshold ward_scan_local_6
#define sixel_final_merge_channel_factor_override_enabled ward_scan_local_7
#define sixel_final_merge_channel_factor_override_value ward_scan_local_8
#define sixel_final_merge_clusters ward_scan_local_9
#define sixel_final_merge_distance_sq ward_scan_local_10
#define sixel_final_merge_env_lock_acquire ward_scan_local_11
#define sixel_final_merge_env_lock_init_once ward_scan_local_12
#define sixel_final_merge_env_lock_release ward_scan_local_13
#define sixel_final_merge_env_mutex ward_scan_local_14
#define sixel_final_merge_env_mutex_once ward_scan_local_15
#define sixel_final_merge_env_mutex_ready ward_scan_local_16
#define sixel_final_merge_env_once ward_scan_local_17
#define sixel_final_merge_is_lab_colorspace ward_scan_local_18
#define sixel_final_merge_lloyd_override_enabled ward_scan_local_19
#define sixel_final_merge_lloyd_override_value ward_scan_local_20
#define sixel_final_merge_override_lock_acquire ward_scan_local_21
#define sixel_final_merge_override_lock_release ward_scan_local_22
#define sixel_final_merge_target_factor_override_enabled ward_scan_local_23
#define sixel_final_merge_target_factor_override_value ward_scan_local_24
#define sixel_final_merge_ward ward_scan_local_25
#define sixel_final_merge_ward_matrix ward_scan_local_26
#define sixel_final_merge_ward_pair_cost ward_scan_local_27
#define sixel_final_merge_ward_refresh_row ward_scan_local_28
#define sixel_kmeans_iter_max_override_enabled ward_scan_local_29
#define sixel_kmeans_iter_max_override_value ward_scan_local_30
#define sixel_kmeans_threshold_override_enabled ward_scan_local_31
#define sixel_kmeans_threshold_override_value ward_scan_local_32
#define sixel_palette_merge_impl ward_scan_impl
#define sixel_final_merge_cluster ward_scan_cluster
#define sixel_final_merge_cluster_t ward_scan_cluster_t
#define malloc ward_scan_malloc
#include "src/palette-common-merge.c"
#undef malloc
#undef sixel_resolve_final_merge_mode
#undef sixel_final_merge_load_env
#undef sixel_set_final_merge_channel_factor_override
#undef sixel_palette_kmeans_iter_max
#undef sixel_set_kmeans_iter_max_override
#undef sixel_set_kmeans_threshold_override
#undef sixel_palette_kmeans_threshold
#undef sixel_set_final_merge_target_factor_override
#undef sixel_set_final_merge_lloyd_iterations_override
#undef sixel_final_merge_lloyd_iterations
#undef sixel_final_merge_target
#undef sixel_palette_apply_merge
#undef sixel_palette_merge_weighted
#undef malloc
#undef env_final_merge_additional_lloyd
#undef env_final_merge_additional_lloyd_overridden
#undef env_final_merge_channel_factor_l
#undef env_final_merge_env_loaded
#undef env_final_merge_target_factor
#undef env_kmeans_iter_max
#undef env_kmeans_threshold
#undef sixel_final_merge_channel_factor_override_enabled
#undef sixel_final_merge_channel_factor_override_value
#undef sixel_final_merge_clusters
#undef sixel_final_merge_distance_sq
#undef sixel_final_merge_env_lock_acquire
#undef sixel_final_merge_env_lock_init_once
#undef sixel_final_merge_env_lock_release
#undef sixel_final_merge_env_mutex
#undef sixel_final_merge_env_mutex_once
#undef sixel_final_merge_env_mutex_ready
#undef sixel_final_merge_env_once
#undef sixel_final_merge_is_lab_colorspace
#undef sixel_final_merge_lloyd_override_enabled
#undef sixel_final_merge_lloyd_override_value
#undef sixel_final_merge_override_lock_acquire
#undef sixel_final_merge_override_lock_release
#undef sixel_final_merge_target_factor_override_enabled
#undef sixel_final_merge_target_factor_override_value
#undef sixel_final_merge_ward
#undef sixel_final_merge_ward_matrix
#undef sixel_final_merge_ward_pair_cost
#undef sixel_final_merge_ward_refresh_row
#undef sixel_kmeans_iter_max_override_enabled
#undef sixel_kmeans_iter_max_override_value
#undef sixel_kmeans_threshold_override_enabled
#undef sixel_kmeans_threshold_override_value
#undef sixel_palette_merge_impl
#undef sixel_final_merge_cluster
#undef sixel_final_merge_cluster_t

int
test_ward_scan_populations(int argc, char **argv)
{
    double weights[4] = { .25, .75, .5, 0 };
    double sums[12] = { -.5, 0, 0, -.75, 0, 0, 1.5, 0, 0, 0, 0, 0 };
    sixel_allocator_t *allocator;
    double first_weight;
    double first_sum;
    int count;
    int ok;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    first_weight = 1;
    first_sum = -1.25;
    if (atoi(argv[1]) == 0) {
        /* An interior hole with two live entries already fits target=2.
         * A scan that clears slots [active,nclusters) loses the last one. */
        weights[1] = 0;
        sums[3] = 0;
        first_weight = .25;
        first_sum = -.5;
    }
    allocator = NULL;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    ward_scan_denied = 0;
    count = ward_scan_merge_weighted(weights, sums, 3, 4, 2,
        SIXEL_FINAL_MERGE_WARD, 0, SIXEL_PIXELFORMAT_RGBFLOAT32, 1,
        allocator);
    /* These binary fractions and the one exact mean need no libm; the
     * small absolute margin still allows excess intermediate precision. */
    ok = count == 2 && ward_scan_denied == 1 &&
        weights[0] == first_weight && weights[1] == .5 &&
        weights[2] == 0 && weights[3] == 0 &&
        isfinite(sums[0]) && fabs(sums[0] - first_sum) <= 1e-12 &&
        isfinite(sums[3]) && fabs(sums[3] - 1.5) <= 1e-12 &&
        sums[6] == 0 && sums[9] == 0;
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
