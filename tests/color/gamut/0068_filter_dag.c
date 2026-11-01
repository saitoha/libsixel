/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_filter_common.h"
#include "src/encoder.h"
#include "src/planner.h"

int
test_gamut_0068_filter_dag(int argc, char **argv)
{
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *frame = NULL;
    sixel_encoding_planner_t plan;
    float pixels[12] = {0};
    int ok, i, gamut, before, after, color;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, pixels, 2, 2,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0)) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "oklab")) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_OUTPUT_COLORSPACE, "gamma:gamut=edge-seeker"));
    gamut = before = after = color = -1;
    sixel_encoding_planner_init(&plan);
    if (ok) {
        sixel_encoding_planner_analyze(&plan, encoder, frame);
        ok = SIXEL_SUCCEEDED(sixel_encoding_planner_schedule(&plan,
            encoder, frame, SIXEL_PALETTE_SAMPLING_FULL_FRAME));
        for (i = 0; i < plan.dag_node_count; ++i) {
            if (plan.dag_nodes[i].kind == SIXEL_PLANNER_NODE_GAMUT) {
                gamut = i;
            }
            if (plan.dag_nodes[i].kind == SIXEL_PLANNER_NODE_COLORSPACE_POST) {
                color = i;
            }
        }
        for (i = 0; i < plan.dag_edge_count; ++i) {
            if (plan.dag_edges[i].to == gamut) {
                before = plan.dag_edges[i].from;
            }
            if (plan.dag_edges[i].from == gamut) {
                after = plan.dag_edges[i].to;
            }
        }
        ok = ok && gamut >= 0 && color >= 0 &&
            before == color && after >= 0 &&
            (plan.dag_nodes[after].kind == SIXEL_PLANNER_NODE_JOIN ||
             plan.dag_nodes[after].kind == SIXEL_PLANNER_NODE_DITHER);
    }
    if (!ok) {
        fprintf(stderr, "gamut=%d color=%d before=%d after=%d\n",
            gamut, color, before, after);
        for (i = 0; i < plan.dag_node_count; ++i) {
            fprintf(stderr, "node %d kind %d\n", i,
                plan.dag_nodes[i].kind);
        }
    }
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
