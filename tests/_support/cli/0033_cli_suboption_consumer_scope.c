/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that suboption rows are visible only to their declared consumer
 * family.  This prevents shared option schemas from accepting foreign keys.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/options.h"
#include "src/options-registry.h"

int
test_cli_0033_consumer_scope(int argc, char **argv)
{
    sixel_option_argument_schema_t const *diffusion_schema;
    sixel_option_argument_schema_t const *dequantize_schema;
    sixel_option_argument_schema_t const *gpu_schema;
    sixel_option_argument_resolution_t resolution;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    diffusion_schema = sixel_option_registry_get(
        SIXEL_OPTION_SCHEMA_DIFFUSION);
    dequantize_schema = sixel_option_registry_get(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE);
    gpu_schema = sixel_option_registry_get(SIXEL_OPTION_SCHEMA_GPU_POLICY);
    memset(&resolution, 0, sizeof(resolution));
    status = SIXEL_FALSE;

    if (!sixel_option_registry_validate() || diffusion_schema == NULL ||
        dequantize_schema == NULL || gpu_schema == NULL) {
        fprintf(stderr, "suboption registry is invalid\n");
        return EXIT_FAILURE;
    }
    /*
     * Encoder and decoder frontends both spell their primary image policy
     * as -d.  The semantic consumer domains, rather than executable names,
     * must keep this intentional short-option reuse unambiguous.
     */
    if (diffusion_schema->optflag != SIXEL_OPTFLAG_DIFFUSION ||
        dequantize_schema->optflag != SIXEL_OPTFLAG_DEQUANTIZE ||
        diffusion_schema->optflag != dequantize_schema->optflag ||
        (diffusion_schema->scope & dequantize_schema->scope) != 0u) {
        fprintf(stderr, "encoder and decoder short-option reuse is invalid\n");
        return EXIT_FAILURE;
    }
    if (sixel_option_registry_suboption_count_for_scope(
            diffusion_schema,
            NULL,
            SIXEL_OPTION_SCOPE_ENCODER) == 0u ||
        sixel_option_registry_suboption_count_for_scope(
            diffusion_schema,
            NULL,
            SIXEL_OPTION_SCOPE_DECODER) != 0u) {
        fprintf(stderr, "diffusion consumer scope is incorrect\n");
        return EXIT_FAILURE;
    }
    if (sixel_option_registry_suboption_count_for_scope(
            dequantize_schema,
            NULL,
            SIXEL_OPTION_SCOPE_DECODER) == 0u ||
        sixel_option_registry_suboption_count_for_scope(
            dequantize_schema,
            NULL,
            SIXEL_OPTION_SCOPE_ENCODER) != 0u) {
        fprintf(stderr, "dequantize consumer scope is incorrect\n");
        return EXIT_FAILURE;
    }
    if (sixel_option_registry_suboption_count_for_scope(
            gpu_schema,
            NULL,
            SIXEL_OPTION_SCOPE_ENCODER) != 1u ||
        sixel_option_registry_suboption_count_for_scope(
            gpu_schema,
            NULL,
            SIXEL_OPTION_SCOPE_DECODER) != 1u) {
        fprintf(stderr, "shared GPU consumer scope is incorrect\n");
        return EXIT_FAILURE;
    }

    status = sixel_option_parse_argument_with_suboptions(
        "auto:palette_threshold=17",
        gpu_schema,
        SIXEL_OPTION_SCOPE_ENCODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_FAILED(status) || resolution.assignment_count != 1u ||
        resolution.assignments[0].key_def->binding.target_class !=
            SIXEL_SUBOPTION_TARGET_ENCODER) {
        fprintf(stderr, "encoder GPU binding is incorrect\n");
        return EXIT_FAILURE;
    }
    sixel_option_free_argument_resolution(&resolution);

    status = sixel_option_parse_argument_with_suboptions(
        "auto:palette_threshold=17",
        gpu_schema,
        SIXEL_OPTION_SCOPE_DECODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "decoder accepted encoder GPU suboption\n");
        sixel_option_free_argument_resolution(&resolution);
        return EXIT_FAILURE;
    }

    status = sixel_option_parse_argument_with_suboptions(
        "auto:dequant_threshold=17",
        gpu_schema,
        SIXEL_OPTION_SCOPE_DECODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_FAILED(status) || resolution.assignment_count != 1u ||
        resolution.assignments[0].key_def->binding.target_class !=
            SIXEL_SUBOPTION_TARGET_DECODER) {
        fprintf(stderr, "decoder GPU binding is incorrect\n");
        return EXIT_FAILURE;
    }
    sixel_option_free_argument_resolution(&resolution);

    status = sixel_option_parse_argument_with_suboptions(
        "auto:dequant_threshold=17",
        gpu_schema,
        SIXEL_OPTION_SCOPE_ENCODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "encoder accepted decoder GPU suboption\n");
        sixel_option_free_argument_resolution(&resolution);
        return EXIT_FAILURE;
    }

    status = sixel_option_parse_argument_with_suboptions(
        "fs:scan=raster",
        diffusion_schema,
        SIXEL_OPTION_SCOPE_ENCODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encoder could not parse diffusion suboption\n");
        return EXIT_FAILURE;
    }
    sixel_option_free_argument_resolution(&resolution);

    status = sixel_option_parse_argument_with_suboptions(
        "fs:scan=raster",
        diffusion_schema,
        SIXEL_OPTION_SCOPE_DECODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "decoder accepted encoder diffusion suboption\n");
        sixel_option_free_argument_resolution(&resolution);
        return EXIT_FAILURE;
    }

    status = sixel_option_parse_argument_with_suboptions(
        "selective_blur:threshold=32",
        dequantize_schema,
        SIXEL_OPTION_SCOPE_DECODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "decoder could not parse dequantize suboption\n");
        return EXIT_FAILURE;
    }
    sixel_option_free_argument_resolution(&resolution);

    status = sixel_option_parse_argument_with_suboptions(
        "selective_blur:threshold=32",
        dequantize_schema,
        SIXEL_OPTION_SCOPE_ENCODER,
        &resolution,
        NULL,
        0u);
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "encoder accepted decoder dequantize suboption\n");
        sixel_option_free_argument_resolution(&resolution);
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
