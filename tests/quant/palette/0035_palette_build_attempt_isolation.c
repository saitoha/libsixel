/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that palette-build attempt state belongs to one palette instance.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/palette.h"
#include "src/palette-private.h"

static int
attempt_matches(sixel_palette_build_attempt_t const *attempt,
                sixel_palette_binning_policy_t binning,
                int quantize_model)
{
    return attempt != NULL &&
        attempt->binning.policy.requested == (int)binning &&
        attempt->binning.policy.phase == SIXEL_PALETTE_POLICY_UNRESOLVED &&
        attempt->quantize_model == quantize_model;
}

int
test_palette_0035_palette_build_attempt_isolation(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_palette_t *first;
    sixel_palette_t *second;
    sixel_palette_storage_t *first_storage;
    sixel_palette_build_context_t active_context;
    sixel_palette_build_attempt_t first_attempt;
    sixel_palette_build_attempt_t second_attempt;
    sixel_palette_build_attempt_t result;
    sixel_palette_generate_request_t generate_request;
    void *object;
    unsigned char pixel[3];

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    first = NULL;
    second = NULL;
    first_storage = NULL;
    object = NULL;
    memset(&active_context, 0, sizeof(active_context));
    memset(&generate_request, 0, sizeof(generate_request));
    pixel[0] = 0U;
    pixel[1] = 0U;
    pixel[2] = 0U;
    sixel_palette_binning_state_init(
        &first_attempt.binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    first_attempt.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    first_attempt.quantizer_retry_count = 0U;
    sixel_palette_binning_state_init(
        &second_attempt.binning,
        SIXEL_PALETTE_BINNING_SOFT,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    second_attempt.quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    second_attempt.quantizer_retry_count = 0U;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_factory_new(allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    first = (sixel_palette_t *)object;
    object = NULL;
    status = sixel_palette_factory_new(allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    second = (sixel_palette_t *)object;
    object = NULL;

    status = sixel_palette_build_attempt_begin(first, &first_attempt);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_build_attempt_begin(second, &second_attempt);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_build_attempt_begin(first, &second_attempt);
    if (status != SIXEL_LOGIC_ERROR) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_palette_build_attempt_finish(second, &result);
    if (SIXEL_FAILED(status) ||
            !attempt_matches(&result,
                             SIXEL_PALETTE_BINNING_SOFT,
                             SIXEL_QUANTIZE_MODEL_AUTO)) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = sixel_palette_build_attempt_finish(first, &result);
    if (SIXEL_FAILED(status) ||
            !attempt_matches(&result,
                             SIXEL_PALETTE_BINNING_HARD,
                             SIXEL_QUANTIZE_MODEL_KMEANS)) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = sixel_palette_build_attempt_finish(first, &result);
    if (status != SIXEL_LOGIC_ERROR) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    first_storage = SIXEL_PALETTE_STORAGE(first);
    first_storage->build_context = &active_context;
    generate_request.data = pixel;
    generate_request.length = sizeof(pixel);
    generate_request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    generate_request.requested_colors = 1U;
    generate_request.quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    status = first->vtbl->generate(first, &generate_request);
    if (status != SIXEL_LOGIC_ERROR ||
            first_storage->build_context != &active_context) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    first_storage->build_context = NULL;
    status = SIXEL_OK;

cleanup:
    if (second != NULL) {
        second->vtbl->unref(second);
    }
    if (first != NULL) {
        first_storage = SIXEL_PALETTE_STORAGE(first);
        first_storage->build_context = NULL;
        first->vtbl->unref(first);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "palette build attempt isolation failed\n");
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
