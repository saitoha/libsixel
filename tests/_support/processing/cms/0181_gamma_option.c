/* SPDX-License-Identifier: MIT */
/* Public environment-only settings share scalar types and registry defaults. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/options.h"
#include "src/options-registry.h"

int
test_cms_0181(int argc, char **argv)
{
    sixel_option_argument_schema_t const *schema;
    sixel_suboption_value_t value;
    char diagnostic[128];
    int result;

    if (argc != 3) {
        return 1;
    }
    schema = sixel_option_registry_get(
        SIXEL_OPTION_SCHEMA_COLORSYNC_NATIVE_GAMMA);
    if (schema == NULL || !sixel_option_registry_validate() ||
        schema->optflag != SIXEL_OPTION_NO_OPTFLAG ||
        schema->value_kind != SIXEL_SUBOPTION_VALUE_BOOLEAN ||
        schema->default_policy != SIXEL_OPTION_DEFAULT_FIXED ||
        schema->default_value.int_value != 0 ||
        schema->env_name == NULL ||
        strcmp(schema->env_name,
            "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA") != 0 ||
        sixel_option_encoder_environment_is_present(SIXEL_OPTION_NO_OPTFLAG)) {
        return 1;
    }
    value.int_value = 123;
    result = sixel_option_resolve_scalar_environment(
        schema->option_id, &value, diagnostic, sizeof(diagnostic));
    if (result != atoi(argv[1]) || value.int_value != atoi(argv[2]) ||
        (result == SIXEL_OPTION_ENVIRONMENT_INVALID &&
         strcmp(diagnostic,
             "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA must be 0 or 1.") != 0)) {
        fprintf(stderr, "environment result=%d value=%d\n",
            result, value.int_value);
        return 1;
    }
    /* An internal schema ID cannot introduce a backdoor argument spelling. */
    value.int_value = 123;
    if (SIXEL_SUCCEEDED(sixel_option_parse_scalar_argument(schema->option_id,
            SIXEL_OPTION_SCOPE_ENCODER, "1", &value,
            diagnostic, sizeof(diagnostic))) || value.int_value != 123) {
        return 1;
    }
    /* Existing assigned scalar options continue to accept their arguments. */
    return SIXEL_FAILED(sixel_option_parse_scalar_argument(
        SIXEL_OPTION_SCHEMA_TERMINAL_POLICY, SIXEL_OPTION_SCOPE_ENCODER,
        "1", &value, diagnostic, sizeof(diagnostic))) || value.int_value != 1;
}
