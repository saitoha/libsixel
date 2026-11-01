/* SPDX-License-Identifier: MIT */
/* Feedback compatibility generation retains its float export */
#include "../../_support/quant/palette/retention/retention_test_common.h"

#include "src/palette-kmeans.h"

int
test_palette_retention_feedback(int argc, char **argv)
{
    sixel_kmeans_feedback_mode saved;
    int ok;

    (void)argc;
    (void)argv;
    saved = sixel_get_kmeans_feedback_mode();
    sixel_set_kmeans_feedback_mode_override(1,
        SIXEL_PALETTE_KMEANS_FEEDBACK_ON);
    ok = retention_generated(SIXEL_QUANTIZE_MODEL_KMEANS);
    sixel_set_kmeans_feedback_mode_override(1, saved);
    return ok ? 0 : 1;
}
