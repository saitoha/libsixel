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

typedef enum sixel_option_precision_mode {
    SIXEL_OPTION_PRECISION_AUTO = 0,
    SIXEL_OPTION_PRECISION_8BIT,
    SIXEL_OPTION_PRECISION_FLOAT32
} sixel_option_precision_mode_t;

/* The registry owns both syntax and typed value validation. */
typedef enum sixel_suboption_value_kind {
    SIXEL_SUBOPTION_VALUE_CHOICE = 0,
    SIXEL_SUBOPTION_VALUE_CHOICE_LIST,
    SIXEL_SUBOPTION_VALUE_BOOLEAN,
    SIXEL_SUBOPTION_VALUE_INT,
    SIXEL_SUBOPTION_VALUE_UINT,
    SIXEL_SUBOPTION_VALUE_SIZE,
    SIXEL_SUBOPTION_VALUE_FLOAT,
    SIXEL_SUBOPTION_VALUE_DOUBLE,
    SIXEL_SUBOPTION_VALUE_INT_PAIR,
    SIXEL_SUBOPTION_VALUE_SCALED_U8,
    SIXEL_SUBOPTION_VALUE_STRING,
    SIXEL_SUBOPTION_VALUE_STRUCTURED
} sixel_suboption_value_kind_t;

/*
 * Ordered choice lists use one unsigned value so parsed request state owns no
 * borrowed strings.  Four four-bit values leave room for a count and the
 * trailing-exclusive marker while keeping the storage ABI plain C99.
 */
#define SIXEL_SUBOPTION_CHOICE_LIST_MAX 4u
#define SIXEL_SUBOPTION_CHOICE_LIST_VALUE_MASK 0x0fu
#define SIXEL_SUBOPTION_CHOICE_LIST_COUNT_SHIFT 16u
#define SIXEL_SUBOPTION_CHOICE_LIST_COUNT_MASK 0x07u
#define SIXEL_SUBOPTION_CHOICE_LIST_EXCLUSIVE (1u << 19)

typedef struct sixel_suboption_choice {
    char const *name;
    int value;
} sixel_suboption_choice_t;

typedef enum sixel_suboption_target_class {
    SIXEL_SUBOPTION_TARGET_NONE = 0,
    SIXEL_SUBOPTION_TARGET_ENCODER,
    SIXEL_SUBOPTION_TARGET_DECODER,
    SIXEL_SUBOPTION_TARGET_LOADER,
    SIXEL_SUBOPTION_TARGET_DEQUANTIZE,
    SIXEL_SUBOPTION_TARGET_RUNTIME,
    SIXEL_SUBOPTION_TARGET_DIAGNOSTICS,
    SIXEL_SUBOPTION_TARGET_CLIPBOARD
} sixel_suboption_target_class_t;

typedef enum sixel_suboption_storage_kind {
    SIXEL_SUBOPTION_STORAGE_INT = 0,
    SIXEL_SUBOPTION_STORAGE_UINT,
    SIXEL_SUBOPTION_STORAGE_SIZE,
    SIXEL_SUBOPTION_STORAGE_FLOAT,
    SIXEL_SUBOPTION_STORAGE_DOUBLE,
    SIXEL_SUBOPTION_STORAGE_INT_PAIR,
    SIXEL_SUBOPTION_STORAGE_STRING
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
    SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG = 1 << 6,
    SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY = 1 << 7,
    SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG = 1 << 8,
    SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH = 1 << 9,
    SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX = 1 << 10
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
    SIXEL_OPTION_SCHEMA_SAMPLING_POLICY,
    SIXEL_OPTION_SCHEMA_BINNING_POLICY,
    SIXEL_OPTION_SCHEMA_MERGE_POLICY,
    SIXEL_OPTION_SCHEMA_COVER_POLICY,
    SIXEL_OPTION_SCHEMA_LUT_POLICY,
    SIXEL_OPTION_SCHEMA_LOADERS,
    SIXEL_OPTION_SCHEMA_PRECISION,
    SIXEL_OPTION_SCHEMA_THREADS,
    SIXEL_OPTION_SCHEMA_COLORS,
    SIXEL_OPTION_SCHEMA_START_FRAME,
    SIXEL_OPTION_SCHEMA_GPU_POLICY,
    SIXEL_OPTION_SCHEMA_ALPHA_POLICY,
    SIXEL_OPTION_SCHEMA_6DELTA_THRESHOLD,
    SIXEL_OPTION_SCHEMA_6DELTA_ERROR,
    SIXEL_OPTION_SCHEMA_BGCOLOR,
    SIXEL_OPTION_SCHEMA_RUNTIME_POLICY,
    SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
    SIXEL_OPTION_SCHEMA_LOG_PATH,
    SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY,
    SIXEL_OPTION_SCHEMA_TERMINAL_POLICY,
    SIXEL_OPTION_SCHEMA_COUNT
} sixel_option_schema_id_t;

/*
 * Option scopes describe library consumer domains only.  Converter-specific
 * visibility belongs to each converter's CLI tables and must not leak client
 * executable names into the library registry.
 */
typedef enum sixel_option_scope {
    SIXEL_OPTION_SCOPE_ENCODER = 1 << 0,
    SIXEL_OPTION_SCOPE_DECODER = 1 << 1
} sixel_option_scope_t;

#define SIXEL_OPTION_SCOPE_ALL \
    (SIXEL_OPTION_SCOPE_ENCODER | SIXEL_OPTION_SCOPE_DECODER)

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

/* Choice matching differences are data, including legacy env strictness. */
typedef enum sixel_option_match_flag {
    SIXEL_OPTION_MATCH_EXACT = 0,
    SIXEL_OPTION_MATCH_PREFIX = 1 << 0,
    SIXEL_OPTION_MATCH_CASE_INSENSITIVE = 1 << 1
} sixel_option_match_flag_t;

typedef enum sixel_option_environment_result {
    SIXEL_OPTION_ENVIRONMENT_RANGE = -2,
    SIXEL_OPTION_ENVIRONMENT_INVALID = -1,
    SIXEL_OPTION_ENVIRONMENT_UNSET = 0,
    SIXEL_OPTION_ENVIRONMENT_MATCH = 1
} sixel_option_environment_result_t;

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
    /* A pointer selects one base; NULL with no base_mask selects every base. */
    sixel_option_value_schema_t const *base_def;
    /* Visibility is independent of the structure receiving the value. */
    unsigned int consumer_scope;
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
    /* CLI and environment ranges can differ for compatibility. */
    double minimum;
    double maximum;
    int has_minimum;
    int has_maximum;
    int allow_zero;
    sixel_suboption_environment_range_policy_t environment_range_policy;
    /* Frontend-specific diagnostics remain declarative registry data. */
    char const *invalid_value_message;
    char const *invalid_value_suffix;
    sixel_suboption_binding_t binding;
    /* Optional topic preserves backend diagnostics during central parsing. */
    char const *environment_trace_topic;
    /* Bits are schema value indexes and are exclusive with base_def. */
    unsigned long long base_mask;
} sixel_suboption_key_t;

typedef union sixel_suboption_value {
    int int_value;
    unsigned int uint_value;
    size_t size_value;
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

/*
 * Test controls are intentionally absent from the public option registry.
 * These typed accessors keep their private environment names inside one
 * broker instead of exposing raw string lookups to production consumers.
 */
SIXEL_INTERNAL_API char const *
sixel_test_environment_bash_version(void);
SIXEL_INTERNAL_API int
sixel_test_environment_librsvg_open_failure(void);
SIXEL_INTERNAL_API int
sixel_test_environment_librsvg_write_failure(void);
SIXEL_INTERNAL_API int
sixel_test_environment_librsvg_close_failure(void);
SIXEL_INTERNAL_API int
sixel_test_environment_libwebp_force_rgb(void);
SIXEL_INTERNAL_API int
sixel_test_environment_palette_disable_tables(void);
SIXEL_INTERNAL_API char const *
sixel_test_environment_decoder_paint_thread_create_failure(void);
SIXEL_INTERNAL_API char const *
sixel_test_environment_palette_job_failure(void);
SIXEL_INTERNAL_API char const *
sixel_test_environment_palette_quantizer_failure(void);
SIXEL_INTERNAL_API char const *
sixel_test_environment_palette_quantizer_post_build_failure(void);

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
sixel_option_resolve_registered_choice_list_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    unsigned int *value);

size_t
sixel_option_choice_list_count(unsigned int value);

int
sixel_option_choice_list_value_at(unsigned int value,
                                  size_t index,
                                  int *choice_value);

int
sixel_option_choice_list_is_exclusive(unsigned int value);

sixel_option_environment_result_t
sixel_option_resolve_registered_size_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    size_t *value);

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
sixel_option_resolve_registered_string_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    char const **value);

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

typedef struct sixel_runtime_policy_options {
    int simd_level;
    int simd_level_override;
    size_t colorspace_parallel_min_pixels;
    int colorspace_parallel_min_pixels_override;
    unsigned int parallel_factor;
    int parallel_factor_override;
    int parallel_skew;
    int parallel_skew_override;
    int resize_precision;
    int resize_precision_override;
    size_t scale_parallel_min_bytes;
    int scale_parallel_min_bytes_override;
} sixel_runtime_policy_options_t;

typedef enum sixel_runtime_resize_precision {
    SIXEL_RUNTIME_RESIZE_PRECISION_PRESERVE = 1,
    SIXEL_RUNTIME_RESIZE_PRECISION_LINEAR32,
    SIXEL_RUNTIME_RESIZE_PRECISION_FLOAT_WORK
} sixel_runtime_resize_precision_t;

typedef struct sixel_diagnostics_policy_options {
    int mode;
    int mode_override;
    int quiet;
    int quiet_override;
    int prefix_suggestions;
    int prefix_suggestions_override;
    int fuzzy_suggestions;
    int fuzzy_suggestions_override;
    int path_suggestions;
    int path_suggestions_override;
    int force_colors;
    int force_colors_override;
    int handoff_trace;
    int handoff_trace_override;
    int psd_trace;
    int psd_trace_override;
    int psd_header_only;
    int psd_header_only_override;
    int abort_trace;
    int abort_trace_override;
    int log_lines;
    int log_lines_override;
    char const *trace_topic;
    int trace_topic_override;
    int cli_suggestion_defaults;
} sixel_diagnostics_policy_options_t;

typedef enum sixel_diagnostics_mode {
    SIXEL_DIAGNOSTICS_MODE_HUMAN = 0,
    SIXEL_DIAGNOSTICS_MODE_CODE
} sixel_diagnostics_mode_t;

typedef struct sixel_clipboard_policy_options {
    int backend;
    int backend_override;
    char const *directory;
    int directory_override;
} sixel_clipboard_policy_options_t;

typedef enum sixel_clipboard_backend {
    SIXEL_CLIPBOARD_BACKEND_SYSTEM = 0,
    SIXEL_CLIPBOARD_BACKEND_FILE
} sixel_clipboard_backend_t;

typedef struct sixel_option_argument_schema {
    sixel_option_schema_id_t option_id;
    unsigned int scope;
    int optflag;
    /* Canonical long name without leading dashes. */
    char const *option_name;
    sixel_option_argument_form_t argument_form;
    sixel_suboption_value_kind_t value_kind;
    unsigned int argument_match_flags;
    unsigned int environment_match_flags;
    char const *env_name;
    char const *env_fallback_name;
    char const *env_legacy_name;
    sixel_suboption_choice_t const *environment_choices;
    size_t environment_choice_count;
    double minimum;
    double maximum;
    int has_minimum;
    int has_maximum;
    double environment_minimum;
    double environment_maximum;
    int environment_has_minimum;
    int environment_has_maximum;
    int allow_zero;
    sixel_suboption_environment_range_policy_t environment_range_policy;
    char const *invalid_value_message;
    char const *decoder_invalid_value_message;
    char const *invalid_value_suffix;
    char const *minimum_error_message;
    char const *maximum_error_message;
    char const *environment_invalid_value_message;
    char const *range_error_message;
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
sixel_option_parse_scalar_argument(
    sixel_option_schema_id_t option_id,
    unsigned int consumer_scope,
    char const *argument,
    sixel_suboption_value_t *value,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API int
sixel_option_resolve_scalar_environment(
    sixel_option_schema_id_t option_id,
    sixel_suboption_value_t *value,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API int
sixel_option_argument_environment_is_present(
    sixel_option_schema_id_t option_id);

SIXEL_INTERNAL_API int
sixel_option_loader_osc11_query_environment_is_present(void);

SIXEL_INTERNAL_API int
sixel_option_encoder_environment_is_present(int optflag);

char const *
sixel_option_resolve_argument_environment(
    sixel_option_schema_id_t option_id);

int
sixel_option_resolve_registered_base_environment(
    sixel_option_schema_id_t option_id,
    unsigned int consumer_scope,
    int *value);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_parse_argument_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    sixel_option_argument_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size);

int
sixel_option_resolve_suboption_environment(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t *value);

sixel_option_environment_result_t
sixel_option_resolve_suboption_environment_result(
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
    unsigned int consumer_scope,
    void *target,
    sixel_suboption_target_class_t target_class);

void
sixel_option_reset_suboption_overrides(
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    void *target,
    sixel_suboption_target_class_t target_class);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_apply_runtime_policy_argument(
    char const *argument,
    unsigned int consumer_scope,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_apply_diagnostics_argument(
    char const *argument,
    unsigned int consumer_scope,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_apply_log_path_argument(
    char const *argument,
    unsigned int consumer_scope,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_apply_clipboard_policy_argument(
    char const *argument,
    unsigned int consumer_scope,
    char *diagnostic,
    size_t diagnostic_size);

SIXEL_INTERNAL_API int sixel_clipboard_policy_backend(void);
SIXEL_INTERNAL_API int sixel_clipboard_policy_copy_directory(
    char *buffer,
    size_t buffer_size);

SIXEL_INTERNAL_API int sixel_diagnostics_mode_is_code(void);
SIXEL_INTERNAL_API int sixel_diagnostics_quiet_is_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_prefix_suggestions_are_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_fuzzy_suggestions_are_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_path_suggestions_are_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_force_colors_is_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_handoff_trace_is_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_psd_trace_is_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_psd_header_only_is_enabled(void);
SIXEL_INTERNAL_API int sixel_diagnostics_abort_trace_is_enabled(void);
SIXEL_INTERNAL_API int
sixel_diagnostics_trace_topic_is_enabled(char const *topic);
SIXEL_INTERNAL_API void
sixel_diagnostics_timeline_line_policy(int *enabled, int *stride);

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

SIXEL_INTERNAL_API void
sixel_option_free_argument_resolution(
    sixel_option_argument_resolution_t *resolution);

SIXELSTATUS
sixel_option_parse_argument_list_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
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
