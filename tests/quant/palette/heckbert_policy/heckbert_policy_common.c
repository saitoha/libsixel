/* SPDX-License-Identifier: MIT */
/* Private algorithm contracts use an isolated copy of the production code.
 * Symbol isolation also supports the amalgamated test runner. */
#include "src/palette-heckbert.h"
#include "src/palette-private.h"
#include "src/palette-common-merge.h"
#define sixel_palette_heckbert_log_start policy_sixel_palette_heckbert_log_start
#define sixel_palette_heckbert_log_finish \
    policy_sixel_palette_heckbert_log_finish
#define sixel_palette_reversible_tuple policy_sixel_palette_reversible_tuple
#define alloctupletable policy_alloctupletable
#define histogram_dense_size policy_histogram_dense_size
#define histogram_control_make_for_policy \
    policy_histogram_control_make_for_policy
#define histogram_reconstruct policy_histogram_reconstruct
#define histogram_quantize policy_histogram_quantize
#define histogram_build_quantize_lut policy_histogram_build_quantize_lut
#define histogram_build_fast_pack_lut policy_histogram_build_fast_pack_lut
#define histogram_pack_color_fast_rgb policy_histogram_pack_color_fast_rgb
#define histogram_pack_color_fast_rgba policy_histogram_pack_color_fast_rgba
#define histogram_pack_color policy_histogram_pack_color
#define histogram_pack_color_from_lut policy_histogram_pack_color_from_lut
#define sixel_lut_build_histogram policy_sixel_lut_build_histogram
#define compareplane_lexicographic policy_compareplane_lexicographic
#define sixel_pca_entry_compare policy_sixel_pca_entry_compare
#define sixel_pca_entry_swap policy_sixel_pca_entry_swap
#define sixel_pca_select_weighted_median policy_sixel_pca_select_weighted_median
#define compareplane_lexicographic policy_compareplane_lexicographic
#define compareplanePca policy_compareplanePca
#define compareplanePcaCached policy_compareplanePcaCached
#define sumcompare policy_sumcompare
#define sixel_palette_heckbert_reposition_box \
    policy_sixel_palette_heckbert_reposition_box
#define newColorMap policy_newColorMap
#define newBoxVector policy_newBoxVector
#define sixel_palette_heckbert_entry_weight \
    policy_sixel_palette_heckbert_entry_weight
#define findBoxBoundariesAndWeight policy_findBoxBoundariesAndWeight
#define sortBoxByAxisBuckets policy_sortBoxByAxisBuckets
#define sixel_heckbert_axis_weights policy_sixel_heckbert_axis_weights
#define largestByWeights policy_largestByWeights
#define computePcaAxis policy_computePcaAxis
#define sortBoxByPcaProjection policy_sortBoxByPcaProjection
#define partitionBoxByPcaMedian policy_partitionBoxByPcaMedian
#define centerBox policy_centerBox
#define averageColors policy_averageColors
#define averagePixels policy_averagePixels
#define colormapFromBv policy_colormapFromBv
#define force_palette_compare policy_force_palette_compare
#define force_palette_completion policy_force_palette_completion
#define sixel_heckbert_rep_add policy_sixel_heckbert_rep_add
#define sixel_final_merge_lloyd_histogram \
    policy_sixel_final_merge_lloyd_histogram
#define sixel_palette_clusters_to_colormap \
    policy_sixel_palette_clusters_to_colormap
#define sixel_palette_heckbert_compute_box_median \
    policy_sixel_palette_heckbert_compute_box_median
#define sixel_heckbert_sse policy_sixel_heckbert_sse
#define sixel_heckbert_box_sse policy_sixel_heckbert_box_sse
#define sixel_palette_heckbert_split_is_degenerate \
    policy_sixel_palette_heckbert_split_is_degenerate
#define sixel_palette_heckbert_split_attempt \
    policy_sixel_palette_heckbert_split_attempt
#define splitBox policy_splitBox
#define mediancut policy_mediancut
#define sixel_heckbert_native_compare policy_sixel_heckbert_native_compare
#define sixel_heckbert_native_histogram policy_sixel_heckbert_native_histogram
#define sixel_palette_heckbert_colormap policy_sixel_palette_heckbert_colormap
#define sixel_palette_build_heckbert policy_sixel_palette_build_heckbert
#define sixel_palette_build_heckbert_float32 \
    policy_sixel_palette_build_heckbert_float32
#define sample policy_sample
#define tuple policy_tuple
#define tupleint policy_tupleint
#define tupletable policy_tupletable
#define tupletable2 policy_tupletable2
#define histogram_control policy_histogram_control
#define box policy_box
#define boxVector policy_boxVector
#define sixel_pca_sort_entry policy_sixel_pca_sort_entry
#define force_palette_source policy_force_palette_source
#define compareplanePcaAxis policy_compareplanePcaAxis
#define compareplanePcaDimensions policy_compareplanePcaDimensions
#define compareplaneTieDepth policy_compareplaneTieDepth
#define sixel_palette_heckbert_max_channels \
    policy_sixel_palette_heckbert_max_channels
#define sixel_palette_heckbert_axis_bins policy_sixel_palette_heckbert_axis_bins
#include "src/palette-heckbert.c"
#undef sixel_palette_heckbert_log_start
#undef sixel_palette_heckbert_log_finish
#undef sixel_palette_reversible_tuple
#undef alloctupletable
#undef histogram_dense_size
#undef histogram_control_make_for_policy
#undef histogram_reconstruct
#undef histogram_quantize
#undef histogram_build_quantize_lut
#undef histogram_build_fast_pack_lut
#undef histogram_pack_color_fast_rgb
#undef histogram_pack_color_fast_rgba
#undef histogram_pack_color
#undef histogram_pack_color_from_lut
#undef sixel_lut_build_histogram
#undef compareplane_lexicographic
#undef sixel_pca_entry_compare
#undef sixel_pca_entry_swap
#undef sixel_pca_select_weighted_median
#undef compareplane_lexicographic
#undef compareplanePca
#undef compareplanePcaCached
#undef sumcompare
#undef sixel_palette_heckbert_reposition_box
#undef newColorMap
#undef newBoxVector
#undef sixel_palette_heckbert_entry_weight
#undef findBoxBoundariesAndWeight
#undef sortBoxByAxisBuckets
#undef sixel_heckbert_axis_weights
#undef largestByWeights
#undef computePcaAxis
#undef sortBoxByPcaProjection
#undef partitionBoxByPcaMedian
#undef centerBox
#undef averageColors
#undef averagePixels
#undef colormapFromBv
#undef force_palette_compare
#undef force_palette_completion
#undef sixel_heckbert_rep_add
#undef sixel_final_merge_lloyd_histogram
#undef sixel_palette_clusters_to_colormap
#undef sixel_palette_heckbert_compute_box_median
#undef sixel_heckbert_sse
#undef sixel_heckbert_box_sse
#undef sixel_palette_heckbert_split_is_degenerate
#undef sixel_palette_heckbert_split_attempt
#undef splitBox
#undef mediancut
#undef sixel_heckbert_native_compare
#undef sixel_heckbert_native_histogram
#undef sixel_palette_heckbert_colormap
#undef sixel_palette_build_heckbert
#undef sixel_palette_build_heckbert_float32
#undef sample
#undef tuple
#undef tupleint
#undef tupletable
#undef tupletable2
#undef histogram_control
#undef box
#undef boxVector
#undef sixel_pca_sort_entry
#undef force_palette_source
#undef compareplanePcaAxis
#undef compareplanePcaDimensions
#undef compareplaneTieDepth
#undef sixel_palette_heckbert_max_channels
#undef sixel_palette_heckbert_axis_bins

int
heckbert_policy_check(int scenario)
{
    policy_tupletable2 table;
    policy_tupletable2 output;
    sixel_allocator_t *allocator;
    sixel_heckbert_options_t options;
    sixel_palette_rep_stats_t stats[2];
    unsigned long cluster_weights[2];
    double cluster_sums[6];
    struct policy_box boxes[3];
    double weights[4];
    double actual[4];
    double sums[4];
    double squares[4];
    double expected;
    double error;
    double best;
    double population;
    double mean;
    unsigned int cut;
    unsigned int count;
    unsigned int i;
    unsigned int j;
    unsigned int k;
    unsigned int side;
    unsigned int winner;
    int format;
    int method;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    memset(&options, 0, sizeof(options));
    memset(&output, 0, sizeof(output));
    memset(boxes, 0, sizeof(boxes));
    memset(actual, 0, sizeof(actual));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL))) {
        return 1;
    }
    table = policy_newColorMap(5, 3, allocator);
    if (table.size == 0U) {
        sixel_allocator_unref(allocator);
        return 1;
    }
    table.preserve_extended = 0;
    for (i = 0; i < 5; ++i) {
        table.table[i]->value = 1U;
        for (j = 0; j < 3; ++j) {
            table.table[i]->policy_tuple[j] = 0.0;
        }
    }
    ok = 0;
    if (scenario < 3) {
        table.table[1]->policy_tuple[0] = 1.0;
        if (scenario == 0) {
            policy_centerBox(0, 2, table, 3, actual);
        } else if (scenario == 1) {
            policy_averageColors(0, 2, table, 3, actual);
        } else {
            policy_averagePixels(0, 2, table, 3, actual);
        }
        ok = actual[0] == 1.0;
    } else if (scenario < 9) {
        table.size = 3;
        table.table[1]->policy_tuple[0] = 2.0;
        table.table[2]->policy_tuple[0] = 9.0;
        table.table[2]->value = 6U;
        table.preserve_extended = 1;
        method = scenario % 3 == 0 ? SIXEL_REP_CENTER_BOX
            : scenario % 3 == 1 ? SIXEL_REP_AVERAGE_COLORS
            : SIXEL_REP_AVERAGE_PIXELS;
        expected = scenario % 3 == 0 ? 4.5
            : scenario % 3 == 1 ? 11.0 / 3.0 : 7.0;
        sixel_set_final_merge_target_factor_override(1, 3.0);
        sixel_set_final_merge_lloyd_iterations_override(1,
            scenario < 6 ? 0 : 3);
        status = policy_mediancut(table, 3, 1, SIXEL_LARGE_NORM,
            method, 0, SIXEL_FINAL_MERGE_WARD,
            SIXEL_PIXELFORMAT_RGBFLOAT32, &output, allocator,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &options);
        ok = SIXEL_SUCCEEDED(status) && output.size == 1 &&
            fabs(output.table[0]->policy_tuple[0] - expected) < 1e-12;
        sixel_set_final_merge_target_factor_override(0, 1.81);
        sixel_set_final_merge_lloyd_iterations_override(0, 3);
    } else if (scenario < 13) {
        format = scenario == 9 ? SIXEL_PIXELFORMAT_RGB888
            : scenario == 10 ? SIXEL_PIXELFORMAT_CIELABFLOAT32
            : scenario == 11 ? SIXEL_PIXELFORMAT_DIN99DFLOAT32
            : SIXEL_PIXELFORMAT_OKLABFLOAT32;
        policy_sixel_heckbert_axis_weights(format, SIXEL_LARGE_NORM,
            &options, weights);
        /* The histogram scales chromatic Lab axes by their nominal span.
         * Check native ratios independently for both norm and luminosity. */
        expected = scenario == 10 ? 1.5 : 1.0;
        ok = weights[0] == 1.0 && weights[1] == expected &&
            weights[2] == expected;
        policy_sixel_heckbert_axis_weights(format, SIXEL_LARGE_LUM,
            &options, weights);
        if (scenario == 9) {
            ok = ok && weights[0] == .2989 && weights[1] == .5866 &&
                weights[2] == .1145;
        } else {
            expected = scenario == 10 ? .75 : .5;
            ok = ok && weights[0] == 1.0 && weights[1] == expected &&
                weights[2] == expected;
        }
        options.weights[0] = 2.0;
        options.weights[1] = options.weights[2] = 1.0;
        policy_sixel_heckbert_axis_weights(format, SIXEL_LARGE_NORM,
            &options, weights);
        memset(sums, 0, sizeof(sums));
        squares[0] = 1.0;
        squares[1] = .75 / (format == SIXEL_PIXELFORMAT_CIELABFLOAT32
            ? 3.0 : format == SIXEL_PIXELFORMAT_DIN99DFLOAT32 ? 2.0 : 1.0);
        squares[2] = 0.0;
        ok = ok && policy_largestByWeights(sums, squares, 3, weights) == 0;
        options.weights[0] = .5;
        policy_sixel_heckbert_axis_weights(format, SIXEL_LARGE_NORM,
            &options, weights);
        ok = ok && policy_largestByWeights(sums, squares, 3, weights) == 1;
    } else if (scenario < 17) {
        /* Independent exhaustive residual sums, not moment subtraction. */
        table.preserve_extended = scenario >= 15;
        for (i = 0; i < 5; ++i) {
            table.table[i]->policy_tuple[0] = i == 4 ? 100.0 : (double)i;
            table.table[i]->policy_tuple[1] = (double)(i % 2);
            table.table[i]->value = i + 1;
        }
        options.cut_sse = 1;
        options.order_sse = 1;
        boxes[0].colors = 5;
        boxes[0].sum = 15;
        count = 1;
        status = policy_splitBox(boxes, &count, 0, table, 3,
            scenario % 2 ? SIXEL_LARGE_NORM : SIXEL_LARGE_PCA,
            NULL, 0, &options, SIXEL_PIXELFORMAT_RGBFLOAT32);
        best = -1.0;
        winner = 0;
        for (k = 1; k < 5; ++k) {
            error = 0.0;
            for (side = 0; side < 2; ++side) {
                for (j = 0; j < 3; ++j) {
                    mean = population = 0.0;
                    for (i = side == 0 ? 0 : k;
                            i < (side == 0 ? k : 5); ++i) {
                        population += table.table[i]->value;
                        mean += table.table[i]->policy_tuple[j]
                            * table.table[i]->value;
                    }
                    mean /= population;
                    for (i = side == 0 ? 0 : k;
                            i < (side == 0 ? k : 5); ++i) {
                        expected = table.table[i]->policy_tuple[j] - mean;
                        error += expected * expected * table.table[i]->value;
                    }
                }
            }
            if (best < 0.0 || error < best) {
                best = error;
                winner = k;
            }
        }
        cut = boxes[0].ind == 0 ? boxes[0].colors : boxes[1].colors;
        ok = SIXEL_SUCCEEDED(status) && count == 2 && cut == winner &&
            winner == 4 && boxes[0].priority >= boxes[1].priority &&
            fabs(boxes[0].priority + boxes[1].priority - best) < 1e-8;
    } else if (scenario == 18) {
        /* The color 5 starts on the left (an exact distance tie), then moves
         * right after the heavy zero population pulls the first centroid. */
        table.size = 4;
        table.preserve_extended = 1;
        table.table[0]->value = 100;
        table.table[1]->policy_tuple[0] = 5;
        table.table[2]->policy_tuple[0] = 9;
        table.table[2]->value = 3;
        table.table[3]->policy_tuple[0] = 10;
        memset(stats, 0, sizeof(stats));
        memset(cluster_sums, 0, sizeof(cluster_sums));
        cluster_weights[0] = 100;
        cluster_weights[1] = 5;
        cluster_sums[3] = 50;
        policy_sixel_final_merge_lloyd_histogram(table, 3, 2,
            cluster_weights, cluster_sums, 3, NULL,
            SIXEL_PIXELFORMAT_RGBFLOAT32, stats);
        ok = stats[0].colors == 1 && stats[1].colors == 3 &&
            stats[1].sums[0] == 24 && stats[1].min[0] == 5 &&
            stats[1].max[0] == 10 && cluster_weights[1] == 5 &&
            cluster_sums[3] == 42;
    } else {
        boxes[0].sum = 100;
        boxes[0].priority = 1;
        boxes[1].sum = 2;
        boxes[1].priority = 20;
        policy_sixel_palette_heckbert_reposition_box(boxes, 2, 1);
        ok = boxes[0].sum == 2;
    }
    if (output.table != NULL) {
        sixel_allocator_free(allocator, output.table);
    }
    sixel_allocator_free(allocator, table.table);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
