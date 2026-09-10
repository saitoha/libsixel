/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that every registered lookup policy retains a unique initial.
 * Policy: docs/cli/prefix-matching.md
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
test_cli_0035_lookup_initials(int argc, char **argv)
{
    sixel_option_argument_schema_t const *schema;
    sixel_option_argument_resolution_t resolution;
    sixel_option_value_schema_t const *value;
    SIXELSTATUS status;
    char prefix[2];
    size_t index;

    (void)argc;
    (void)argv;
    schema = sixel_option_registry_get(SIXEL_OPTION_SCHEMA_LUT_POLICY);

    if (schema == NULL || schema->value_count == 0u ||
        (schema->argument_match_flags & SIXEL_OPTION_MATCH_PREFIX) == 0u) {
        fprintf(stderr, "lookup policy schema does not accept prefixes\n");
        return EXIT_FAILURE;
    }

    /*
     * Resolve the initial through the production parser instead of copying
     * the policy list into the test.  A newly registered policy therefore
     * enters this invariant automatically.
     */
    index = 0u;
    while (index < schema->value_count) {
        value = schema->values + index;
        if (value->name == NULL || value->name[0] == '\0') {
            fprintf(stderr, "lookup policy %zu has no initial\n", index);
            return EXIT_FAILURE;
        }
        prefix[0] = value->name[0];
        prefix[1] = '\0';
        memset(&resolution, 0, sizeof(resolution));
        status = sixel_option_parse_argument_with_suboptions(
            prefix,
            schema,
            SIXEL_OPTION_SCOPE_ENCODER,
            &resolution,
            NULL,
            0u);
        if (SIXEL_FAILED(status) ||
            resolution.resolved_base_value != value->value) {
            fprintf(stderr,
                    "lookup policy %s does not resolve from prefix %s\n",
                    value->name,
                    prefix);
            sixel_option_free_argument_resolution(&resolution);
            return EXIT_FAILURE;
        }
        sixel_option_free_argument_resolution(&resolution);
        ++index;
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
