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

/*
 * Suboption values may either be matched against a fixed choice table or
 * accepted as free-form text validated by caller-specific logic.
 */
typedef enum sixel_suboption_value_kind {
    SIXEL_SUBOPTION_VALUE_FREE = 0,
    SIXEL_SUBOPTION_VALUE_CHOICE = 1
} sixel_suboption_value_kind_t;

typedef struct sixel_suboption_choice {
    char const *name;
    int value;
} sixel_suboption_choice_t;

typedef struct sixel_suboption_key {
    char const *name;
    char const *short_name;
    char const *env_name;
    sixel_suboption_value_kind_t value_kind;
    sixel_suboption_choice_t const *choices;
    size_t choice_count;
} sixel_suboption_key_t;

typedef struct sixel_option_value_schema {
    char const *name;
    int value;
    sixel_suboption_key_t const *subkeys;
    size_t subkey_count;
} sixel_option_value_schema_t;

typedef struct sixel_option_argument_schema {
    int optflag;
    char const *option_name;
    sixel_option_value_schema_t const *values;
    size_t value_count;
} sixel_option_argument_schema_t;

typedef struct sixel_suboption_assignment {
    sixel_suboption_key_t const *key_def;
    char const *resolved_key_name;
    char *resolved_value_text;
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

sixel_option_choice_result_t
sixel_option_match_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size);

void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size);

void
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

#endif /* !defined(LIBSIXEL_OPTIONS_H) */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
