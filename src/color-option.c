/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat_stub.h"
#include "color-option.h"
#include "options-registry.h"

SIXELSTATUS
sixel_color_option_parse(char const *text, int decoder, int *base,
                         sixel_color_def_t **definition)
{
    sixel_option_argument_schema_t const *schema;
    sixel_option_argument_resolution_t resolution;
    sixel_color_file_options_t options;
    sixel_color_def_t *candidate;
    unsigned char *bytes;
    unsigned int scope;
    sixel_suboption_target_class_t target;
    FILE *file;
    long length;
    SIXELSTATUS status;

    if (base == NULL || definition == NULL || (decoder != 0 && decoder != 1)) {
        return SIXEL_BAD_ARGUMENT;
    }
    schema = sixel_option_registry_get(decoder
        ? SIXEL_OPTION_SCHEMA_INPUT_COLORSPACE
        : SIXEL_OPTION_SCHEMA_OUTPUT_COLORSPACE);
    scope = decoder ? SIXEL_OPTION_SCOPE_DECODER : SIXEL_OPTION_SCOPE_ENCODER;
    target = decoder ? SIXEL_SUBOPTION_TARGET_INPUT_COLOR_FILE
        : SIXEL_SUBOPTION_TARGET_OUTPUT_COLOR_FILE;
    memset(&resolution, 0, sizeof(resolution));
    memset(&options, 0, sizeof(options));
    candidate = NULL;
    bytes = NULL;
    file = NULL;
    status = sixel_option_parse_argument_with_suboptions(text, schema, scope,
        &resolution, NULL, 0u);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (resolution.resolved_base_value != (decoder
            ? SIXEL_NAMED_COLOR_COUNT : -SIXEL_NAMED_COLOR_COUNT)) {
        goto publish;
    }
    sixel_option_apply_suboption_environment(schema, resolution.base_def,
        scope, &options, target);
    if (!sixel_option_apply_suboption_assignments(&resolution, &options,
            target)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = SIXEL_BAD_ARGUMENT;
    if (options.path == NULL || options.path[0] == '\0' ||
            strcmp(options.path, "-") == 0) {
        sixel_helper_set_additional_message(
            "icc requires path=FILE (PFILE) or its ICC_PATH environment.");
        goto end;
    }
    /* Bound allocation before reading. Never consume image stdin as ICC.
     * Keep a private copy so later path/environment changes cannot alter
     * a running conversion or its PNG snapshot. */
    file = sixel_compat_fopen(options.path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
            (length = ftell(file)) < 128L || length > 16L * 1024L * 1024L ||
            fseek(file, 0, SEEK_SET) != 0) {
        sixel_helper_set_additional_message(
            "cannot read ICC file (expected 128 bytes to 16 MiB).");
        goto end;
    }
    bytes = malloc((size_t)length);
    if (bytes == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (fread(bytes, 1u, (size_t)length, file) != (size_t)length ||
            fgetc(file) != EOF || ferror(file)) {
        sixel_helper_set_additional_message("cannot read complete ICC file.");
        goto end;
    }
    /* DeviceLink, abstract and named-color profiles do not describe this
     * RGB endpoint. Class acceptance is separate from transform support. */
    if (memcmp(bytes + 16, "RGB ", 4) != 0 ||
            (memcmp(bytes + 12, "mntr", 4) != 0 &&
             memcmp(bytes + 12, "scnr", 4) != 0 &&
             memcmp(bytes + 12, "prtr", 4) != 0 &&
             memcmp(bytes + 12, "spac", 4) != 0)) {
        sixel_helper_set_additional_message(
            "ICC endpoint requires an RGB device/color-space profile.");
        goto end;
    }
    candidate = sixel_cdef_icc(bytes, (size_t)length);
    if (!sixel_cdef_output_ready(candidate, 1, 1)) {
        sixel_helper_set_additional_message(
            "ICC endpoint requires supported relative RGB/PCS transforms "
            "in both directions.");
        goto end;
    }
publish:
    *base = resolution.resolved_base_value;
    *definition = candidate;
    candidate = NULL;
    status = SIXEL_OK;
end:
    if (file != NULL) {
        fclose(file);
    }
    free(bytes);
    sixel_cdef_free(candidate);
    sixel_option_free_argument_resolution(&resolution);
    return status;
}
