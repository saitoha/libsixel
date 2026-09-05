/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that sampling and binning choices are pure stage-boundary decisions.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

static int
quantizer_selection_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_quantizer_resolver_input_t input;
    sixel_palette_quantizer_selection_t selection;

    status = SIXEL_FALSE;
    input.requested = SIXEL_QUANTIZE_MODEL_AUTO;
    input.binning_requested = SIXEL_PALETTE_BINNING_AUTO;
    status = sixel_palette_quantizer_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_QUANTIZE_MODEL_MEDIANCUT ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_LEGACY_COMPAT ||
            selection.capabilities.quantize_model !=
                SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        return 0;
    }

    input.binning_requested = SIXEL_PALETTE_BINNING_SOFT;
    status = sixel_palette_quantizer_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_QUANTIZE_MODEL_KMEANS ||
            selection.reason !=
                SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY ||
            selection.capabilities.accepts_fractional_weights != 1) {
        return 0;
    }

    input.binning_requested = SIXEL_PALETTE_BINNING_EXACT;
    status = sixel_palette_quantizer_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_QUANTIZE_MODEL_KMEANS ||
            selection.reason !=
                SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY ||
            selection.capabilities.quantize_model !=
                SIXEL_QUANTIZE_MODEL_KMEANS) {
        return 0;
    }

    input.requested = SIXEL_QUANTIZE_MODEL_KMEDOIDS;
    input.binning_requested = SIXEL_PALETTE_BINNING_AUTO;
    status = sixel_palette_quantizer_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_QUANTIZE_MODEL_KMEDOIDS ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_EXPLICIT ||
            selection.capabilities.requires_observed_representatives != 1) {
        return 0;
    }
    return 1;
}

static int
sampling_selection_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_sampling_resolver_input_t input;
    sixel_palette_sampling_selection_t selection;

    status = SIXEL_FALSE;
    input.requested = SIXEL_PALETTE_SAMPLING_AUTO;
    input.total_threads = 3;
    input.heavy_operations = 1;
    input.async_eligible = 1;
    selection.effective = SIXEL_PALETTE_SAMPLING_AUTO;
    selection.source = SIXEL_PALETTE_SAMPLING_SOURCE_NONE;
    selection.reason = SIXEL_PALETTE_RESOLUTION_NONE;
    status = sixel_palette_sampling_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            selection.source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME ||
            selection.reason !=
                SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE) {
        return 0;
    }

    input.requested = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    status = sixel_palette_sampling_select(&input, &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
            selection.source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_EXPLICIT) {
        return 0;
    }

    input.requested = (sixel_palette_sampling_policy_t)99;
    selection.effective = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    selection.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    selection.reason = SIXEL_PALETTE_RESOLUTION_FALLBACK;
    status = sixel_palette_sampling_select(&input, &selection);
    if (status != SIXEL_BAD_ARGUMENT ||
            selection.effective != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            selection.source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_FALLBACK) {
        return 0;
    }
    return 1;
}

static void
binning_input_init(sixel_palette_binning_resolver_input_t *input)
{
    input->requested = SIXEL_PALETTE_BINNING_AUTO;
    input->bits_per_axis = 6u;
    input->grid_map = SIXEL_PALETTE_BINNING_GRID_UNIFORM;
    input->kernel = SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR;
    input->backend = SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE;
    input->source_point_count = 255u;
    input->requested_colors = 8u;
    input->auto_ratio = 32u;
}

static int
kmeans_binning_selection_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_quantizer_capabilities_t capabilities;
    sixel_palette_binning_resolver_input_t input;
    sixel_palette_binning_selection_t selection;

    status = sixel_palette_quantizer_capabilities_get(
        SIXEL_QUANTIZE_MODEL_KMEANS,
        &capabilities);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    binning_input_init(&input);
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_BINNING_HARD ||
            selection.bits_per_axis != 6u ||
            selection.kernel != SIXEL_PALETTE_BINNING_KERNEL_NONE ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA) {
        return 0;
    }

    input.source_point_count = 256u;
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_BINNING_SOFT ||
            selection.kernel !=
                SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA) {
        return 0;
    }

    input.requested = SIXEL_PALETTE_BINNING_HARD;
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_BINNING_HARD ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_EXPLICIT) {
        return 0;
    }

    input.requested = SIXEL_PALETTE_BINNING_EXACT;
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_BINNING_EXACT ||
            selection.bits_per_axis != 0u ||
            selection.grid_map != SIXEL_PALETTE_BINNING_GRID_NONE ||
            selection.kernel != SIXEL_PALETTE_BINNING_KERNEL_NONE ||
            selection.backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_EXPLICIT) {
        return 0;
    }
    return 1;
}

static int
capability_constrained_binning_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_quantizer_capabilities_t capabilities;
    sixel_palette_binning_resolver_input_t input;
    sixel_palette_binning_selection_t selection;

    status = sixel_palette_quantizer_capabilities_get(
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        &capabilities);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    binning_input_init(&input);
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (SIXEL_FAILED(status) ||
            selection.effective != SIXEL_PALETTE_BINNING_NONE ||
            selection.backend != SIXEL_PALETTE_BINNING_BACKEND_DIRECT ||
            selection.reason !=
                SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY) {
        return 0;
    }

    status = sixel_palette_quantizer_capabilities_get(
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        &capabilities);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    input.requested = SIXEL_PALETTE_BINNING_SOFT;
    selection.effective = SIXEL_PALETTE_BINNING_EXACT;
    selection.bits_per_axis = 99u;
    selection.reason = SIXEL_PALETTE_RESOLUTION_FALLBACK;
    status = sixel_palette_binning_select(&input,
                                          &capabilities,
                                          &selection);
    if (status != SIXEL_BAD_ARGUMENT ||
            selection.effective != SIXEL_PALETTE_BINNING_EXACT ||
            selection.bits_per_axis != 99u ||
            selection.reason != SIXEL_PALETTE_RESOLUTION_FALLBACK) {
        return 0;
    }
    return 1;
}

int
test_palette_0034_palette_stage_resolvers(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!quantizer_selection_is_valid() ||
            !sampling_selection_is_valid() ||
            !kmeans_binning_selection_is_valid() ||
            !capability_constrained_binning_is_valid()) {
        fprintf(stderr, "palette stage resolver contract failed\n");
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
