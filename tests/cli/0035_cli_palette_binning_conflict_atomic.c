/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that conflicting palette-binning spellings do not partially update
 * encoder state when sixel_encoder_setopt() rejects the second spelling.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/encoder.h"
#include "src/palette-kmeans.h"
#include "src/palette-plan.h"

static int
top_level_first_is_atomic(void)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    int valid;

    status = SIXEL_FALSE;
    encoder = NULL;
    valid = 0;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        return 0;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_PALETTE_BINNING,
                                  "hard");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_QUANTIZE_MODEL,
                                  "kmeans:binning=soft");
    valid = status == SIXEL_BAD_ARGUMENT &&
        encoder->palette_binning_policy == SIXEL_PALETTE_BINNING_HARD &&
        encoder->palette_binning_override == 1 &&
        encoder->palette_binning_origin ==
            SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT &&
        encoder->quantize_model == SIXEL_QUANTIZE_MODEL_AUTO &&
        encoder->quantize_model_kmeans_binning_mode ==
            SIXEL_PALETTE_KMEANS_BINNING_AUTO &&
        encoder->quantize_model_kmeans_binning_override == 0 &&
        encoder->quantize_model_kmeans_binning_origin ==
            SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT;

cleanup:
    sixel_encoder_unref(encoder);
    return valid;
}

static int
legacy_alias_first_is_atomic(void)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    int valid;

    status = SIXEL_FALSE;
    encoder = NULL;
    valid = 0;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        return 0;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_QUANTIZE_MODEL,
                                  "kmeans:binning=soft");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_PALETTE_BINNING,
                                  "hard");
    valid = status == SIXEL_BAD_ARGUMENT &&
        encoder->palette_binning_policy == SIXEL_PALETTE_BINNING_AUTO &&
        encoder->palette_binning_override == 0 &&
        encoder->palette_binning_origin ==
            SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT &&
        encoder->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS &&
        encoder->quantize_model_kmeans_binning_mode ==
            SIXEL_PALETTE_KMEANS_BINNING_SOFT &&
        encoder->quantize_model_kmeans_binning_override == 1 &&
        encoder->quantize_model_kmeans_binning_origin ==
            SIXEL_PALETTE_POLICY_ORIGIN_LEGACY_ALIAS;

cleanup:
    sixel_encoder_unref(encoder);
    return valid;
}

int
test_cli_0035_cli_palette_binning_conflict_atomic(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!top_level_first_is_atomic() ||
            !legacy_alias_first_is_atomic()) {
        fprintf(stderr, "palette-binning conflict changed encoder state\n");
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
