/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_OPTIONS_H
#define LIBSIXEL_OPTIONS_H

#include <stddef.h>

/*
 * The CLI suggestion features can be driven by environment variables so
 * library embedders retain control.  Each variable accepts "1" to enable
 * the associated hint generator and "0" (or absence) to keep it quiet.
 */
#define SIXEL_OPTION_ENV_PREFIX_SUGGESTIONS "SIXEL_OPTION_PREFIX_SUGGESTIONS"
#define SIXEL_OPTION_ENV_FUZZY_SUGGESTIONS  "SIXEL_OPTION_FUZZY_SUGGESTIONS"
#define SIXEL_OPTION_ENV_PATH_SUGGESTIONS   "SIXEL_OPTION_PATH_SUGGESTIONS"

#define SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD_DEFAULT 24
#define SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD_MAX 441
#define SIXEL_OPTION_DEQUANTIZE_LSO_BASE (-1)

/*
 * The choice descriptor couples the textual prefix with the integral
 * payload stored by the caller.  The helper functions only inspect the
 * `name` field when matching, returning the associated `value` on
 * success.
 */
typedef struct sixel_option_choice {
    char const *name;
    int value;
} sixel_option_choice_t;

typedef enum sixel_option_choice_result {
    SIXEL_OPTION_CHOICE_MATCH = 0,
    SIXEL_OPTION_CHOICE_AMBIGUOUS = 1,
    SIXEL_OPTION_CHOICE_NONE = 2
} sixel_option_choice_result_t;

/* The registry owns both syntax and typed value validation. */
typedef enum sixel_suboption_value_kind {
    SIXEL_SUBOPTION_VALUE_CHOICE = 0,
    SIXEL_SUBOPTION_VALUE_BOOLEAN,
    SIXEL_SUBOPTION_VALUE_INT,
    SIXEL_SUBOPTION_VALUE_UINT,
    SIXEL_SUBOPTION_VALUE_FLOAT,
    SIXEL_SUBOPTION_VALUE_DOUBLE,
    SIXEL_SUBOPTION_VALUE_INT_PAIR,
    SIXEL_SUBOPTION_VALUE_SCALED_U8,
    SIXEL_SUBOPTION_VALUE_STRING,
    SIXEL_SUBOPTION_VALUE_STRUCTURED
} sixel_suboption_value_kind_t;

typedef struct sixel_suboption_choice {
    char const *name;
    int value;
} sixel_suboption_choice_t;

typedef enum sixel_suboption_target_class {
    SIXEL_SUBOPTION_TARGET_NONE = 0,
    SIXEL_SUBOPTION_TARGET_ENCODER,
    SIXEL_SUBOPTION_TARGET_LOADER,
    SIXEL_SUBOPTION_TARGET_DEQUANTIZE
} sixel_suboption_target_class_t;

typedef enum sixel_suboption_storage_kind {
    SIXEL_SUBOPTION_STORAGE_INT = 0,
    SIXEL_SUBOPTION_STORAGE_UINT,
    SIXEL_SUBOPTION_STORAGE_FLOAT,
    SIXEL_SUBOPTION_STORAGE_DOUBLE,
    SIXEL_SUBOPTION_STORAGE_INT_PAIR
} sixel_suboption_storage_kind_t;

/* Environment ranges may preserve historical endpoint clamping. */
typedef enum sixel_suboption_environment_range_policy {
    SIXEL_SUBOPTION_ENV_RANGE_REJECT = 0,
    SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM = 1 << 0,
    SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM = 1 << 1,
    SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM = 1 << 2,
    SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG = 1 << 3,
    SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH = 1 << 4,
    SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH = 1 << 5,
    SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG = 1 << 6
} sixel_suboption_environment_range_policy_t;

#define SIXEL_SUBOPTION_OFFSET_NONE ((size_t)-1)

/*
 * A binding identifier is generated from field tokens, never copied as an
 * environment string.  Lower-level consumers use it to select their registry
 * row without duplicating registry-owned environment metadata.
 */
#define SIXEL_SUBOPTION_STRINGIZE_INNER(token_) #token_
#define SIXEL_SUBOPTION_STRINGIZE(token_) \
    SIXEL_SUBOPTION_STRINGIZE_INNER(token_)
#define SIXEL_SUBOPTION_BINDING_ID_1(first_) \
    SIXEL_SUBOPTION_STRINGIZE(first_)
#define SIXEL_SUBOPTION_BINDING_ID_2(first_, second_) \
    SIXEL_SUBOPTION_STRINGIZE(first_) "," \
    SIXEL_SUBOPTION_STRINGIZE(second_)
#define SIXEL_SUBOPTION_BINDING_ID_3(first_, second_, third_) \
    SIXEL_SUBOPTION_STRINGIZE(first_) "," \
    SIXEL_SUBOPTION_STRINGIZE(second_) "," \
    SIXEL_SUBOPTION_STRINGIZE(third_)

typedef struct sixel_suboption_binding {
    sixel_suboption_target_class_t target_class;
    sixel_suboption_storage_kind_t storage_kind;
    size_t value_offset;
    size_t second_value_offset;
    size_t override_offset;
    size_t mirror_offset;
    /* Stable field tokens let each regression detect a wrong offset row. */
    char const *identifier;
} sixel_suboption_binding_t;

/* Getopt characters are not unique across encoder and decoder contexts. */
typedef enum sixel_option_schema_id {
    SIXEL_OPTION_SCHEMA_DEQUANTIZE = 0,
    SIXEL_OPTION_SCHEMA_DIFFUSION,
    SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
    SIXEL_OPTION_SCHEMA_LUT_POLICY,
    SIXEL_OPTION_SCHEMA_LOADERS,
    SIXEL_OPTION_SCHEMA_COUNT
} sixel_option_schema_id_t;

/*
 * Option scopes keep short-name reuse explicit.  Encoder and decoder options
 * may share a character only when their library and converter scopes do not
 * overlap.
 */
typedef enum sixel_option_scope {
    SIXEL_OPTION_SCOPE_ENCODER = 1 << 0,
    SIXEL_OPTION_SCOPE_DECODER = 1 << 1,
    SIXEL_OPTION_SCOPE_IMG2SIXEL = 1 << 2,
    SIXEL_OPTION_SCOPE_SIXEL2PNG = 1 << 3
} sixel_option_scope_t;

#define SIXEL_OPTION_SCOPE_ALL \
    (SIXEL_OPTION_SCOPE_ENCODER | SIXEL_OPTION_SCOPE_DECODER | \
     SIXEL_OPTION_SCOPE_IMG2SIXEL | SIXEL_OPTION_SCOPE_SIXEL2PNG)

/* Structured arguments either select one base or an ordered base list. */
typedef enum sixel_option_argument_form {
    SIXEL_OPTION_ARGUMENT_SINGLE = 0,
    SIXEL_OPTION_ARGUMENT_LIST
} sixel_option_argument_form_t;

/*
 * Fixed defaults live in the registry.  Owner defaults describe options such
 * as loader order whose default is a runtime-discovered ordered list rather
 * than one base value.
 */
typedef enum sixel_option_default_policy {
    SIXEL_OPTION_DEFAULT_FIXED = 0,
    SIXEL_OPTION_DEFAULT_OWNER
} sixel_option_default_policy_t;

/*
 * Base policies describe initialization which cannot be represented by a
 * single suboption binding.  Keeping the policy beside the base definition
 * prevents encoder code from dispatching on option names.
 */
typedef enum sixel_option_base_policy {
    SIXEL_OPTION_BASE_POLICY_NONE = 0,
    SIXEL_OPTION_BASE_POLICY_DIFFUSION_INTERFRAME,
    SIXEL_OPTION_BASE_POLICY_DIFFUSION_STBN
} sixel_option_base_policy_t;

typedef struct sixel_option_value_schema {
    char const *name;
    int value;
    /* Insert option-wide suboptions after this many base-specific rows. */
    size_t common_suboption_offset;
    sixel_option_base_policy_t base_policy;
} sixel_option_value_schema_t;

typedef struct sixel_suboption_key {
    sixel_option_schema_id_t option_id;
    /* NULL makes the suboption common to every base of the option. */
    sixel_option_value_schema_t const *base_def;
    char const *name;
    /* A compact suboption name is exactly one uppercase ASCII letter. */
    char short_name;
    char const *env_name;
    char const *env_fallback_name;
    char const *env_legacy_name;
    sixel_suboption_value_kind_t value_kind;
    sixel_suboption_choice_t const *choices;
    size_t choice_count;
    /* Environment-only aliases preserve strict CLI vocabularies. */
    sixel_suboption_choice_t const *environment_choices;
    size_t environment_choice_count;
    double minimum;
    double maximum;
    int has_minimum;
    int has_maximum;
    int allow_zero;
    sixel_suboption_environment_range_policy_t environment_range_policy;
    char const *invalid_value_message;
    char const *invalid_value_suffix;
    sixel_suboption_binding_t binding;
} sixel_suboption_key_t;

typedef union sixel_suboption_value {
    int int_value;
    unsigned int uint_value;
    float float_value;
    double double_value;
    char const *string_value;
    struct {
        int first;
        int second;
    } int_pair;
} sixel_suboption_value_t;

/*
 * Boolean controls deliberately accept only the ASCII values "0" and "1".
 * Keeping this parser shared by the registry and direct library fallbacks
 * prevents individual algorithms from growing incompatible boolean aliases.
 */
int
sixel_option_parse_boolean_text(char const *text, int *value);

int
sixel_option_resolve_boolean_environment(char const *name, int fallback);

/*
 * Resolve registered environment variables through registry metadata.  The
 * typed entry points reject a registry kind mismatch before exposing a value
 * to lower-level code.
 */
int
sixel_option_resolve_registered_boolean_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int fallback);

int
sixel_option_resolve_registered_int_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int *value);

int
sixel_option_resolve_registered_uint_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    unsigned int *value);

int
sixel_option_resolve_registered_float_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    float *value);

int
sixel_option_resolve_registered_double_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    double *value);

int
sixel_option_resolve_registered_int_pair_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int *first,
    int *second);

typedef struct sixel_dequantize_options {
    int method;
    int selective_blur_threshold;
} sixel_dequantize_options_t;

typedef struct sixel_option_argument_schema {
    sixel_option_schema_id_t option_id;
    unsigned int scope;
    int optflag;
    /* Canonical long name without leading dashes. */
    char const *option_name;
    sixel_option_argument_form_t argument_form;
    sixel_suboption_value_kind_t value_kind;
    char const *env_name;
    char const *env_fallback_name;
    char const *env_legacy_name;
    sixel_suboption_choice_t const *environment_choices;
    size_t environment_choice_count;
    double minimum;
    double maximum;
    int has_minimum;
    int has_maximum;
    int allow_zero;
    sixel_suboption_environment_range_policy_t environment_range_policy;
    char const *invalid_value_message;
    char const *invalid_value_suffix;
    sixel_option_default_policy_t default_policy;
    sixel_suboption_value_t default_value;
    sixel_option_value_schema_t const *values;
    size_t value_count;
} sixel_option_argument_schema_t;

typedef struct sixel_suboption_assignment {
    sixel_suboption_key_t const *key_def;
    char const *resolved_key_name;
    char *resolved_value_text;
    sixel_suboption_value_t value;
} sixel_suboption_assignment_t;

typedef struct sixel_option_argument_resolution {
    int resolved_base_value;
    sixel_option_value_schema_t const *base_def;
    sixel_suboption_assignment_t *assignments;
    size_t assignment_count;
} sixel_option_argument_resolution_t;

typedef struct sixel_option_argument_list_item {
    sixel_option_argument_resolution_t resolution;
} sixel_option_argument_list_item_t;

/*
 * Parsed argument-list tree for options that accept
 * ITEM[:KEY=VALUE][,ITEM[:KEY=VALUE]...][!].
 * The structure acts as a reusable AST shared across validation and
 * downstream option application paths.
 */
typedef struct sixel_option_argument_list_resolution {
    char *canonical_argument;
    int has_trailing_bang;
    sixel_option_argument_list_item_t *items;
    size_t item_count;
} sixel_option_argument_list_resolution_t;

/*
 * The filesystem validator accepts caller-defined flags controlling special
 * pseudo paths.  Remote URLs, clipboard pseudo paths, and standard input may
 * bypass the on-disk existence check when the corresponding bit is present.
 */
#define SIXEL_OPTION_PATH_ALLOW_STDIN       (1u << 0)
#define SIXEL_OPTION_PATH_ALLOW_CLIPBOARD   (1u << 1)
#define SIXEL_OPTION_PATH_ALLOW_REMOTE      (1u << 2)
#define SIXEL_OPTION_PATH_ALLOW_EMPTY       (1u << 3)

void
sixel_option_apply_cli_suggestion_defaults(void);

int
sixel_option_validate_filesystem_path(
    char const *argument,
    char const *resolved_path,
    unsigned int flags);

SIXEL_INTERNAL_API sixel_option_choice_result_t
sixel_option_match_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size);

SIXEL_INTERNAL_API void
sixel_option_report_invalid_choice(
    char const *base_message,
    char const *suggestions,
    char *buffer,
    size_t buffer_size);

SIXELSTATUS
sixel_option_parse_argument_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    sixel_option_argument_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size);

int
sixel_option_resolve_suboption_environment(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t *value);

int
sixel_option_apply_suboption_value(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value,
    void *target,
    sixel_suboption_target_class_t target_class);

int
sixel_option_apply_suboption_assignments(
    sixel_option_argument_resolution_t const *resolution,
    void *target,
    sixel_suboption_target_class_t target_class);

void
sixel_option_apply_suboption_environment(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    void *target,
    sixel_suboption_target_class_t target_class);

void
sixel_option_reset_suboption_overrides(
    sixel_option_argument_schema_t const *schema,
    void *target,
    sixel_suboption_target_class_t target_class);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_parse_dequantize_argument(
    char const *argument,
    int *method,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_parse_dequantize_argument_with_options(
    char const *argument,
    int *method,
    int *selective_blur_threshold,
    char *diagnostic,
    size_t diagnostic_size);

void
sixel_option_free_argument_resolution(
    sixel_option_argument_resolution_t *resolution);

SIXELSTATUS
sixel_option_parse_argument_list_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size);

void
sixel_option_free_argument_list_resolution(
    sixel_option_argument_list_resolution_t *resolution);

void
sixel_option_init_argument_list_resolution(
    sixel_option_argument_list_resolution_t *resolution);

void
sixel_option_move_argument_list_resolution(
    sixel_option_argument_list_resolution_t *destination,
    sixel_option_argument_list_resolution_t *source);

#endif /* !defined(LIBSIXEL_OPTIONS_H) */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
