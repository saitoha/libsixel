/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that quantizer capabilities describe the artifact boundary currently
 * implemented by each palette model.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

typedef struct quantizer_capability_case {
    int model;
    int accepts_raw;
    int accepts_weighted;
    int accepts_fractional;
    int requires_observed;
    int accepts_moments;
} quantizer_capability_case_t;

static quantizer_capability_case_t const capability_cases[] = {
    { SIXEL_QUANTIZE_MODEL_MEDIANCUT, 1, 0, 0, 0, 0 },
    { SIXEL_QUANTIZE_MODEL_KMEANS, 1, 1, 1, 0, 0 },
    { SIXEL_QUANTIZE_MODEL_KMEDOIDS, 1, 0, 0, 1, 0 },
    { SIXEL_QUANTIZE_MODEL_KCENTER, 1, 1, 0, 0, 0 }
};

static int
quantizer_capabilities_are_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_quantizer_capabilities_t capabilities;
    quantizer_capability_case_t const *test_case;
    size_t index;

    status = SIXEL_FALSE;
    test_case = NULL;
    index = 0u;
    for (index = 0u;
            index < sizeof(capability_cases) / sizeof(capability_cases[0]);
            ++index) {
        test_case = &capability_cases[index];
        status = sixel_palette_quantizer_capabilities_get(
            test_case->model,
            &capabilities);
        if (SIXEL_FAILED(status) ||
                capabilities.quantize_model != test_case->model ||
                capabilities.accepts_raw_samples !=
                    test_case->accepts_raw ||
                capabilities.accepts_weighted_points !=
                    test_case->accepts_weighted ||
                capabilities.accepts_fractional_weights !=
                    test_case->accepts_fractional ||
                capabilities.requires_observed_representatives !=
                    test_case->requires_observed ||
                capabilities.accepts_additional_moments !=
                    test_case->accepts_moments) {
            return 0;
        }
    }

    capabilities.quantize_model = 99;
    capabilities.accepts_raw_samples = 99;
    status = sixel_palette_quantizer_capabilities_get(
        SIXEL_QUANTIZE_MODEL_AUTO,
        &capabilities);
    if (status != SIXEL_BAD_ARGUMENT ||
            capabilities.quantize_model != 99 ||
            capabilities.accepts_raw_samples != 99) {
        return 0;
    }
    return 1;
}

int
test_palette_0033_quantizer_capabilities(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!quantizer_capabilities_are_valid()) {
        fprintf(stderr, "quantizer capability contract failed\n");
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
