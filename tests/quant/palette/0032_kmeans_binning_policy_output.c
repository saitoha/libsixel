/*
 * SPDX-License-Identifier: MIT
 *
 * Lock deterministic Kmeans palette bytes across the shared binning-policy
 * boundary. These cases cover hard, soft, and feedback-assisted construction.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/palette-kmeans.h"
#include "src/palette-plan.h"
#include "src/palette-private.h"
#include "src/palette.h"

static void
kmeans_policy_reset_overrides(void)
{
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_binbits_override(0, 0u);
    sixel_set_kmeans_mapping_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM);
    sixel_set_kmeans_softdist_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR);
    sixel_set_kmeans_seed_override(0, 0u);
    sixel_set_kmeans_restarts_override(0, 0u);
    sixel_set_kmeans_iter_override(0, 0u);
    sixel_set_kmeans_miniter_override(0, 0u);
    sixel_set_kmeans_polish_iter_override(0, 0u);
    sixel_set_kmeans_prune_policy_override(
        0,
        SIXEL_PALETTE_KMEANS_PRUNE_AUTO);
    sixel_set_kmeans_feedback_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kmeans_feedback_slots_override(0, 0u);
    sixel_set_kmeans_feedback_interval_override(0, 0u);
    sixel_set_kmeans_threshold_override(0, 0.0);
}

static SIXELSTATUS
kmeans_policy_build(unsigned char const *pixels,
                    unsigned int length,
                    sixel_palette_binning_policy_t binning_policy,
                    sixel_kmeans_feedback_mode feedback_mode,
                    sixel_allocator_t *allocator,
                    unsigned char output[12])
{
    SIXELSTATUS status;
    sixel_palette_t *palette;
    sixel_palette_storage_t *storage;
    sixel_palette_build_context_t context;
    sixel_palette_build_attempt_t attempt;
    sixel_palette_entries_view_t view;
    void *object;

    status = SIXEL_FALSE;
    palette = NULL;
    storage = NULL;
    memset(&context, 0, sizeof(context));
    memset(&attempt, 0, sizeof(attempt));
    memset(&view, 0, sizeof(view));
    object = NULL;
    if (pixels == NULL || allocator == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_set_kmeans_init_type_override(1, SIXEL_PALETTE_KMEANS_INIT_PCA);
    sixel_set_kmeans_binbits_override(1, 4u);
    sixel_set_kmeans_mapping_mode_override(
        1,
        SIXEL_PALETTE_KMEANS_MAPPING_SRGB);
    sixel_set_kmeans_softdist_mode_override(
        1,
        SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR);
    sixel_set_kmeans_seed_override(1, 0x12345678u);
    sixel_set_kmeans_restarts_override(1, 1u);
    sixel_set_kmeans_iter_override(1, 12u);
    sixel_set_kmeans_miniter_override(1, 1u);
    sixel_set_kmeans_polish_iter_override(1, 0u);
    sixel_set_kmeans_prune_policy_override(
        1,
        SIXEL_PALETTE_KMEANS_PRUNE_NONE);
    sixel_set_kmeans_feedback_mode_override(1, feedback_mode);
    sixel_set_kmeans_feedback_slots_override(1, 2u);
    sixel_set_kmeans_feedback_interval_override(1, 1u);
    sixel_set_kmeans_threshold_override(1, 0.0);

    status = sixel_palette_factory_new(allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    palette = (sixel_palette_t *)object;
    storage = SIXEL_PALETTE_STORAGE(palette);
    context.requested_colors = 4u;
    context.quality_mode = SIXEL_QUALITY_FULL;
    context.force_palette = 1;
    context.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    context.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    sixel_palette_binning_state_init(
        &attempt.binning,
        binning_policy,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    attempt.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    attempt.quantizer_retry_count = 0u;
    context.attempt = &attempt;
    storage->requested_colors = context.requested_colors;
    storage->build_context = &context;

    status = sixel_palette_build_kmeans(palette,
                                        pixels,
                                        length,
                                        SIXEL_PIXELFORMAT_RGB888,
                                        allocator,
                                        NULL,
                                        NULL,
                                        "policy-golden",
                                        NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = palette->vtbl->get_entries(palette, &view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (attempt.binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED ||
            attempt.binning.policy.effective != (int)binning_policy ||
            view.entry_count != 4u || view.depth != 3 ||
            view.entries_size < 12u) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    memcpy(output, view.entries, 12u);

cleanup:
    if (storage != NULL) {
        storage->build_context = NULL;
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    kmeans_policy_reset_overrides();
    return status;
}

int
test_palette_0032_kmeans_binning_policy_output(int argc, char **argv)
{
    static unsigned char const expected[3][12] = {
        {36, 64, 124, 176, 88, 58, 123, 181, 144, 191, 225, 153},
        {35, 64, 124, 176, 88, 58, 122, 163, 152, 177, 230, 144},
        {36, 64, 124, 162, 64, 6, 154, 173, 132, 168, 62, 16}
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char pixels[96];
    unsigned char actual[3][12];
    unsigned int index;
    unsigned int component;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    memset(pixels, 0, sizeof(pixels));
    memset(actual, 0, sizeof(actual));
    index = 0u;
    component = 0u;
    for (index = 0u; index < 32u; ++index) {
        pixels[index * 3u + 0u] = (unsigned char)(index * 37u + 11u);
        pixels[index * 3u + 1u] = (unsigned char)(index * 73u + 29u);
        pixels[index * 3u + 2u] = (unsigned char)(index * 19u + 53u);
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = kmeans_policy_build(
        pixels,
        sizeof(pixels),
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF,
        allocator,
        actual[0]);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = kmeans_policy_build(
        pixels,
        sizeof(pixels),
        SIXEL_PALETTE_BINNING_SOFT,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF,
        allocator,
        actual[1]);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = kmeans_policy_build(
        pixels,
        sizeof(pixels),
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_KMEANS_FEEDBACK_ON,
        allocator,
        actual[2]);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (memcmp(actual, expected, sizeof(expected)) != 0) {
        for (index = 0u; index < 3u; ++index) {
            fprintf(stderr, "case %u:", index);
            for (component = 0u; component < 12u; ++component) {
                fprintf(stderr, " %u", actual[index][component]);
            }
            fputc('\n', stderr);
        }
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    kmeans_policy_reset_overrides();
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
