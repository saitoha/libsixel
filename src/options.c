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

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if defined(_WIN32) && HAVE_WINDOWS_H
# include <windows.h>
#endif

#include <sixel.h>
#include "compat_stub.h"
#include "loader-common.h"
#include "options.h"
#include "options-registry.h"
#include "output.h"

/*
 * The option helper entry points centralize prefix matching and
 * diagnostic reporting used by both the encoder and the decoder.  The
 * implementations stay here so the CLI remains thin while the library
 * can share the matching logic.
 */

#define SIXEL_OPTION_CHOICE_SUGGESTION_LIMIT 5u
#define SIXEL_OPTION_CHOICE_SUGGESTION_THRESHOLD 0.6
#define SIXEL_OPTION_CHOICE_SHORT_NAME_LENGTH 3u

#if defined(__clang__)
# if __has_attribute(unused)
#  define SIXEL_OPTION_UNUSED __attribute__((unused))
# else
#  define SIXEL_OPTION_UNUSED
# endif
#elif defined(__GNUC__)
# define SIXEL_OPTION_UNUSED __attribute__((unused))
#else
# define SIXEL_OPTION_UNUSED
#endif

static void
sixel_option_apply_env_default(char const *variable);

static int
sixel_option_environment_is_enabled(char const *variable);

static int
sixel_option_diag_mode_is_code(void);

typedef struct sixel_option_choice_suggestion {
    char const *name;
    double score;
    size_t distance;
} sixel_option_choice_suggestion_t;

static double
sixel_option_normalized_levenshtein(
    char const *lhs,
    char const *rhs,
    size_t *distance_out);

static void
sixel_option_trace_path_probe_begin(
    char const *argument,
    char const *resolved_path,
    unsigned int flags);

static void
sixel_option_trace_path_probe_end(
    char const *argument,
    char const *resolved_path,
    unsigned int flags,
    int result,
    int error_value,
    double elapsed_seconds);

void
sixel_option_apply_cli_suggestion_defaults(void)
{
    sixel_option_apply_env_default(
        SIXEL_OPTION_ENV_PREFIX_SUGGESTIONS);
    sixel_option_apply_env_default(
        SIXEL_OPTION_ENV_FUZZY_SUGGESTIONS);
    /* Path suggestions stay opt-in for the CLI frontends. */
}

static void
sixel_option_apply_env_default(char const *variable)
{
    char const *existing;
    int status;

    existing = NULL;
    status = 0;

    if (variable == NULL) {
        return;
    }

    existing = sixel_compat_getenv(variable);
    if (existing != NULL) {
        return;
    }

    status = sixel_compat_setenv(variable, "1");
    (void)status;
}

static int
sixel_option_environment_is_enabled(char const *variable)
{
    char const *value;

    value = NULL;

    if (variable == NULL) {
        return 0;
    }

    value = sixel_compat_getenv(variable);
    if (value == NULL) {
        return 0;
    }
    if (value[0] == '1' && value[1] == '\0') {
        return 1;
    }

    return 0;
}

static int
sixel_option_diag_mode_is_code(void)
{
    char const *value;

    value = NULL;

    value = sixel_compat_getenv("SIXEL_DIAG_MODE");
    if (value == NULL) {
        return 0;
    }
    if (strcmp(value, "code") == 0) {
        return 1;
    }

    return 0;
}

/*
 * Build a ranked suggestion list for choice arguments.  The helper evaluates
 * every prefix that would be accepted by the prefix-matching logic, compares
 * the user input against those prefixes, and then projects the highest scoring
 * matches back to their canonical option names.  The diagnostic string reuses
 * the comma-separated format consumed by the CLI frontends.
 */
static size_t
sixel_option_collect_choice_suggestions(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    char *diagnostic,
    size_t diagnostic_size);

static int
sixel_option_choice_prefix_accepts(
    sixel_option_choice_t const *choices,
    size_t choice_count,
    char const *name,
    size_t prefix_length);

static void
sixel_option_reset_argument_resolution(
    sixel_option_argument_resolution_t *resolution);

static int
sixel_option_append_suboption_assignment(
    sixel_option_argument_resolution_t *resolution,
    sixel_suboption_key_t const *key_def,
    char const *resolved_value_text,
    sixel_suboption_value_t const *value);

static SIXELSTATUS
sixel_option_parse_choice_list(
    sixel_suboption_key_t const *key_def,
    char const *text,
    sixel_suboption_value_t *value,
    int environment_syntax)
{
    char const *cursor;
    char const *end;
    char const *token_start;
    char const *token_end;
    char const *separator;
    char delimiter;
    size_t count;
    size_t choice_index;
    size_t stored_index;
    size_t token_length;
    size_t name_length;
    unsigned int packed;
    unsigned int stored;
    int candidate;
    int candidate_set;
    int ambiguous;
    int exclusive;

    cursor = text;
    end = NULL;
    token_start = NULL;
    token_end = NULL;
    separator = NULL;
    delimiter = environment_syntax ? ',' : '+';
    count = 0u;
    choice_index = 0u;
    stored_index = 0u;
    token_length = 0u;
    name_length = 0u;
    packed = 0u;
    stored = 0u;
    candidate = 0;
    candidate_set = 0;
    ambiguous = 0;
    exclusive = 0;
    if (key_def == NULL || text == NULL || value == NULL ||
        key_def->choices == NULL || key_def->choice_count == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    end = text + strlen(text);
    while (end > cursor && isspace((unsigned char)end[-1])) {
        --end;
    }
    if (end == cursor) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (end[-1] == '!') {
        exclusive = 1;
        --end;
        while (end > cursor && isspace((unsigned char)end[-1])) {
            --end;
        }
    }
    if (end == cursor) {
        return SIXEL_BAD_ARGUMENT;
    }

    while (cursor < end) {
        token_start = cursor;
        separator = cursor;
        while (separator < end && *separator != delimiter) {
            ++separator;
        }
        token_end = separator;
        while (token_start < token_end &&
               isspace((unsigned char)*token_start)) {
            ++token_start;
        }
        while (token_end > token_start &&
               isspace((unsigned char)token_end[-1])) {
            --token_end;
        }
        if (token_start == token_end) {
            return SIXEL_BAD_ARGUMENT;
        }

        token_length = (size_t)(token_end - token_start);
        candidate = 0;
        candidate_set = 0;
        ambiguous = 0;
        choice_index = 0u;
        while (choice_index < key_def->choice_count) {
            name_length = strlen(key_def->choices[choice_index].name);
            if (name_length >= token_length &&
                memcmp(key_def->choices[choice_index].name,
                       token_start,
                       token_length) == 0 &&
                (environment_syntax == 0 ||
                 name_length == token_length)) {
                if (name_length == token_length) {
                    candidate = key_def->choices[choice_index].value;
                    candidate_set = 1;
                    ambiguous = 0;
                    break;
                }
                if (!candidate_set) {
                    candidate = key_def->choices[choice_index].value;
                    candidate_set = 1;
                } else if (candidate !=
                           key_def->choices[choice_index].value) {
                    ambiguous = 1;
                }
            }
            ++choice_index;
        }
        if (!candidate_set || ambiguous || candidate < 0 ||
            candidate > (int)SIXEL_SUBOPTION_CHOICE_LIST_VALUE_MASK) {
            return SIXEL_BAD_ARGUMENT;
        }
        stored_index = 0u;
        while (stored_index < count) {
            stored = (packed >> (stored_index * 4u)) &
                SIXEL_SUBOPTION_CHOICE_LIST_VALUE_MASK;
            if (stored == (unsigned int)candidate) {
                break;
            }
            ++stored_index;
        }
        if (stored_index == count) {
            if (count >= SIXEL_SUBOPTION_CHOICE_LIST_MAX) {
                return SIXEL_BAD_ARGUMENT;
            }
            packed |= (unsigned int)candidate << (count * 4u);
            ++count;
        }

        if (separator == end) {
            cursor = end;
        } else {
            cursor = separator + 1;
            if (cursor == end && !environment_syntax) {
                return SIXEL_BAD_ARGUMENT;
            }
        }
    }
    if (count == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    packed |= (unsigned int)count <<
        SIXEL_SUBOPTION_CHOICE_LIST_COUNT_SHIFT;
    if (exclusive) {
        packed |= SIXEL_SUBOPTION_CHOICE_LIST_EXCLUSIVE;
    }
    value->uint_value = packed;
    return SIXEL_OK;
}

size_t
sixel_option_choice_list_count(unsigned int value)
{
    return (size_t)((value >> SIXEL_SUBOPTION_CHOICE_LIST_COUNT_SHIFT) &
                    SIXEL_SUBOPTION_CHOICE_LIST_COUNT_MASK);
}

int
sixel_option_choice_list_value_at(unsigned int value,
                                  size_t index,
                                  int *choice_value)
{
    if (choice_value == NULL ||
        index >= sixel_option_choice_list_count(value)) {
        return 0;
    }
    *choice_value = (int)((value >> (index * 4u)) &
        SIXEL_SUBOPTION_CHOICE_LIST_VALUE_MASK);
    return 1;
}

int
sixel_option_choice_list_is_exclusive(unsigned int value)
{
    return (value & SIXEL_SUBOPTION_CHOICE_LIST_EXCLUSIVE) != 0u;
}

static SIXELSTATUS
sixel_option_parse_typed_suboption_value(
    sixel_suboption_key_t const *key_def,
    char const *text,
    sixel_suboption_value_t *value,
    sixel_suboption_environment_range_policy_t range_policy,
    int environment_syntax,
    int report_error,
    int *range_error);

static sixel_option_environment_result_t
sixel_option_parse_environment_value(
    sixel_suboption_key_t const *key_def,
    char const *environment_name,
    char const *text,
    sixel_suboption_value_t *value);

static void
sixel_option_trace_environment_value(
    sixel_suboption_key_t const *key_def,
    char const *environment_name,
    char const *text,
    sixel_suboption_value_t const *value,
    int matched);

static sixel_option_choice_result_t
sixel_option_match_suboption_key(
    char const *token,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    int *matched_index,
    char *diagnostic,
    size_t diagnostic_size);

static sixel_option_choice_result_t
sixel_option_match_suboption_short_key(
    char const *token,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    int *matched_index,
    char const **value_text_out);

static sixel_option_choice_result_t
sixel_option_match_schema_value(
    char const *token,
    sixel_option_value_schema_t const *values,
    size_t value_count,
    int *matched_index,
    char *diagnostic,
    size_t diagnostic_size);

static int
sixel_option_take_argument_resolution(
    sixel_option_argument_list_resolution_t *list_resolution,
    sixel_option_argument_resolution_t *item_resolution);

static size_t
sixel_option_calculate_resolution_text_length(
    sixel_option_argument_resolution_t const *resolution);

static size_t
sixel_option_emit_resolution_text(
    char *buffer,
    size_t offset,
    sixel_option_argument_resolution_t const *resolution);

static SIXELSTATUS
sixel_option_build_canonical_argument(
    sixel_option_argument_list_resolution_t *resolution);

SIXEL_INTERNAL_API sixel_option_choice_result_t
sixel_option_match_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size)
{
    size_t index;
    size_t value_length;
    int candidate_index;
    size_t match_count;
    int base_value;
    int base_value_set;
    int ambiguous_values;
    size_t diag_length;
    size_t copy_length;

    index = 0u;
    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (value == NULL) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    value_length = strlen(value);
    if (value_length == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    candidate_index = (-1);
    match_count = 0u;
    base_value = 0;
    base_value_set = 0;
    ambiguous_values = 0;
    diag_length = 0u;

    while (index < choice_count) {
        if (strncmp(choices[index].name, value, value_length) == 0) {
            if (choices[index].name[value_length] == '\0') {
                *matched_value = choices[index].value;
                return SIXEL_OPTION_CHOICE_MATCH;
            }
            if (!base_value_set) {
                base_value = choices[index].value;
                base_value_set = 1;
            } else if (choices[index].value != base_value) {
                ambiguous_values = 1;
            }
            if (candidate_index == (-1)) {
                candidate_index = (int)index;
            }
            ++match_count;
            if (diagnostic != NULL && diagnostic_size > 0u) {
                if (diag_length > 0u && diag_length + 2u < diagnostic_size) {
                    diagnostic[diag_length] = ',';
                    diagnostic[diag_length + 1u] = ' ';
                    diag_length += 2u;
                    diagnostic[diag_length] = '\0';
                }
                copy_length = strlen(choices[index].name);
                if (copy_length > diagnostic_size - diag_length - 1u) {
                    copy_length = diagnostic_size - diag_length - 1u;
                }
                memcpy(diagnostic + diag_length,
                       choices[index].name,
                       copy_length);
                diag_length += copy_length;
                diagnostic[diag_length] = '\0';
            }
        }
        ++index;
    }

    if (match_count == 0u || candidate_index == (-1)) {
        (void)sixel_option_collect_choice_suggestions(
            value,
            choices,
            choice_count,
            diagnostic,
            diagnostic_size);
        return SIXEL_OPTION_CHOICE_NONE;
    }
    if (!ambiguous_values) {
        *matched_value = choices[candidate_index].value;
        return SIXEL_OPTION_CHOICE_MATCH;
    }

    return SIXEL_OPTION_CHOICE_AMBIGUOUS;
}

SIXEL_INTERNAL_API void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size)
{
    int written;
    int suggestions_enabled;
    int diag_mode_code;
    char const *active_candidates;

    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    diag_mode_code = sixel_option_diag_mode_is_code();
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PREFIX_SUGGESTIONS);
    active_candidates = suggestions_enabled ? candidates : NULL;
    if (diag_mode_code) {
        active_candidates = NULL;
    }
    if (active_candidates != NULL && active_candidates[0] != '\0') {
        written = snprintf(buffer,
                           buffer_size,
                           "ambiguous prefix \"%s\" (matches: \\fB%s\\fP).",
                           value,
                           active_candidates);
    } else {
        written = snprintf(buffer,
                           buffer_size,
                           "ambiguous prefix \"%s\".",
                           value);
    }
    (void) written;
    sixel_helper_set_additional_message(buffer);
}

SIXEL_INTERNAL_API void
sixel_option_report_invalid_choice(
    char const *base_message,
    char const *suggestions,
    char *buffer,
    size_t buffer_size)
{
    int written;
    int diag_mode_code;

    if (base_message == NULL) {
        return;
    }
    diag_mode_code = sixel_option_diag_mode_is_code();

    if (!diag_mode_code && suggestions != NULL && suggestions[0] != '\0'
        && buffer != NULL && buffer_size > 0u) {
        buffer[0] = '\0';
        written = snprintf(buffer,
                           buffer_size,
                           "%s Did you mean: \\fW%s\\fP?",
                           base_message,
                           suggestions);
        if (written < 0) {
            sixel_helper_set_additional_message(base_message);
            return;
        }
        sixel_helper_set_additional_message(buffer);
        return;
    }

    sixel_helper_set_additional_message(base_message);
}

static void
sixel_option_emit_candidate_list_from_subkeys(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    char *buffer,
    size_t buffer_size)
{
    size_t index;
    size_t subkey_count;
    size_t offset;
    size_t available;
    size_t copy_length;
    char const *name;
    sixel_suboption_key_t const *subkey;

    index = 0u;
    subkey_count = 0u;
    offset = 0u;
    available = 0u;
    copy_length = 0u;
    name = NULL;
    subkey = NULL;
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }

    buffer[0] = '\0';
    subkey_count = sixel_option_registry_suboption_count_for_scope(
        schema,
        base_def,
        consumer_scope);
    if (subkey_count == 0u) {
        return;
    }

    while (index < subkey_count && offset + 1u < buffer_size) {
        subkey = sixel_option_registry_suboption_at_for_scope(
            schema,
            base_def,
            consumer_scope,
            index);
        name = subkey != NULL ? subkey->name : NULL;
        if (name == NULL || name[0] == '\0') {
            ++index;
            continue;
        }
        if (offset > 0u) {
            available = buffer_size - offset;
            if (available <= 2u) {
                break;
            }
            buffer[offset++] = ',';
            buffer[offset++] = ' ';
            buffer[offset] = '\0';
        }
        available = buffer_size - offset;
        if (available <= 1u) {
            break;
        }
        copy_length = strlen(name);
        if (copy_length > available - 1u) {
            copy_length = available - 1u;
        }
        memcpy(buffer + offset, name, copy_length);
        offset += copy_length;
        buffer[offset] = '\0';
        ++index;
    }
}

static void
sixel_option_emit_candidate_list_from_choices(
    sixel_suboption_choice_t const *choices,
    size_t choice_count,
    char *buffer,
    size_t buffer_size)
{
    size_t index;
    size_t offset;
    size_t available;
    size_t copy_length;
    char const *name;

    index = 0u;
    offset = 0u;
    available = 0u;
    copy_length = 0u;
    name = NULL;
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }

    buffer[0] = '\0';
    if (choices == NULL || choice_count == 0u) {
        return;
    }

    while (index < choice_count && offset + 1u < buffer_size) {
        name = choices[index].name;
        if (name == NULL || name[0] == '\0') {
            ++index;
            continue;
        }
        if (offset > 0u) {
            available = buffer_size - offset;
            if (available <= 2u) {
                break;
            }
            buffer[offset++] = ',';
            buffer[offset++] = ' ';
            buffer[offset] = '\0';
        }
        available = buffer_size - offset;
        if (available <= 1u) {
            break;
        }
        copy_length = strlen(name);
        if (copy_length > available - 1u) {
            copy_length = available - 1u;
        }
        memcpy(buffer + offset, name, copy_length);
        offset += copy_length;
        buffer[offset] = '\0';
        ++index;
    }
}

static void
sixel_option_emit_candidate_list_from_schema_values(
    sixel_option_value_schema_t const *values,
    size_t value_count,
    char *buffer,
    size_t buffer_size)
{
    size_t index;
    size_t offset;
    size_t available;
    size_t copy_length;
    char const *name;

    index = 0u;
    offset = 0u;
    available = 0u;
    copy_length = 0u;
    name = NULL;
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }

    buffer[0] = '\0';
    if (values == NULL || value_count == 0u) {
        return;
    }

    while (index < value_count && offset + 1u < buffer_size) {
        name = values[index].name;
        if (name == NULL || name[0] == '\0') {
            ++index;
            continue;
        }
        if (offset > 0u) {
            available = buffer_size - offset;
            if (available <= 2u) {
                break;
            }
            buffer[offset++] = ',';
            buffer[offset++] = ' ';
            buffer[offset] = '\0';
        }
        available = buffer_size - offset;
        if (available <= 1u) {
            break;
        }
        copy_length = strlen(name);
        if (copy_length > available - 1u) {
            copy_length = available - 1u;
        }
        memcpy(buffer + offset, name, copy_length);
        offset += copy_length;
        buffer[offset] = '\0';
        ++index;
    }
}

static void
sixel_option_report_unknown_base_value(
    char const *base_token,
    char const *suggestions,
    sixel_option_value_schema_t const *values,
    size_t value_count,
    char *buffer,
    size_t buffer_size)
{
    char candidates[256];
    int written;
    int diag_mode_code;

    candidates[0] = '\0';
    written = 0;
    diag_mode_code = 0;
    sixel_option_emit_candidate_list_from_schema_values(values,
                                                        value_count,
                                                        candidates,
                                                        sizeof(candidates));
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    diag_mode_code = sixel_option_diag_mode_is_code();

    if (diag_mode_code) {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown option base value \"%s\".",
            base_token != NULL ? base_token : "");
        (void)written;
        sixel_helper_set_additional_message(buffer);
        return;
    }

    if (suggestions != NULL && suggestions[0] != '\0') {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown option base value \"%s\". valid values: \\fB%s\\fP. "
            "Did you mean: \\fW%s\\fP?",
            base_token != NULL ? base_token : "",
            candidates[0] != '\0' ? candidates : "(none)",
            suggestions);
    } else {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown option base value \"%s\". valid values: \\fB%s\\fP.",
            base_token != NULL ? base_token : "",
            candidates[0] != '\0' ? candidates : "(none)");
    }
    (void)written;
    sixel_helper_set_additional_message(buffer);
}

static void
sixel_option_report_unknown_suboption_key(
    char const *key_token,
    char const *suggestions,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    char *buffer,
    size_t buffer_size)
{
    /*
     * Keep this list small enough to fit the 512-byte diagnostic buffer
     * together with the fixed message text on strict GCC builds.
     */
    char candidates[384];
    int written;
    int diag_mode_code;

    candidates[0] = '\0';
    written = 0;
    diag_mode_code = 0;
    sixel_option_emit_candidate_list_from_subkeys(schema,
                                                  base_def,
                                                  consumer_scope,
                                                  candidates,
                                                  sizeof(candidates));
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    diag_mode_code = sixel_option_diag_mode_is_code();

    if (diag_mode_code) {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption key \"%s\".",
            key_token != NULL ? key_token : "");
        (void)written;
        sixel_helper_set_additional_message(buffer);
        return;
    }

    if (suggestions != NULL && suggestions[0] != '\0') {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption key \"%s\". valid keys: \\fB%s\\fP. "
            "Did you mean: \\fW%s\\fP?",
            key_token != NULL ? key_token : "",
            candidates[0] != '\0' ? candidates : "(none)",
            suggestions);
    } else {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption key \"%s\". valid keys: \\fB%s\\fP.",
            key_token != NULL ? key_token : "",
            candidates[0] != '\0' ? candidates : "(none)");
    }
    (void)written;
    sixel_helper_set_additional_message(buffer);
}

static void
sixel_option_report_unknown_suboption_value(
    char const *key_name,
    char const *value_token,
    char const *suggestions,
    sixel_suboption_choice_t const *choices,
    size_t choice_count,
    char *buffer,
    size_t buffer_size)
{
    char candidates[192];
    int written;
    int diag_mode_code;

    candidates[0] = '\0';
    written = 0;
    diag_mode_code = 0;
    sixel_option_emit_candidate_list_from_choices(choices,
                                                  choice_count,
                                                  candidates,
                                                  sizeof(candidates));
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    diag_mode_code = sixel_option_diag_mode_is_code();

    if (diag_mode_code) {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption value \"%s\" for key \"%s\".",
            value_token != NULL ? value_token : "",
            key_name != NULL ? key_name : "");
        (void)written;
        sixel_helper_set_additional_message(buffer);
        return;
    }

    if (suggestions != NULL && suggestions[0] != '\0') {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption value \"%s\" for key \"%s\". "
            "valid values: \\fB%s\\fP. Did you mean: \\fW%s\\fP?",
            value_token != NULL ? value_token : "",
            key_name != NULL ? key_name : "",
            candidates[0] != '\0' ? candidates : "(none)",
            suggestions);
    } else {
        written = snprintf(
            buffer,
            buffer_size,
            "unknown suboption value \"%s\" for key \"%s\". "
            "valid values: \\fB%s\\fP.",
            value_token != NULL ? value_token : "",
            key_name != NULL ? key_name : "",
            candidates[0] != '\0' ? candidates : "(none)");
    }
    (void)written;
    sixel_helper_set_additional_message(buffer);
}

SIXELSTATUS
sixel_option_parse_argument_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    sixel_option_argument_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;
    char *work;
    char *base_token;
    char *cursor;
    char *entry_end;
    char *equal_pos;
    int value_index;
    int key_index;
    int value_choice;
    size_t index;
    size_t subkey_count;
    sixel_option_choice_result_t match_result;
    sixel_suboption_key_t const *key_def;
    char const *value_text;
    char const *resolved_text;
    sixel_suboption_value_t typed_value;
    char match_message[512];

    status = SIXEL_OK;
    work = NULL;
    base_token = NULL;
    cursor = NULL;
    entry_end = NULL;
    equal_pos = NULL;
    value_index = 0;
    key_index = 0;
    value_choice = 0;
    index = 0u;
    subkey_count = 0u;
    match_result = SIXEL_OPTION_CHOICE_NONE;
    key_def = NULL;
    value_text = NULL;
    resolved_text = NULL;
    memset(&typed_value, 0, sizeof(typed_value));
    match_message[0] = '\0';

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (resolution == NULL || schema == NULL || argument == NULL ||
        consumer_scope == 0u ||
        (consumer_scope & ~schema->scope) != 0u ||
        ((consumer_scope & SIXEL_OPTION_SCOPE_ENCODER_FAMILY) != 0u &&
         (consumer_scope & SIXEL_OPTION_SCOPE_DECODER_FAMILY) != 0u)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_option_registry_validate()) {
        sixel_helper_set_additional_message(
            "invalid suboption registry metadata.");
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_option_reset_argument_resolution(resolution);

    work = (char *)malloc(strlen(argument) + 1u);
    if (work == NULL) {
        sixel_helper_set_additional_message(
            "sixel_option_parse_argument_with_suboptions: malloc failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    (void)sixel_compat_strcpy(work, strlen(argument) + 1u, argument);

    base_token = work;
    cursor = strchr(work, ':');
    if (cursor != NULL) {
        *cursor = '\0';
        ++cursor;
    }

    match_result = sixel_option_match_schema_value(
        base_token,
        schema->values,
        schema->value_count,
        &value_index,
        diagnostic,
        diagnostic_size);
    if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
        sixel_option_report_ambiguous_prefix(base_token,
                                             diagnostic,
                                             match_message,
                                             sizeof(match_message));
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (match_result != SIXEL_OPTION_CHOICE_MATCH) {
        sixel_option_report_unknown_base_value(
            base_token,
            diagnostic,
            schema->values,
            schema->value_count,
            match_message,
            sizeof(match_message));
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    resolution->resolved_base_value = schema->values[value_index].value;
    resolution->base_def = schema->values + value_index;
    subkey_count = sixel_option_registry_suboption_count_for_scope(
        schema,
        resolution->base_def,
        consumer_scope);

    while (cursor != NULL && *cursor != '\0') {
        entry_end = strchr(cursor, ':');
        if (entry_end != NULL) {
            *entry_end = '\0';
        }
        equal_pos = strchr(cursor, '=');
        /*
         * Long keys keep the explicit key=value spelling.  Short keys use
         * the compact KEYvalue form so abbreviated long keys cannot collide
         * with the short-key namespace.
         */
        if (equal_pos != NULL) {
            if (equal_pos == cursor || equal_pos[1] == '\0') {
                sixel_helper_set_additional_message(
                    "suboption must be written as key=value or KEYvalue.");
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            *equal_pos = '\0';
            value_text = equal_pos + 1;
            match_result = sixel_option_match_suboption_key(
                cursor,
                schema,
                resolution->base_def,
                consumer_scope,
                &key_index,
                diagnostic,
                diagnostic_size);
        } else {
            match_result = sixel_option_match_suboption_short_key(
                cursor,
                schema,
                resolution->base_def,
                consumer_scope,
                &key_index,
                &value_text);
        }
        if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
            sixel_option_report_ambiguous_prefix(cursor,
                                                 diagnostic,
                                                 match_message,
                                                 sizeof(match_message));
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        if (match_result != SIXEL_OPTION_CHOICE_MATCH) {
            sixel_option_report_unknown_suboption_key(
                cursor,
                diagnostic,
                schema,
                resolution->base_def,
                consumer_scope,
                match_message,
                sizeof(match_message));
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }

        if (key_index < 0 || (size_t)key_index >= subkey_count) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        key_def = sixel_option_registry_suboption_at_for_scope(
            schema,
            resolution->base_def,
            consumer_scope,
            (size_t)key_index);
        if (key_def == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        resolved_text = value_text;
        if (key_def->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE) {
            match_result = sixel_option_match_choice(
                value_text,
                (sixel_option_choice_t const *)key_def->choices,
                key_def->choice_count,
                &value_choice,
                diagnostic,
                diagnostic_size);
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value_text,
                                                     diagnostic,
                                                     match_message,
                                                     sizeof(match_message));
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            if (match_result != SIXEL_OPTION_CHOICE_MATCH) {
                sixel_option_report_unknown_suboption_value(
                    key_def->name,
                    value_text,
                    diagnostic,
                    key_def->choices,
                    key_def->choice_count,
                    match_message,
                    sizeof(match_message));
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            index = 0u;
            while (index < key_def->choice_count) {
                if (key_def->choices[index].value == value_choice) {
                    resolved_text = key_def->choices[index].name;
                    break;
                }
                ++index;
            }
            typed_value.int_value = value_choice;
        } else {
            status = sixel_option_parse_typed_suboption_value(
                key_def,
            value_text,
            &typed_value,
            SIXEL_SUBOPTION_ENV_RANGE_REJECT,
            0,
            1,
            NULL);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
        }

        if (!sixel_option_append_suboption_assignment(
                resolution,
                key_def,
                resolved_text,
                &typed_value)) {
            sixel_helper_set_additional_message(
                "failed to store parsed suboption assignment.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }

        if (entry_end == NULL) {
            cursor = NULL;
        } else {
            cursor = entry_end + 1;
        }
    }

cleanup:
    free(work);
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_resolution(resolution);
    }
    return status;
}

void
sixel_option_free_argument_resolution(
    sixel_option_argument_resolution_t *resolution)
{
    size_t index;

    index = 0u;
    if (resolution == NULL) {
        return;
    }

    while (index < resolution->assignment_count) {
        free(resolution->assignments[index].resolved_value_text);
        resolution->assignments[index].resolved_value_text = NULL;
        ++index;
    }
    free(resolution->assignments);
    sixel_option_reset_argument_resolution(resolution);
}

int
sixel_option_resolve_registered_base_environment(
    sixel_option_schema_id_t option_id,
    unsigned int consumer_scope,
    int *value)
{
    SIXELSTATUS status;
    char const *text;
    sixel_option_argument_schema_t const *schema;
    sixel_option_argument_resolution_t resolution;

    status = SIXEL_OK;
    text = NULL;
    schema = NULL;
    memset(&resolution, 0, sizeof(resolution));
    if (value == NULL) {
        return 0;
    }

    schema = sixel_option_registry_get(option_id);
    text = sixel_option_resolve_argument_environment(option_id);
    if (schema == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }
    status = sixel_option_parse_argument_with_suboptions(
        text,
        schema,
        consumer_scope,
        &resolution,
        NULL,
        0u);
    if (SIXEL_FAILED(status)) {
        return 0;
    }

    *value = resolution.resolved_base_value;
    sixel_option_free_argument_resolution(&resolution);
    return 1;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_parse_dequantize_argument_with_options(
    char const *argument,
    int *method,
    int *selective_blur_threshold,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;
    sixel_option_argument_resolution_t resolution;
    sixel_option_argument_schema_t const *schema;
    sixel_dequantize_options_t options;

    status = SIXEL_OK;
    schema = NULL;
    options.method = SIXEL_DEQUANTIZE_NONE;
    options.selective_blur_threshold =
        SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD_DEFAULT;
    memset(&resolution, 0, sizeof(resolution));

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (method == NULL || argument == NULL || argument[0] == '\0') {
        sixel_helper_set_additional_message(
            "-d/--dequantize requires a dequantize method.");
        return SIXEL_BAD_ARGUMENT;
    }

    schema = sixel_option_registry_get(SIXEL_OPTION_SCHEMA_DEQUANTIZE);
    status = sixel_option_parse_argument_with_suboptions(
        argument,
        schema,
        SIXEL_OPTION_SCOPE_DECODER,
        &resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    options.method = resolution.resolved_base_value;
    sixel_option_apply_suboption_environment(
        schema,
        resolution.base_def,
        SIXEL_OPTION_SCOPE_DECODER,
        &options,
        SIXEL_SUBOPTION_TARGET_DEQUANTIZE);
    if (!sixel_option_apply_suboption_assignments(
            &resolution,
            &options,
            SIXEL_SUBOPTION_TARGET_DEQUANTIZE)) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (options.method == SIXEL_OPTION_DEQUANTIZE_LSO_BASE) {
        sixel_helper_set_additional_message(
            "lso_undither requires Vfs or Vlight.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    *method = options.method;
    if (selective_blur_threshold != NULL) {
        *selective_blur_threshold = options.selective_blur_threshold;
    }

cleanup:
    sixel_option_free_argument_resolution(&resolution);
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_option_parse_dequantize_argument(
    char const *argument,
    int *method,
    char *diagnostic,
    size_t diagnostic_size)
{
    return sixel_option_parse_dequantize_argument_with_options(
        argument,
        method,
        NULL,
        diagnostic,
        diagnostic_size);
}

static int
sixel_option_argument_final_bang_is_choice_list(
    char const *argument,
    char const *argument_end,
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope)
{
    char const *item_start;
    char const *cursor;
    char *item;
    size_t length;
    SIXELSTATUS status;
    sixel_option_argument_resolution_t resolution;
    sixel_suboption_key_t const *key_def;
    int result;

    item_start = argument;
    cursor = argument;
    item = NULL;
    length = 0u;
    status = SIXEL_BAD_ARGUMENT;
    memset(&resolution, 0, sizeof(resolution));
    key_def = NULL;
    result = 0;
    if (argument == NULL || argument_end == NULL || schema == NULL ||
        argument_end <= argument) {
        return 0;
    }
    while (cursor < argument_end) {
        if (*cursor == ',') {
            item_start = cursor + 1;
        }
        ++cursor;
    }
    while (item_start < argument_end &&
           isspace((unsigned char)*item_start)) {
        ++item_start;
    }
    length = (size_t)(argument_end - item_start);
    item = (char *)malloc(length + 1u);
    if (item == NULL) {
        return 0;
    }
    memcpy(item, item_start, length);
    item[length] = '\0';
    status = sixel_option_parse_argument_with_suboptions(
        item,
        schema,
        consumer_scope,
        &resolution,
        NULL,
        0u);
    if (SIXEL_SUCCEEDED(status) && resolution.assignment_count > 0u) {
        key_def = resolution.assignments[
            resolution.assignment_count - 1u].key_def;
        result = key_def != NULL &&
            key_def->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE_LIST;
    }
    sixel_option_free_argument_resolution(&resolution);
    free(item);
    return result;
}

SIXELSTATUS
sixel_option_parse_argument_list_with_suboptions(
    char const *argument,
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;
    char const *cursor;
    char const *token_start;
    char const *token_end;
    char const *argument_end;
    size_t token_length;
    int has_trailing_bang;
    char *token_buffer;
    sixel_option_argument_resolution_t item_resolution;

    status = SIXEL_OK;
    cursor = NULL;
    token_start = NULL;
    token_end = NULL;
    argument_end = NULL;
    token_length = 0u;
    has_trailing_bang = 0;
    token_buffer = NULL;
    item_resolution.resolved_base_value = 0;
    item_resolution.base_def = NULL;
    item_resolution.assignments = NULL;
    item_resolution.assignment_count = 0u;

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (schema == NULL || resolution == NULL || consumer_scope == 0u ||
        (consumer_scope & ~schema->scope) != 0u ||
        ((consumer_scope & SIXEL_OPTION_SCOPE_ENCODER_FAMILY) != 0u &&
         (consumer_scope & SIXEL_OPTION_SCOPE_DECODER_FAMILY) != 0u)) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_option_init_argument_list_resolution(resolution);

    if (argument == NULL) {
        return SIXEL_OK;
    }
    if (argument[0] == '\0') {
        return sixel_option_build_canonical_argument(resolution);
    }

    argument_end = argument + strlen(argument);
    while (argument_end > argument &&
           isspace((unsigned char)argument_end[-1])) {
        --argument_end;
    }
    if (argument_end > argument && argument_end[-1] == '!' &&
        ((argument_end - argument >= 2 && argument_end[-2] == '!') ||
         !sixel_option_argument_final_bang_is_choice_list(
             argument,
             argument_end - 1,
             schema,
             consumer_scope))) {
        has_trailing_bang = 1;
        --argument_end;
        while (argument_end > argument &&
               isspace((unsigned char)argument_end[-1])) {
            --argument_end;
        }
    }

    if (argument_end == argument) {
        sixel_helper_set_additional_message(
            "option argument list requires at least one item.");
        return SIXEL_BAD_ARGUMENT;
    }

    cursor = argument;
    token_start = argument;
    while (cursor <= argument_end) {
        if (cursor == argument_end || *cursor == ',') {
            token_end = cursor;
            while (token_start < token_end &&
                   isspace((unsigned char)*token_start)) {
                ++token_start;
            }
            while (token_end > token_start &&
                   isspace((unsigned char)token_end[-1])) {
                --token_end;
            }
            token_length = (size_t)(token_end - token_start);
            if (token_length > 0u) {
                token_buffer = (char *)malloc(token_length + 1u);
                if (token_buffer == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_option_parse_argument_list_with_suboptions: "
                        "malloc failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                memcpy(token_buffer, token_start, token_length);
                token_buffer[token_length] = '\0';

                status = sixel_option_parse_argument_with_suboptions(
                    token_buffer,
                    schema,
                    consumer_scope,
                    &item_resolution,
                    diagnostic,
                    diagnostic_size);
                free(token_buffer);
                token_buffer = NULL;
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }

                if (!sixel_option_take_argument_resolution(resolution,
                                                           &item_resolution)) {
                    sixel_helper_set_additional_message(
                        "sixel_option_parse_argument_list_with_suboptions: "
                        "failed to store parsed item.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
            }
            token_start = cursor + 1;
        }
        ++cursor;
    }

    if (resolution->item_count == 0u) {
        sixel_helper_set_additional_message(
            "option argument list requires at least one item.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    resolution->has_trailing_bang = has_trailing_bang;
    status = sixel_option_build_canonical_argument(resolution);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

cleanup:
    free(token_buffer);
    sixel_option_free_argument_resolution(&item_resolution);
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_list_resolution(resolution);
    }
    return status;
}

void
sixel_option_free_argument_list_resolution(
    sixel_option_argument_list_resolution_t *resolution)
{
    size_t index;

    index = 0u;
    if (resolution == NULL) {
        return;
    }

    while (index < resolution->item_count) {
        sixel_option_free_argument_resolution(
            &resolution->items[index].resolution);
        ++index;
    }
    free(resolution->items);
    free(resolution->canonical_argument);
    sixel_option_init_argument_list_resolution(resolution);
}

void
sixel_option_init_argument_list_resolution(
    sixel_option_argument_list_resolution_t *resolution)
{
    if (resolution == NULL) {
        return;
    }

    resolution->canonical_argument = NULL;
    resolution->has_trailing_bang = 0;
    resolution->items = NULL;
    resolution->item_count = 0u;
}

void
sixel_option_move_argument_list_resolution(
    sixel_option_argument_list_resolution_t *destination,
    sixel_option_argument_list_resolution_t *source)
{
    if (destination == NULL || source == NULL || destination == source) {
        return;
    }

    sixel_option_free_argument_list_resolution(destination);
    *destination = *source;
    sixel_option_init_argument_list_resolution(source);
}

static int
sixel_option_take_argument_resolution(
    sixel_option_argument_list_resolution_t *list_resolution,
    sixel_option_argument_resolution_t *item_resolution)
{
    sixel_option_argument_list_item_t *grown;

    grown = NULL;
    if (list_resolution == NULL || item_resolution == NULL) {
        return 0;
    }

    grown = (sixel_option_argument_list_item_t *)realloc(
        list_resolution->items,
        (list_resolution->item_count + 1u)
            * sizeof(sixel_option_argument_list_item_t));
    if (grown == NULL) {
        return 0;
    }
    list_resolution->items = grown;
    list_resolution->items[list_resolution->item_count].resolution =
        *item_resolution;
    list_resolution->item_count += 1u;
    sixel_option_reset_argument_resolution(item_resolution);

    return 1;
}

static size_t
sixel_option_calculate_resolution_text_length(
    sixel_option_argument_resolution_t const *resolution)
{
    size_t index;
    size_t length;

    index = 0u;
    length = 0u;
    if (resolution == NULL || resolution->base_def == NULL ||
        resolution->base_def->name == NULL) {
        return 0u;
    }

    length += strlen(resolution->base_def->name);
    while (index < resolution->assignment_count) {
        length += 2u;
        length += strlen(resolution->assignments[index].resolved_key_name);
        length += strlen(resolution->assignments[index].resolved_value_text);
        ++index;
    }

    return length;
}

static size_t
sixel_option_emit_resolution_text(
    char *buffer,
    size_t offset,
    sixel_option_argument_resolution_t const *resolution)
{
    size_t index;
    size_t length;

    index = 0u;
    length = 0u;
    if (buffer == NULL || resolution == NULL || resolution->base_def == NULL) {
        return offset;
    }

    length = strlen(resolution->base_def->name);
    memcpy(buffer + offset, resolution->base_def->name, length);
    offset += length;

    while (index < resolution->assignment_count) {
        buffer[offset] = ':';
        ++offset;
        length = strlen(resolution->assignments[index].resolved_key_name);
        memcpy(buffer + offset,
               resolution->assignments[index].resolved_key_name,
               length);
        offset += length;
        buffer[offset] = '=';
        ++offset;
        length = strlen(resolution->assignments[index].resolved_value_text);
        memcpy(buffer + offset,
               resolution->assignments[index].resolved_value_text,
               length);
        offset += length;
        ++index;
    }

    return offset;
}

static SIXELSTATUS
sixel_option_build_canonical_argument(
    sixel_option_argument_list_resolution_t *resolution)
{
    size_t index;
    size_t length;
    size_t offset;

    index = 0u;
    length = 0u;
    offset = 0u;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    free(resolution->canonical_argument);
    resolution->canonical_argument = NULL;
    if (resolution->item_count == 0u) {
        /*
         * Keep successful parses self-contained: callers that round-trip or
         * duplicate the canonical text expect a non-NULL pointer on success.
         * The empty list canonicalizes to the empty string.
         */
        resolution->canonical_argument = (char *)malloc(1u);
        if (resolution->canonical_argument == NULL) {
            sixel_helper_set_additional_message(
                "sixel_option_build_canonical_argument: malloc failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        resolution->canonical_argument[0] = '\0';
        return SIXEL_OK;
    }

    while (index < resolution->item_count) {
        if (index > 0u) {
            length += 1u;
        }
        length += sixel_option_calculate_resolution_text_length(
            &resolution->items[index].resolution);
        ++index;
    }
    if (resolution->has_trailing_bang) {
        length += 1u;
    }

    resolution->canonical_argument = (char *)malloc(length + 1u);
    if (resolution->canonical_argument == NULL) {
        sixel_helper_set_additional_message(
            "sixel_option_build_canonical_argument: malloc failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    index = 0u;
    while (index < resolution->item_count) {
        if (index > 0u) {
            resolution->canonical_argument[offset] = ',';
            ++offset;
        }
        offset = sixel_option_emit_resolution_text(
            resolution->canonical_argument,
            offset,
            &resolution->items[index].resolution);
        ++index;
    }
    if (resolution->has_trailing_bang) {
        resolution->canonical_argument[offset] = '!';
        ++offset;
    }
    resolution->canonical_argument[offset] = '\0';

    return SIXEL_OK;
}

static void
sixel_option_reset_argument_resolution(
    sixel_option_argument_resolution_t *resolution)
{
    if (resolution == NULL) {
        return;
    }
    resolution->resolved_base_value = 0;
    resolution->base_def = NULL;
    resolution->assignments = NULL;
    resolution->assignment_count = 0u;
}

/*
 * Parse every non-enumerated suboption value through metadata in the
 * registry.  Callers may suppress diagnostics when probing environment
 * defaults.  Ordered choice lists retain their comma-separated environment
 * grammar while using plus signs inside structured CLI arguments.
 */
static SIXELSTATUS
sixel_option_parse_typed_suboption_value(
    sixel_suboption_key_t const *key_def,
    char const *text,
    sixel_suboption_value_t *value,
    sixel_suboption_environment_range_policy_t range_policy,
    int environment_syntax,
    int report_error,
    int *range_error)
{
    char *endptr;
    char const *comma;
    long parsed_int;
    unsigned long parsed_ulong;
    unsigned long long parsed_uint;
    char const *digit;
    double parsed_double;
    double scaled;
    char message[256];
    int valid;

    endptr = NULL;
    comma = NULL;
    parsed_int = 0L;
    parsed_ulong = 0UL;
    parsed_uint = 0ULL;
    digit = NULL;
    parsed_double = 0.0;
    scaled = 0.0;
    message[0] = '\0';
    valid = 0;
    if (range_error != NULL) {
        *range_error = 0;
    }

    if (key_def == NULL || text == NULL || text[0] == '\0' ||
        value == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (key_def->value_kind) {
    case SIXEL_SUBOPTION_VALUE_CHOICE_LIST:
        valid = SIXEL_SUCCEEDED(sixel_option_parse_choice_list(
            key_def,
            text,
            value,
            environment_syntax));
        break;
    case SIXEL_SUBOPTION_VALUE_BOOLEAN:
        valid = sixel_option_parse_boolean_text(text, &value->int_value);
        break;
    case SIXEL_SUBOPTION_VALUE_INT:
        errno = 0;
        parsed_int = strtol(text, &endptr, 10);
        valid = endptr != text && endptr != NULL && endptr[0] == '\0' &&
            errno != ERANGE && parsed_int >= (long)INT_MIN &&
            parsed_int <= (long)INT_MAX;
        if (valid && key_def->has_minimum &&
            (double)parsed_int < key_def->minimum &&
            (range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0) {
            parsed_int = (long)key_def->minimum;
        }
        if (valid && key_def->has_maximum &&
            (double)parsed_int > key_def->maximum &&
            (range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0) {
            parsed_int = (long)key_def->maximum;
        }
        if (valid && key_def->has_minimum) {
            valid = (double)parsed_int >= key_def->minimum;
        }
        if (valid && key_def->has_maximum) {
            valid = (double)parsed_int <= key_def->maximum;
        }
        if (valid) {
            value->int_value = (int)parsed_int;
        }
        break;
    case SIXEL_SUBOPTION_VALUE_UINT:
        if ((range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG) != 0) {
            errno = 0;
            parsed_int = strtol(text, &endptr, 10);
            valid = endptr != text && endptr != NULL &&
                endptr[0] == '\0' && errno != ERANGE;
            if (valid && key_def->has_minimum &&
                (double)parsed_int < key_def->minimum &&
                (range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0) {
                parsed_int = (long)key_def->minimum;
            }
            if (valid && key_def->has_maximum &&
                (double)parsed_int > key_def->maximum &&
                (range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0) {
                parsed_int = (long)key_def->maximum;
            }
            valid = valid && parsed_int >= 0L &&
                (unsigned long)parsed_int <= (unsigned long)UINT_MAX;
            if (valid) {
                parsed_uint = (unsigned long long)parsed_int;
            }
        } else {
            valid = 1;
            if ((range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY) != 0) {
                digit = text;
                while (*digit != '\0') {
                    if (*digit < '0' || *digit > '9') {
                        valid = 0;
                        break;
                    }
                    ++digit;
                }
            }
            if (!valid) {
                break;
            }
            errno = 0;
            if ((range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG) != 0) {
                parsed_ulong = strtoul(text, &endptr, 10);
                parsed_uint = (unsigned long long)parsed_ulong;
            } else {
                parsed_uint = strtoull(text, &endptr, 10);
            }
            valid = endptr != text && endptr != NULL &&
                endptr[0] == '\0' && errno != ERANGE;
            if (valid && text[0] == '-' &&
                (range_policy &
                 (SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                  SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH)) == 0) {
                valid = 0;
            }
            if (valid &&
                (range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH) != 0 &&
                parsed_uint > (unsigned long long)UINT_MAX) {
                valid = 0;
            }
            if (valid &&
                (range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH) != 0 &&
                parsed_uint > (unsigned long long)UINT_MAX) {
                parsed_uint = (unsigned long long)UINT_MAX;
            }
        }
        if (valid && key_def->allow_zero && parsed_uint == 0ULL) {
            value->uint_value = 0u;
            return SIXEL_OK;
        }
        if (valid && key_def->has_minimum &&
            (double)parsed_uint < key_def->minimum &&
            ((range_policy & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0 ||
             ((range_policy &
               SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM) != 0 &&
              parsed_uint > 0ULL))) {
            parsed_uint = (unsigned long long)key_def->minimum;
        }
        if (valid && key_def->has_maximum &&
            (double)parsed_uint > key_def->maximum &&
            (range_policy & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0) {
            parsed_uint = (unsigned long long)key_def->maximum;
        }
        valid = valid && parsed_uint <= (unsigned long long)UINT_MAX;
        if (valid && key_def->has_minimum) {
            valid = (double)parsed_uint >= key_def->minimum;
        }
        if (valid && key_def->has_maximum) {
            valid = (double)parsed_uint <= key_def->maximum;
        }
        if (valid) {
            value->uint_value = (unsigned int)parsed_uint;
        }
        break;
    case SIXEL_SUBOPTION_VALUE_SIZE:
        errno = 0;
        if ((range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG) != 0) {
            parsed_ulong = strtoul(text, &endptr, 10);
            parsed_uint = (unsigned long long)parsed_ulong;
        } else {
            parsed_uint = strtoull(text, &endptr, 10);
        }
        valid = endptr != text && endptr != NULL &&
            endptr[0] == '\0';
        if (valid && errno == ERANGE &&
            (range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG) == 0) {
            valid = 0;
        } else if (valid &&
                   parsed_uint > (unsigned long long)SIZE_MAX) {
            if (range_error != NULL) {
                *range_error = 1;
            }
            valid = 0;
        }
        if (valid && key_def->has_minimum &&
            (double)parsed_uint < key_def->minimum) {
            valid = 0;
        }
        if (valid && key_def->has_maximum &&
            (double)parsed_uint > key_def->maximum) {
            valid = 0;
        }
        if (valid) {
            value->size_value = (size_t)parsed_uint;
        }
        break;
    case SIXEL_SUBOPTION_VALUE_FLOAT:
    case SIXEL_SUBOPTION_VALUE_DOUBLE:
    case SIXEL_SUBOPTION_VALUE_SCALED_U8:
        errno = 0;
        parsed_double = strtod(text, &endptr);
        valid = endptr != text && endptr != NULL && endptr[0] == '\0' &&
            errno != ERANGE;
        if (valid && (key_def->has_minimum || key_def->has_maximum) &&
            parsed_double != parsed_double) {
            valid = 0;
        }
        if (valid && key_def->has_minimum &&
            parsed_double < key_def->minimum &&
            (range_policy & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0) {
            parsed_double = key_def->minimum;
        }
        if (valid && key_def->has_maximum &&
            parsed_double > key_def->maximum &&
            (range_policy & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0) {
            parsed_double = key_def->maximum;
        }
        if (valid && key_def->has_minimum) {
            valid = parsed_double >= key_def->minimum;
        }
        if (valid && key_def->has_maximum) {
            valid = parsed_double <= key_def->maximum;
        }
        if (valid && key_def->value_kind == SIXEL_SUBOPTION_VALUE_FLOAT) {
            value->float_value = (float)parsed_double;
        } else if (valid &&
                   key_def->value_kind == SIXEL_SUBOPTION_VALUE_DOUBLE) {
            value->double_value = parsed_double;
        } else if (valid) {
            scaled = parsed_double * 255.0;
            if (scaled > 255.0) {
                scaled = 255.0;
            }
            value->int_value = (int)(scaled + 0.5);
        }
        break;
    case SIXEL_SUBOPTION_VALUE_INT_PAIR:
        comma = strchr(text, ',');
        if (comma == NULL) {
            break;
        }
        errno = 0;
        parsed_int = strtol(text, &endptr, 10);
        valid = endptr == comma && errno != ERANGE &&
            parsed_int >= (long)INT_MIN && parsed_int <= (long)INT_MAX;
        if (valid) {
            value->int_pair.first = (int)parsed_int;
            errno = 0;
            parsed_int = strtol(comma + 1, &endptr, 10);
            valid = endptr != comma + 1 && endptr != NULL &&
                endptr[0] == '\0' && errno != ERANGE &&
                parsed_int >= (long)INT_MIN && parsed_int <= (long)INT_MAX;
        }
        if (valid) {
            value->int_pair.second = (int)parsed_int;
        }
        break;
    case SIXEL_SUBOPTION_VALUE_CHOICE:
    default:
        return SIXEL_BAD_ARGUMENT;
    }

    if (valid) {
        return SIXEL_OK;
    }
    if (report_error && key_def->invalid_value_message != NULL) {
        if (key_def->invalid_value_suffix != NULL) {
            (void)sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s%s%s",
                                        key_def->invalid_value_message,
                                        text,
                                        key_def->invalid_value_suffix);
        } else {
            (void)sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s",
                                        key_def->invalid_value_message);
        }
        sixel_helper_set_additional_message(message);
    }
    return SIXEL_BAD_ARGUMENT;
}

int
sixel_option_parse_boolean_text(char const *text, int *value)
{
    if (text == NULL || value == NULL) {
        return 0;
    }
    if (text[0] == '0' && text[1] == '\0') {
        *value = 0;
        return 1;
    }
    if (text[0] == '1' && text[1] == '\0') {
        *value = 1;
        return 1;
    }

    return 0;
}

int
sixel_option_resolve_boolean_environment(char const *name, int fallback)
{
    char const *text;
    int value;

    text = NULL;
    value = fallback ? 1 : 0;
    if (name == NULL || name[0] == '\0') {
        return value;
    }
    text = sixel_compat_getenv(name);
    if (text == NULL || text[0] == '\0') {
        return value;
    }
    if (!sixel_option_parse_boolean_text(text, &value)) {
        return fallback ? 1 : 0;
    }

    return value;
}

int
sixel_option_resolve_registered_boolean_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int fallback)
{
    sixel_suboption_key_t const *key_def;
    sixel_suboption_value_t parsed;

    key_def = NULL;
    memset(&parsed, 0, sizeof(parsed));
    if (binding_identifier == NULL ||
        !sixel_option_registry_validate()) {
        return fallback ? 1 : 0;
    }
    key_def = sixel_option_registry_suboption_by_binding(
        option_id,
        base_name,
        binding_identifier);
    if (key_def == NULL ||
        key_def->value_kind != SIXEL_SUBOPTION_VALUE_BOOLEAN) {
        return fallback ? 1 : 0;
    }
    if (!sixel_option_resolve_suboption_environment(key_def, &parsed)) {
        return fallback ? 1 : 0;
    }

    return parsed.int_value != 0;
}

static int
sixel_option_resolve_registered_binding_value(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    sixel_suboption_value_kind_t expected_kind,
    sixel_suboption_value_t *value)
{
    sixel_suboption_key_t const *key_def;

    key_def = NULL;
    if (binding_identifier == NULL || value == NULL ||
        !sixel_option_registry_validate()) {
        return 0;
    }
    key_def = sixel_option_registry_suboption_by_binding(
        option_id,
        base_name,
        binding_identifier);
    if (key_def == NULL || key_def->value_kind != expected_kind) {
        return 0;
    }

    return sixel_option_resolve_suboption_environment(key_def, value);
}

int
sixel_option_resolve_registered_int_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int *value)
{
    sixel_suboption_key_t const *key_def;
    sixel_suboption_value_t parsed;

    key_def = NULL;
    memset(&parsed, 0, sizeof(parsed));
    if (binding_identifier == NULL || value == NULL ||
        !sixel_option_registry_validate()) {
        return 0;
    }
    key_def = sixel_option_registry_suboption_by_binding(
        option_id,
        base_name,
        binding_identifier);
    if (key_def == NULL ||
        (key_def->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE &&
         key_def->value_kind != SIXEL_SUBOPTION_VALUE_BOOLEAN &&
         key_def->value_kind != SIXEL_SUBOPTION_VALUE_INT &&
         key_def->value_kind != SIXEL_SUBOPTION_VALUE_SCALED_U8)) {
        return 0;
    }
    if (!sixel_option_resolve_suboption_environment(key_def, &parsed)) {
        return 0;
    }
    *value = parsed.int_value;
    return 1;
}

int
sixel_option_resolve_registered_uint_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    unsigned int *value)
{
    sixel_suboption_value_t parsed;

    memset(&parsed, 0, sizeof(parsed));
    if (value == NULL ||
        !sixel_option_resolve_registered_binding_value(
            option_id,
            base_name,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_UINT,
            &parsed)) {
        return 0;
    }
    *value = parsed.uint_value;
    return 1;
}

int
sixel_option_resolve_registered_choice_list_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    unsigned int *value)
{
    sixel_suboption_value_t parsed;

    memset(&parsed, 0, sizeof(parsed));
    if (value == NULL ||
        !sixel_option_resolve_registered_binding_value(
            option_id,
            base_name,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_CHOICE_LIST,
            &parsed)) {
        return 0;
    }
    *value = parsed.uint_value;
    return 1;
}

sixel_option_environment_result_t
sixel_option_resolve_registered_size_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    size_t *value)
{
    sixel_suboption_key_t const *key_def;
    sixel_suboption_value_t parsed;
    sixel_option_environment_result_t result;

    key_def = NULL;
    memset(&parsed, 0, sizeof(parsed));
    result = SIXEL_OPTION_ENVIRONMENT_INVALID;
    if (binding_identifier == NULL || value == NULL ||
        !sixel_option_registry_validate()) {
        return result;
    }
    key_def = sixel_option_registry_suboption_by_binding(
        option_id,
        base_name,
        binding_identifier);
    if (key_def == NULL ||
        key_def->value_kind != SIXEL_SUBOPTION_VALUE_SIZE) {
        return result;
    }
    result = sixel_option_resolve_suboption_environment_result(
        key_def,
        &parsed);
    if (result == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        *value = parsed.size_value;
    }
    return result;
}

int
sixel_option_resolve_registered_float_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    float *value)
{
    sixel_suboption_value_t parsed;

    memset(&parsed, 0, sizeof(parsed));
    if (value == NULL ||
        !sixel_option_resolve_registered_binding_value(
            option_id,
            base_name,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_FLOAT,
            &parsed)) {
        return 0;
    }
    *value = parsed.float_value;
    return 1;
}

int
sixel_option_resolve_registered_double_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    double *value)
{
    sixel_suboption_value_t parsed;

    memset(&parsed, 0, sizeof(parsed));
    if (value == NULL ||
        !sixel_option_resolve_registered_binding_value(
            option_id,
            base_name,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_DOUBLE,
            &parsed)) {
        return 0;
    }
    *value = parsed.double_value;
    return 1;
}

int
sixel_option_resolve_registered_int_pair_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier,
    int *first,
    int *second)
{
    sixel_suboption_value_t parsed;

    memset(&parsed, 0, sizeof(parsed));
    if (first == NULL || second == NULL ||
        !sixel_option_resolve_registered_binding_value(
            option_id,
            base_name,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_INT_PAIR,
            &parsed)) {
        return 0;
    }
    *first = parsed.int_pair.first;
    *second = parsed.int_pair.second;
    return 1;
}

static void
sixel_option_trace_environment_value(
    sixel_suboption_key_t const *key_def,
    char const *environment_name,
    char const *text,
    sixel_suboption_value_t const *value,
    int matched)
{
    char *endptr;
    unsigned long long parsed;

    endptr = NULL;
    parsed = 0ULL;
    if (key_def == NULL || environment_name == NULL || text == NULL ||
        value == NULL ||
        key_def->environment_trace_topic == NULL) {
        return;
    }
    if (!matched) {
        sixel_trace_topic_message(
            key_def->environment_trace_topic,
            "ignore invalid %s=%s",
            environment_name,
            text);
        return;
    }
    if (key_def->value_kind != SIXEL_SUBOPTION_VALUE_UINT ||
        !key_def->has_maximum ||
        (key_def->environment_range_policy &
         SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) == 0) {
        return;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (errno == 0 && endptr != text && endptr != NULL &&
        endptr[0] == '\0' &&
        (double)parsed > key_def->maximum) {
        sixel_trace_topic_message(
            key_def->environment_trace_topic,
            "clamp %s=%s to %u",
            environment_name,
            text,
            value->uint_value);
    }
}

static sixel_option_environment_result_t
sixel_option_parse_environment_value(
    sixel_suboption_key_t const *key_def,
    char const *environment_name,
    char const *text,
    sixel_suboption_value_t *value)
{
    size_t index;
    char *endptr;
    long parsed_int;
    int matched;
    int range_error;

    index = 0u;
    endptr = NULL;
    parsed_int = 0L;
    matched = 0;
    range_error = 0;
    if (key_def == NULL || environment_name == NULL || text == NULL ||
        text[0] == '\0' || value == NULL) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    if (key_def->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE) {
        matched = SIXEL_SUCCEEDED(sixel_option_parse_typed_suboption_value(
            key_def,
            text,
            value,
            key_def->environment_range_policy,
            1,
            0,
            &range_error));
        sixel_option_trace_environment_value(
            key_def,
            environment_name,
            text,
            value,
            matched);
        if (matched) {
            return SIXEL_OPTION_ENVIRONMENT_MATCH;
        }
        return range_error
            ? SIXEL_OPTION_ENVIRONMENT_RANGE
            : SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    if ((key_def->environment_range_policy &
         SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG) != 0) {
        errno = 0;
        parsed_int = strtol(text, &endptr, 10);
        if (endptr == text || endptr == NULL || endptr[0] != '\0' ||
            errno == ERANGE || parsed_int < (long)INT_MIN ||
            parsed_int > (long)INT_MAX) {
            return SIXEL_OPTION_ENVIRONMENT_INVALID;
        }
        while (index < key_def->choice_count) {
            if (key_def->choices[index].value == (int)parsed_int) {
                value->int_value = (int)parsed_int;
                return SIXEL_OPTION_ENVIRONMENT_MATCH;
            }
            ++index;
        }
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    while (index < key_def->environment_choice_count) {
        if (key_def->environment_choices[index].name != NULL &&
            sixel_compat_strcasecmp(
                key_def->environment_choices[index].name,
                text) == 0) {
            value->int_value =
                key_def->environment_choices[index].value;
            return SIXEL_OPTION_ENVIRONMENT_MATCH;
        }
        ++index;
    }

    index = 0u;
    while (index < key_def->choice_count) {
        if (key_def->choices[index].name != NULL &&
            sixel_compat_strcasecmp(key_def->choices[index].name, text) == 0) {
            value->int_value = key_def->choices[index].value;
            return SIXEL_OPTION_ENVIRONMENT_MATCH;
        }
        ++index;
    }

    return SIXEL_OPTION_ENVIRONMENT_INVALID;
}

/*
 * Resolve primary, shared-fallback, and legacy environment spellings in one
 * place.  A present but invalid canonical value never falls through to a
 * legacy typo; shared fallback variables still provide the documented
 * process default.
 */
sixel_option_environment_result_t
sixel_option_resolve_suboption_environment_result(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t *value)
{
    char const *text;
    int primary_present;
    sixel_option_environment_result_t result;
    sixel_option_environment_result_t error_result;

    text = NULL;
    primary_present = 0;
    result = SIXEL_OPTION_ENVIRONMENT_UNSET;
    error_result = SIXEL_OPTION_ENVIRONMENT_UNSET;
    if (key_def == NULL || value == NULL || key_def->env_name == NULL ||
        !sixel_option_registry_validate()) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    text = sixel_compat_getenv(key_def->env_name);
    primary_present = text != NULL && text[0] != '\0';
    if (primary_present) {
        result = sixel_option_parse_environment_value(
            key_def,
            key_def->env_name,
            text,
            value);
        if (result == SIXEL_OPTION_ENVIRONMENT_MATCH) {
            return result;
        }
        error_result = result;
    }

    if (key_def->env_fallback_name != NULL) {
        text = sixel_compat_getenv(key_def->env_fallback_name);
        if (text != NULL && text[0] != '\0') {
            result = sixel_option_parse_environment_value(
                key_def,
                key_def->env_fallback_name,
                text,
                value);
            if (result == SIXEL_OPTION_ENVIRONMENT_MATCH) {
                return result;
            }
            if (error_result == SIXEL_OPTION_ENVIRONMENT_UNSET) {
                error_result = result;
            }
        }
    }

    if (primary_present || key_def->env_legacy_name == NULL) {
        return error_result;
    }
    text = sixel_compat_getenv(key_def->env_legacy_name);
    if (text == NULL || text[0] == '\0') {
        return error_result;
    }
    result = sixel_option_parse_environment_value(
        key_def,
        key_def->env_legacy_name,
        text,
        value);
    return result;
}

int
sixel_option_resolve_suboption_environment(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t *value)
{
    return sixel_option_resolve_suboption_environment_result(
        key_def,
        value) == SIXEL_OPTION_ENVIRONMENT_MATCH;
}

static int
sixel_option_store_suboption_value(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value,
    unsigned char *target,
    size_t offset)
{
    int int_value;

    int_value = 0;
    if (key_def == NULL || value == NULL || target == NULL ||
        !sixel_option_registry_validate() ||
        offset == SIXEL_SUBOPTION_OFFSET_NONE) {
        return 0;
    }

    switch (key_def->binding.storage_kind) {
    case SIXEL_SUBOPTION_STORAGE_INT:
        if (key_def->value_kind == SIXEL_SUBOPTION_VALUE_UINT) {
            int_value = (int)value->uint_value;
        } else {
            int_value = value->int_value;
        }
        memcpy(target + offset, &int_value, sizeof(int_value));
        break;
    case SIXEL_SUBOPTION_STORAGE_UINT:
        memcpy(target + offset,
               &value->uint_value,
               sizeof(value->uint_value));
        break;
    case SIXEL_SUBOPTION_STORAGE_SIZE:
        memcpy(target + offset,
               &value->size_value,
               sizeof(value->size_value));
        break;
    case SIXEL_SUBOPTION_STORAGE_FLOAT:
        memcpy(target + offset,
               &value->float_value,
               sizeof(value->float_value));
        break;
    case SIXEL_SUBOPTION_STORAGE_DOUBLE:
        memcpy(target + offset,
               &value->double_value,
               sizeof(value->double_value));
        break;
    case SIXEL_SUBOPTION_STORAGE_INT_PAIR:
        if (key_def->binding.second_value_offset ==
            SIXEL_SUBOPTION_OFFSET_NONE) {
            return 0;
        }
        memcpy(target + offset,
               &value->int_pair.first,
               sizeof(value->int_pair.first));
        memcpy(target + key_def->binding.second_value_offset,
               &value->int_pair.second,
               sizeof(value->int_pair.second));
        break;
    default:
        return 0;
    }

    return 1;
}

/*
 * Compare the typed assignment with the value stored through the registry
 * binding.  The trace emitted from this check is intentionally generic: it
 * lets every suboption regression verify the common application boundary
 * without adding option-specific diagnostic code to individual algorithms.
 */
static int
sixel_option_suboption_value_is_stored(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value,
    unsigned char const *target,
    size_t offset)
{
    int int_value;
    int stored_int;
    unsigned int stored_uint;
    size_t stored_size;
    float stored_float;
    double stored_double;

    int_value = 0;
    stored_int = 0;
    stored_uint = 0u;
    stored_size = 0u;
    stored_float = 0.0f;
    stored_double = 0.0;
    if (key_def == NULL || value == NULL || target == NULL ||
        offset == SIXEL_SUBOPTION_OFFSET_NONE) {
        return 0;
    }

    switch (key_def->binding.storage_kind) {
    case SIXEL_SUBOPTION_STORAGE_INT:
        if (key_def->value_kind == SIXEL_SUBOPTION_VALUE_UINT) {
            int_value = (int)value->uint_value;
        } else {
            int_value = value->int_value;
        }
        memcpy(&stored_int, target + offset, sizeof(stored_int));
        return stored_int == int_value;
    case SIXEL_SUBOPTION_STORAGE_UINT:
        memcpy(&stored_uint, target + offset, sizeof(stored_uint));
        return stored_uint == value->uint_value;
    case SIXEL_SUBOPTION_STORAGE_SIZE:
        memcpy(&stored_size, target + offset, sizeof(stored_size));
        return stored_size == value->size_value;
    case SIXEL_SUBOPTION_STORAGE_FLOAT:
        memcpy(&stored_float, target + offset, sizeof(stored_float));
        return stored_float == value->float_value;
    case SIXEL_SUBOPTION_STORAGE_DOUBLE:
        memcpy(&stored_double, target + offset, sizeof(stored_double));
        return stored_double == value->double_value;
    case SIXEL_SUBOPTION_STORAGE_INT_PAIR:
        if (key_def->binding.second_value_offset ==
            SIXEL_SUBOPTION_OFFSET_NONE) {
            return 0;
        }
        memcpy(&stored_int, target + offset, sizeof(stored_int));
        if (stored_int != value->int_pair.first) {
            return 0;
        }
        memcpy(&stored_int,
               target + key_def->binding.second_value_offset,
               sizeof(stored_int));
        return stored_int == value->int_pair.second;
    default:
        break;
    }

    return 0;
}

static void
sixel_option_emit_suboption_value(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value)
{
    size_t count;
    size_t index;
    size_t choice_index;
    int choice_value;

    count = 0u;
    index = 0u;
    choice_index = 0u;
    choice_value = 0;
    if (key_def == NULL || value == NULL) {
        return;
    }

    switch (key_def->value_kind) {
    case SIXEL_SUBOPTION_VALUE_CHOICE:
    case SIXEL_SUBOPTION_VALUE_BOOLEAN:
    case SIXEL_SUBOPTION_VALUE_INT:
    case SIXEL_SUBOPTION_VALUE_SCALED_U8:
        fprintf(stderr, "%d", value->int_value);
        break;
    case SIXEL_SUBOPTION_VALUE_UINT:
        fprintf(stderr, "%u", value->uint_value);
        break;
    case SIXEL_SUBOPTION_VALUE_CHOICE_LIST:
        count = sixel_option_choice_list_count(value->uint_value);
        while (index < count) {
            if (index > 0u) {
                fputc('+', stderr);
            }
            if (!sixel_option_choice_list_value_at(value->uint_value,
                                                   index,
                                                   &choice_value)) {
                fputs("invalid", stderr);
                break;
            }
            choice_index = 0u;
            while (choice_index < key_def->choice_count &&
                   key_def->choices[choice_index].value != choice_value) {
                ++choice_index;
            }
            if (choice_index < key_def->choice_count) {
                fputs(key_def->choices[choice_index].name, stderr);
            } else {
                fprintf(stderr, "%d", choice_value);
            }
            ++index;
        }
        if (sixel_option_choice_list_is_exclusive(value->uint_value)) {
            fputc('!', stderr);
        }
        break;
    case SIXEL_SUBOPTION_VALUE_SIZE:
        fprintf(stderr, "%zu", value->size_value);
        break;
    case SIXEL_SUBOPTION_VALUE_FLOAT:
        fprintf(stderr, "%.9g", (double)value->float_value);
        break;
    case SIXEL_SUBOPTION_VALUE_DOUBLE:
        fprintf(stderr, "%.17g", value->double_value);
        break;
    case SIXEL_SUBOPTION_VALUE_INT_PAIR:
        fprintf(stderr,
                "%d,%d",
                value->int_pair.first,
                value->int_pair.second);
        break;
    default:
        break;
    }
}

static void
sixel_option_emit_suboption_contract(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value,
    unsigned char const *target)
{
    int stored;
    int override_value;

    stored = 0;
    override_value = 0;
    if (key_def == NULL || value == NULL || target == NULL ||
        !sixel_trace_topic_is_enabled("suboption_contract")) {
        return;
    }

    stored = sixel_option_suboption_value_is_stored(
        key_def,
        value,
        target,
        key_def->binding.value_offset);
    if (stored != 0 &&
        key_def->binding.override_offset != SIXEL_SUBOPTION_OFFSET_NONE) {
        memcpy(&override_value,
               target + key_def->binding.override_offset,
               sizeof(override_value));
        stored = override_value == 1;
    }
    if (stored != 0 &&
        key_def->binding.mirror_offset != SIXEL_SUBOPTION_OFFSET_NONE) {
        stored = sixel_option_suboption_value_is_stored(
            key_def,
            value,
            target,
            key_def->binding.mirror_offset);
    }

    fprintf(stderr,
            "LSXSUB1|schema=%d|base=%s|key=%s|stored=%d|binding=%s|value=",
            (int)key_def->option_id,
            key_def->base_def != NULL ? key_def->base_def->name : "*",
            key_def->name,
            stored,
            key_def->binding.identifier != NULL
                ? key_def->binding.identifier
                : "");
    sixel_option_emit_suboption_value(key_def, value);
    fprintf(stderr, "\n");
}

int
sixel_option_apply_suboption_value(
    sixel_suboption_key_t const *key_def,
    sixel_suboption_value_t const *value,
    void *target,
    sixel_suboption_target_class_t target_class)
{
    unsigned char *bytes;
    int override_value;

    bytes = (unsigned char *)target;
    override_value = 1;
    if (key_def == NULL || value == NULL || target == NULL ||
        key_def->binding.target_class != target_class ||
        target_class == SIXEL_SUBOPTION_TARGET_NONE) {
        return 0;
    }
    if (!sixel_option_store_suboption_value(
            key_def,
            value,
            bytes,
            key_def->binding.value_offset)) {
        return 0;
    }
    if (key_def->binding.override_offset != SIXEL_SUBOPTION_OFFSET_NONE) {
        memcpy(bytes + key_def->binding.override_offset,
               &override_value,
               sizeof(override_value));
    }
    if (key_def->binding.mirror_offset != SIXEL_SUBOPTION_OFFSET_NONE &&
        !sixel_option_store_suboption_value(
            key_def,
            value,
            bytes,
            key_def->binding.mirror_offset)) {
        return 0;
    }

    sixel_option_emit_suboption_contract(key_def, value, bytes);

    return 1;
}

int
sixel_option_apply_suboption_assignments(
    sixel_option_argument_resolution_t const *resolution,
    void *target,
    sixel_suboption_target_class_t target_class)
{
    size_t index;

    index = 0u;
    if (resolution == NULL || target == NULL) {
        return 0;
    }
    while (index < resolution->assignment_count) {
        if (!sixel_option_apply_suboption_value(
                resolution->assignments[index].key_def,
                &resolution->assignments[index].value,
                target,
                target_class)) {
            return 0;
        }
        ++index;
    }

    return 1;
}

void
sixel_option_apply_suboption_environment(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    void *target,
    sixel_suboption_target_class_t target_class)
{
    size_t index;
    size_t count;
    sixel_suboption_key_t const *key_def;
    sixel_suboption_value_t value;

    index = 0u;
    count = 0u;
    key_def = NULL;
    memset(&value, 0, sizeof(value));
    if (schema == NULL || target == NULL ||
        !sixel_option_registry_validate()) {
        return;
    }

    count = sixel_option_registry_suboption_count_for_scope(
        schema,
        base_def,
        consumer_scope);
    while (index < count) {
        key_def = sixel_option_registry_suboption_at_for_scope(
            schema,
            base_def,
            consumer_scope,
            index);
        if (sixel_option_resolve_suboption_environment(key_def, &value)) {
            (void)sixel_option_apply_suboption_value(
                key_def,
                &value,
                target,
                target_class);
        }
        ++index;
    }
}

void
sixel_option_reset_suboption_overrides(
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    void *target,
    sixel_suboption_target_class_t target_class)
{
    size_t base_index;
    size_t key_index;
    size_t key_count;
    sixel_suboption_key_t const *key_def;
    unsigned char *bytes;
    int override_value;

    base_index = 0u;
    key_index = 0u;
    key_count = 0u;
    key_def = NULL;
    bytes = (unsigned char *)target;
    override_value = 0;
    if (schema == NULL || target == NULL ||
        !sixel_option_registry_validate() ||
        target_class == SIXEL_SUBOPTION_TARGET_NONE) {
        return;
    }

    while (base_index < schema->value_count) {
        key_count = sixel_option_registry_suboption_count_for_scope(
            schema,
            schema->values + base_index,
            consumer_scope);
        key_index = 0u;
        while (key_index < key_count) {
            key_def = sixel_option_registry_suboption_at_for_scope(
                schema,
                schema->values + base_index,
                consumer_scope,
                key_index);
            if (key_def != NULL &&
                key_def->binding.target_class == target_class &&
                key_def->binding.override_offset !=
                    SIXEL_SUBOPTION_OFFSET_NONE) {
                memcpy(bytes + key_def->binding.override_offset,
                       &override_value,
                       sizeof(override_value));
            }
            ++key_index;
        }
        ++base_index;
    }
}

static int
sixel_option_append_suboption_assignment(
    sixel_option_argument_resolution_t *resolution,
    sixel_suboption_key_t const *key_def,
    char const *resolved_value_text,
    sixel_suboption_value_t const *value)
{
    size_t index;
    char *value_copy;
    sixel_suboption_assignment_t *grown;

    index = 0u;
    value_copy = NULL;
    grown = NULL;

    if (resolution == NULL || key_def == NULL ||
        resolved_value_text == NULL || value == NULL) {
        return 0;
    }

    while (index < resolution->assignment_count) {
        if (resolution->assignments[index].key_def == key_def) {
            value_copy = (char *)malloc(strlen(resolved_value_text) + 1u);
            if (value_copy == NULL) {
                return 0;
            }
            (void)sixel_compat_strcpy(value_copy,
                                      strlen(resolved_value_text) + 1u,
                                      resolved_value_text);
            free(resolution->assignments[index].resolved_value_text);
            resolution->assignments[index].resolved_value_text = value_copy;
            resolution->assignments[index].value = *value;
            return 1;
        }
        ++index;
    }

    grown = (sixel_suboption_assignment_t *)realloc(
        resolution->assignments,
        (resolution->assignment_count + 1u)
        * sizeof(sixel_suboption_assignment_t));
    if (grown == NULL) {
        return 0;
    }
    resolution->assignments = grown;

    value_copy = (char *)malloc(strlen(resolved_value_text) + 1u);
    if (value_copy == NULL) {
        return 0;
    }
    (void)sixel_compat_strcpy(value_copy,
                              strlen(resolved_value_text) + 1u,
                              resolved_value_text);

    resolution->assignments[resolution->assignment_count].key_def = key_def;
    resolution->assignments[resolution->assignment_count].resolved_key_name =
        key_def->name;
    resolution->assignments[resolution->assignment_count].resolved_value_text =
        value_copy;
    resolution->assignments[resolution->assignment_count].value = *value;
    resolution->assignment_count += 1u;

    return 1;
}

static sixel_option_choice_result_t
sixel_option_match_schema_value(
    char const *token,
    sixel_option_value_schema_t const *values,
    size_t value_count,
    int *matched_index,
    char *diagnostic,
    size_t diagnostic_size)
{
    size_t index;
    sixel_option_choice_t *choices;
    sixel_option_choice_result_t result;
    int matched_value;

    index = 0u;
    choices = NULL;
    result = SIXEL_OPTION_CHOICE_NONE;
    matched_value = 0;

    if (matched_index != NULL) {
        *matched_index = 0;
    }
    if (values == NULL || value_count == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    choices = (sixel_option_choice_t *)malloc(
        value_count * sizeof(sixel_option_choice_t));
    if (choices == NULL) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    while (index < value_count) {
        choices[index].name = values[index].name;
        choices[index].value = (int)index;
        ++index;
    }

    result = sixel_option_match_choice(token,
                                       choices,
                                       value_count,
                                       &matched_value,
                                       diagnostic,
                                       diagnostic_size);
    free(choices);

    if (result == SIXEL_OPTION_CHOICE_MATCH && matched_index != NULL) {
        *matched_index = matched_value;
    }

    return result;
}

static sixel_option_choice_result_t
sixel_option_match_suboption_key(
    char const *token,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    int *matched_index,
    char *diagnostic,
    size_t diagnostic_size)
{
    size_t index;
    size_t key_count;
    size_t choice_count;
    sixel_option_choice_t *choices;
    sixel_suboption_key_t const *key;
    int candidate_index;

    index = 0u;
    key_count = 0u;
    choice_count = 0u;
    choices = NULL;
    key = NULL;
    candidate_index = (-1);

    if (matched_index != NULL) {
        *matched_index = 0;
    }
    key_count = sixel_option_registry_suboption_count_for_scope(
        schema,
        base_def,
        consumer_scope);
    if (key_count == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    while (index < key_count) {
        key = sixel_option_registry_suboption_at_for_scope(
            schema,
            base_def,
            consumer_scope,
            index);
        if (key == NULL) {
            ++index;
            continue;
        }
        if (key->name != NULL && strcmp(key->name, token) == 0) {
            if (candidate_index != (-1)) {
                return SIXEL_OPTION_CHOICE_AMBIGUOUS;
            }
            candidate_index = (int)index;
        }
        ++index;
    }

    if (candidate_index != (-1)) {
        if (matched_index != NULL) {
            *matched_index = candidate_index;
        }
        return SIXEL_OPTION_CHOICE_MATCH;
    }

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (key_count > (SIZE_MAX / sizeof(sixel_option_choice_t))) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    choices = (sixel_option_choice_t *)malloc(
        key_count * sizeof(sixel_option_choice_t));
    if (choices == NULL) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    choice_count = 0u;
    index = 0u;
    while (index < key_count) {
        key = sixel_option_registry_suboption_at_for_scope(
            schema,
            base_def,
            consumer_scope,
            index);
        if (key == NULL) {
            ++index;
            continue;
        }
        if (key->name != NULL && key->name[0] != '\0') {
            choices[choice_count].name = key->name;
            choices[choice_count].value = (int)index;
            choice_count += 1u;
        }
        ++index;
    }

    (void)sixel_option_collect_choice_suggestions(token,
                                                  choices,
                                                  choice_count,
                                                  diagnostic,
                                                  diagnostic_size);
    free(choices);

    return SIXEL_OPTION_CHOICE_NONE;
}

static sixel_option_choice_result_t
sixel_option_match_suboption_short_key(
    char const *token,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
    int *matched_index,
    char const **value_text_out)
{
    size_t index;
    size_t key_count;
    int candidate_index;
    char const *candidate_value_text;
    sixel_suboption_key_t const *key;

    index = 0u;
    key_count = 0u;
    candidate_index = (-1);
    candidate_value_text = NULL;
    key = NULL;

    if (matched_index != NULL) {
        *matched_index = 0;
    }
    if (value_text_out != NULL) {
        *value_text_out = NULL;
    }
    key_count = sixel_option_registry_suboption_count_for_scope(
        schema,
        base_def,
        consumer_scope);
    if (token == NULL || key_count == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    /* A nonempty suffix is required because it is the value text. */
    while (index < key_count) {
        key = sixel_option_registry_suboption_at_for_scope(
            schema,
            base_def,
            consumer_scope,
            index);
        if (key == NULL) {
            ++index;
            continue;
        }
        if (token[0] == key->short_name && token[1] != '\0') {
            if (candidate_index != (-1)) {
                return SIXEL_OPTION_CHOICE_AMBIGUOUS;
            }
            candidate_index = (int)index;
            candidate_value_text = token + 1;
        }
        ++index;
    }

    if (candidate_index == (-1)) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    if (matched_index != NULL) {
        *matched_index = candidate_index;
    }
    if (value_text_out != NULL) {
        *value_text_out = candidate_value_text;
    }

    return SIXEL_OPTION_CHOICE_MATCH;
}

static int
sixel_option_choice_suggestion_compare(
    void const *lhs,
    void const *rhs)
{
    sixel_option_choice_suggestion_t const *left;
    sixel_option_choice_suggestion_t const *right;
    size_t left_length;
    size_t right_length;

    left = (sixel_option_choice_suggestion_t const *)lhs;
    right = (sixel_option_choice_suggestion_t const *)rhs;
    left_length = strlen(left->name);
    right_length = strlen(right->name);

    if (left->score < right->score) {
        return 1;
    }
    if (left->score > right->score) {
        return -1;
    }
    if (left->distance > right->distance) {
        return 1;
    }
    if (left->distance < right->distance) {
        return -1;
    }
    if (left_length > right_length) {
        return 1;
    }
    if (left_length < right_length) {
        return -1;
    }

    return strcmp(left->name, right->name);
}

static size_t
sixel_option_collect_choice_suggestions(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    char *diagnostic,
    size_t diagnostic_size)
{
    sixel_option_choice_suggestion_t *candidates;
    size_t candidate_count;
    size_t candidate_capacity;
    size_t index;
    size_t diag_length;
    size_t copy_length;
    size_t distance;
    size_t name_length;
    size_t prefix_length;
    size_t existing_index;
    double score;
    char *prefix;
    char const *name;
    int has_existing;
    int suggestions_enabled;

    candidates = NULL;
    candidate_count = 0u;
    candidate_capacity = 0u;
    index = 0u;
    diag_length = 0u;
    copy_length = 0u;
    distance = 0u;
    name_length = 0u;
    prefix_length = 0u;
    existing_index = 0u;
    score = 0.0;
    prefix = NULL;
    name = NULL;
    has_existing = 0;
    suggestions_enabled = 0;

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (value == NULL || choices == NULL || choice_count == 0u) {
        return 0u;
    }

    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_FUZZY_SUGGESTIONS);
    if (!suggestions_enabled) {
        return 0u;
    }

    candidates = (sixel_option_choice_suggestion_t *)malloc(
        choice_count * sizeof(sixel_option_choice_suggestion_t));
    if (candidates == NULL) {
        return 0u;
    }
    candidate_capacity = choice_count;

    for (index = 0u; index < choice_count; ++index) {
        name = choices[index].name;
        name_length = strlen(name);
        for (prefix_length = 1u;
             prefix_length <= name_length;
             ++prefix_length) {
            if (!sixel_option_choice_prefix_accepts(
                    choices,
                    choice_count,
                    name,
                    prefix_length)) {
                continue;
            }
            prefix = (char *)malloc(prefix_length + 1u);
            if (prefix == NULL) {
                free(candidates);
                return 0u;
            }
            memcpy(prefix, name, prefix_length);
            prefix[prefix_length] = '\0';
            score = sixel_option_normalized_levenshtein(
                value,
                prefix,
                &distance);
            free(prefix);
            prefix = NULL;
            if (distance == 0u) {
                continue;
            }
            if (distance > 2u) {
                continue;
            }
            if (score < SIXEL_OPTION_CHOICE_SUGGESTION_THRESHOLD) {
                continue;
            }
            if (prefix_length <= SIXEL_OPTION_CHOICE_SHORT_NAME_LENGTH &&
                distance > 1u) {
                continue;
            }
            has_existing = 0;
            for (existing_index = 0u;
                 existing_index < candidate_count;
                 ++existing_index) {
                if (strcmp(candidates[existing_index].name, name) == 0) {
                    has_existing = 1;
                    break;
                }
            }
            if (has_existing) {
                if (score > candidates[existing_index].score ||
                    (score == candidates[existing_index].score &&
                     distance < candidates[existing_index].distance)) {
                    candidates[existing_index].score = score;
                    candidates[existing_index].distance = distance;
                }
                continue;
            }
            if (candidate_count >= candidate_capacity) {
                continue;
            }
            candidates[candidate_count].name = name;
            candidates[candidate_count].score = score;
            candidates[candidate_count].distance = distance;
            ++candidate_count;
        }
    }

    if (candidate_count == 0u) {
        free(candidates);
        return 0u;
    }

    if (candidate_count > 1u) {
        qsort(candidates,
              candidate_count,
              sizeof(sixel_option_choice_suggestion_t),
              sixel_option_choice_suggestion_compare);
    }

    if (candidate_count > SIXEL_OPTION_CHOICE_SUGGESTION_LIMIT) {
        candidate_count = SIXEL_OPTION_CHOICE_SUGGESTION_LIMIT;
    }

    if (diagnostic != NULL && diagnostic_size > 0u) {
        for (index = 0u; index < candidate_count; ++index) {
            if (diag_length > 0u && diag_length + 2u < diagnostic_size) {
                diagnostic[diag_length] = ',';
                diagnostic[diag_length + 1u] = ' ';
                diag_length += 2u;
                diagnostic[diag_length] = '\0';
            }
            copy_length = strlen(candidates[index].name);
            if (copy_length > diagnostic_size - diag_length - 1u) {
                copy_length = diagnostic_size - diag_length - 1u;
            }
            memcpy(diagnostic + diag_length,
                   candidates[index].name,
                   copy_length);
            diag_length += copy_length;
            diagnostic[diag_length] = '\0';
            if (diag_length >= diagnostic_size - 1u) {
                break;
            }
        }
    }

    free(candidates);
    return candidate_count;
}

static int
sixel_option_choice_prefix_accepts(
    sixel_option_choice_t const *choices,
    size_t choice_count,
    char const *name,
    size_t prefix_length)
{
    size_t index;
    int base_value;
    int base_set;

    if (choices == NULL || name == NULL || prefix_length == 0u) {
        return 0;
    }

    base_value = 0;
    base_set = 0;
    for (index = 0u; index < choice_count; ++index) {
        if (strncmp(choices[index].name, name, prefix_length) != 0) {
            continue;
        }
        if (!base_set) {
            base_value = choices[index].value;
            base_set = 1;
            continue;
        }
        if (choices[index].value != base_value) {
            return 0;
        }
    }

    return base_set;
}

#define SIXEL_OPTION_SUGGESTION_LIMIT 5u
#define SIXEL_OPTION_SUGGESTION_NAME_WEIGHT 0.55
#define SIXEL_OPTION_SUGGESTION_EXTENSION_WEIGHT 0.25
#define SIXEL_OPTION_SUGGESTION_RECENCY_WEIGHT 0.20

/*
 * The suggestion engine mirrors the fuzzy finder heuristics used by the
 * converters before the refactor.  Filename similarity dominates the ranking
 * while matching extensions and recent modification times act as tiebreakers.
 */

typedef struct sixel_option_path_candidate {
    char *path;
    char const *name;
    time_t mtime;
    double name_score;
    double extension_score;
    double recency_score;
    double total_score;
} sixel_option_path_candidate_t;

static SIXEL_OPTION_UNUSED int
sixel_option_case_insensitive_equals(
    char const *lhs,
    char const *rhs)
{
    size_t index;
    unsigned char left;
    unsigned char right;

    index = 0u;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    while (lhs[index] != '\0' && rhs[index] != '\0') {
        left = (unsigned char)lhs[index];
        right = (unsigned char)rhs[index];
        if (tolower(left) != tolower(right)) {
            return 0;
        }
        ++index;
    }

    return lhs[index] == '\0' && rhs[index] == '\0';
}

static SIXEL_OPTION_UNUSED char const *
sixel_option_basename_view(char const *path)
{
    char const *forward;
#if defined(_WIN32)
    char const *backward;
#endif
    char const *start;

    forward = NULL;
#if defined(_WIN32)
    backward = NULL;
#endif
    start = path;

    if (path == NULL) {
        return NULL;
    }

    forward = strrchr(path, '/');
#if defined(_WIN32)
    backward = strrchr(path, '\\');
    if (backward != NULL && (forward == NULL || backward > forward)) {
        forward = backward;
    }
#endif

    if (forward != NULL) {
        return forward + 1;
    }

    return start;
}

static SIXEL_OPTION_UNUSED char const *
sixel_option_extension_view(char const *name)
{
    char const *dot;

    dot = NULL;

    if (name == NULL) {
        return NULL;
    }

    dot = strrchr(name, '.');
    if (dot == NULL || dot == name) {
        return NULL;
    }

    return dot + 1;
}

static double
sixel_option_normalized_levenshtein(
    char const *lhs,
    char const *rhs,
    size_t *distance_out)
{
    size_t lhs_length;
    size_t rhs_length;
    size_t *previous;
    size_t *current;
    size_t column;
    size_t row;
    size_t cost;
    size_t deletion;
    size_t insertion;
    size_t substitution;
    size_t distance_value;
    unsigned char left_char;
    unsigned char right_char;
    double normalized;

    lhs_length = 0u;
    rhs_length = 0u;
    previous = NULL;
    current = NULL;
    column = 0u;
    row = 0u;
    cost = 0u;
    deletion = 0u;
    insertion = 0u;
    substitution = 0u;
    distance_value = 0u;
    left_char = 0u;
    right_char = 0u;
    normalized = 0.0;

    if (distance_out != NULL) {
        *distance_out = 0u;
    }

    if (lhs == NULL || rhs == NULL) {
        return 0.0;
    }

    lhs_length = strlen(lhs);
    rhs_length = strlen(rhs);
    if (lhs_length == 0u && rhs_length == 0u) {
        return 1.0;
    }

    previous = (size_t *)malloc((rhs_length + 1u) * sizeof(size_t));
    if (previous == NULL) {
        return 0.0;
    }

    current = (size_t *)malloc((rhs_length + 1u) * sizeof(size_t));
    if (current == NULL) {
        free(previous);
        return 0.0;
    }

    column = 0u;
    while (column <= rhs_length) {
        previous[column] = column;
        ++column;
    }

    row = 1u;
    while (row <= lhs_length) {
        current[0] = row;
        column = 1u;
        while (column <= rhs_length) {
            left_char = (unsigned char)lhs[row - 1u];
            right_char = (unsigned char)rhs[column - 1u];
            cost = (tolower(left_char) == tolower(right_char)) ? 0u : 1u;
            deletion = previous[column] + 1u;
            insertion = current[column - 1u] + 1u;
            substitution = previous[column - 1u] + cost;
            current[column] = deletion;
            if (insertion < current[column]) {
                current[column] = insertion;
            }
            if (substitution < current[column]) {
                current[column] = substitution;
            }
            ++column;
        }
        memcpy(previous, current, (rhs_length + 1u) * sizeof(size_t));
        ++row;
    }

    distance_value = previous[rhs_length];
    free(current);
    free(previous);

    if (distance_out != NULL) {
        *distance_out = distance_value;
    }

    normalized = 1.0 - (double)distance_value /
        (double)((lhs_length > rhs_length) ? lhs_length : rhs_length);
    if (normalized < 0.0) {
        normalized = 0.0;
    }

    return normalized;
}

static char *
sixel_option_duplicate_string(char const *text)
{
    size_t length;
    char *copy;

    length = 0u;
    copy = NULL;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0u) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';

    return copy;
}

static char *
sixel_option_duplicate_directory(char const *path)
{
    char const *forward;
#if defined(_WIN32)
    char const *backward;
#endif
    char const *separator;
    size_t length;
    char *copy;

    forward = NULL;
#if defined(_WIN32)
    backward = NULL;
#endif
    separator = NULL;
    length = 0u;
    copy = NULL;

    if (path == NULL || path[0] == '\0') {
        return sixel_option_duplicate_string(".");
    }

    forward = strrchr(path, '/');
#if defined(_WIN32)
    backward = strrchr(path, '\\');
    if (backward != NULL && (forward == NULL || backward > forward)) {
        forward = backward;
    }
#endif
    separator = forward;

    if (separator == NULL) {
        return sixel_option_duplicate_string(".");
    }
    if (separator == path) {
        return sixel_option_duplicate_string("/");
    }

    length = (size_t)(separator - path);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }
    if (length > 0u) {
        memcpy(copy, path, length);
    }
    copy[length] = '\0';

    return copy;
}

#if defined(_WIN32) && HAVE_WINDOWS_H
/*
 * Convert POSIX-like drive paths to Win32-style drive paths.
 *
 * Supported inputs:
 *   - /cygdrive/x/... -> X:\...
 *   - /x/...          -> X:\... (MSYS style)
 *
 * Other inputs are copied as-is so existing Windows-native paths continue to
 * work without behavior changes.
 */
static char *
sixel_option_duplicate_win32_path(char const *path)
{
    char *converted;
    size_t index;
    size_t source_index;
    size_t source_length;
    size_t target_length;
    int converted_drive;

    converted = NULL;
    index = 0u;
    source_index = 0u;
    source_length = 0u;
    target_length = 0u;
    converted_drive = 0;

    if (path == NULL) {
        return NULL;
    }

    source_length = strlen(path);
    converted = sixel_option_duplicate_string(path);
    if (converted == NULL) {
        return NULL;
    }

    if (source_length >= 11u && strncmp(path, "/cygdrive/", 10u) == 0 &&
            isalpha((unsigned char)path[10]) &&
            (path[11] == '/' || path[11] == '\\' || path[11] == '\0')) {
        source_index = 11u;
        converted_drive = 1;
    } else if (source_length >= 2u && path[0] == '/' &&
               isalpha((unsigned char)path[1]) &&
               (path[2] == '/' || path[2] == '\\' || path[2] == '\0')) {
        source_index = 2u;
        converted_drive = 1;
    }

    if (!converted_drive) {
        return converted;
    }

    if (source_length - source_index > SIZE_MAX - 4u) {
        free(converted);
        return NULL;
    }
    target_length = source_length - source_index + 4u;
    free(converted);
    converted = (char *)malloc(target_length);
    if (converted == NULL) {
        return NULL;
    }

    converted[0] = (char)toupper((unsigned char)path[source_index - 1u]);
    converted[1] = ':';
    converted[2] = '\\';
    index = 3u;

    if (path[source_index] == '/' || path[source_index] == '\\') {
        source_index += 1u;
    }

    while (source_index < source_length) {
        converted[index] = path[source_index] == '/'
            ? '\\'
            : path[source_index];
        ++index;
        ++source_index;
    }
    converted[index] = '\0';

    return converted;
}
#endif

#if HAVE_DIRENT_H && HAVE_SYS_STAT_H || \
    (defined(_WIN32) && HAVE_WINDOWS_H)
static char *
sixel_option_join_directory_entry(
    char const *directory,
    char const *entry)
{
    size_t directory_length;
    size_t entry_length;
    int needs_separator;
    char *joined;

    directory_length = 0u;
    entry_length = 0u;
    needs_separator = 0;
    joined = NULL;

    if (directory == NULL || entry == NULL) {
        return NULL;
    }

    directory_length = strlen(directory);
    entry_length = strlen(entry);
    if (directory_length == 0u) {
        needs_separator = 0;
    } else if (directory[directory_length - 1u] == '/'
#if defined(_WIN32)
               || directory[directory_length - 1u] == '\\'
#endif
               ) {
        needs_separator = 0;
    } else {
        needs_separator = 1;
    }

    joined = (char *)malloc(directory_length + entry_length +
                            (size_t)needs_separator + 1u);
    if (joined == NULL) {
        return NULL;
    }

    if (directory_length > 0u) {
        memcpy(joined, directory, directory_length);
    }
    if (needs_separator) {
        joined[directory_length] = '/';
    }
    if (entry_length > 0u) {
        memcpy(joined + directory_length + (size_t)needs_separator,
               entry,
               entry_length);
    }
    joined[directory_length + (size_t)needs_separator + entry_length] = '\0';

    return joined;
}

static double
sixel_option_extension_similarity(
    char const *lhs,
    char const *rhs)
{
    char const *lhs_extension;
    char const *rhs_extension;

    lhs_extension = sixel_option_extension_view(lhs);
    rhs_extension = sixel_option_extension_view(rhs);
    if (lhs_extension == NULL || rhs_extension == NULL) {
        return 0.0;
    }
    if (sixel_option_case_insensitive_equals(lhs_extension, rhs_extension)) {
        return 1.0;
    }

    return 0.0;
}

static void
sixel_option_format_timestamp(
    time_t value,
    char *buffer,
    size_t buffer_size)
{
    struct tm *time_pointer;
    struct tm time_view;

    if (buffer == NULL || buffer_size == 0u) {
        return;
    }

    time_pointer = sixel_compat_localtime(&value, &time_view);
    if (time_pointer != NULL) {
        (void)strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", time_pointer);
        return;
    }

    (void)snprintf(buffer, buffer_size, "unknown");
}

static int
sixel_option_candidate_compare(void const *lhs, void const *rhs)
{
    sixel_option_path_candidate_t const *left;
    sixel_option_path_candidate_t const *right;

    left = (sixel_option_path_candidate_t const *)lhs;
    right = (sixel_option_path_candidate_t const *)rhs;

    if (left->total_score < right->total_score) {
        return 1;
    }
    if (left->total_score > right->total_score) {
        return -1;
    }
    if (left->mtime < right->mtime) {
        return 1;
    }
    if (left->mtime > right->mtime) {
        return -1;
    }

    if (left->path == NULL || right->path == NULL) {
        return 0;
    }

    return strcmp(left->path, right->path);
}

static SIXEL_OPTION_UNUSED char *
sixel_option_strerror(
    int error_number,
    char *buffer,
    size_t buffer_size)
{
    char *message;

    /*
     * Normalize strerror_r() semantics via the compatibility layer so that
     * GNU and XSI flavours behave uniformly across amalgamated builds.
     */
    message = sixel_compat_strerror(error_number, buffer, buffer_size);
    if (message == NULL) {
        buffer[0] = '\0';
    }

    return message;
}
#endif /* HAVE_DIRENT_H && HAVE_SYS_STAT_H || windows */

static int
sixel_option_path_is_clipboard(char const *argument)
{
    char const *marker;

    marker = NULL;

    if (argument == NULL) {
        return 0;
    }

    marker = strstr(argument, "clipboard:");
    if (marker == NULL) {
        return 0;
    }
    if (marker[10] != '\0') {
        return 0;
    }
    if (marker == argument) {
        return 1;
    }
    if (marker > argument && marker[-1] == ':') {
        return 1;
    }

    return 0;
}

static int
sixel_option_path_looks_remote(char const *path)
{
    char const *separator;
    size_t prefix_length;
    size_t index;
    unsigned char value;

    separator = NULL;
    prefix_length = 0u;
    index = 0u;
    value = 0u;

    if (path == NULL) {
        return 0;
    }

    separator = strstr(path, "://");
    if (separator == NULL) {
        return 0;
    }

    prefix_length = (size_t)(separator - path);
    if (prefix_length == 0u) {
        return 0;
    }

    while (index < prefix_length) {
        value = (unsigned char)path[index];
        if (!(isalpha(value) || value == '+' || value == '-' ||
              value == '.')) {
            return 0;
        }
        ++index;
    }

    return 1;
}

/*
 * Append the shared diagnostic for a missing directory while preserving
 * the caller's output buffer layout logic.
 */
static SIXEL_OPTION_UNUSED void
sixel_option_append_missing_directory_message(
    char *buffer,
    size_t buffer_size,
    size_t offset,
    char const *directory)
{
    int written;

    written = 0;

    if (buffer == NULL || buffer_size == 0u || offset >= buffer_size - 1u) {
        return;
    }

    written = snprintf(buffer + offset,
                       buffer_size - offset,
                       "Directory \"%s\" does not exist.\n",
                       directory != NULL ? directory : "(null)");
    if (written < 0) {
        written = 0;
    }
}

/*
 * Append the shared fallback message when suggestion lookup is unavailable
 * for platform or allocation reasons.
 */
static SIXEL_OPTION_UNUSED void
sixel_option_append_unavailable_suggestions_message(
    char *buffer,
    size_t buffer_size,
    size_t offset)
{
    int written;

    written = 0;

    if (buffer == NULL || buffer_size == 0u || offset >= buffer_size - 1u) {
        return;
    }

    written = snprintf(buffer + offset,
                       buffer_size - offset,
                       "Suggestion lookup unavailable on this build.\n");
    if (written < 0) {
        written = 0;
    }
}

/*
 * Compose a multi-line diagnostic highlighting the missing path along with
 * nearby suggestions.  The first line reports the original token supplied by
 * the caller so CLI wrappers can relay the exact argument the user typed.
 */
static int
sixel_option_build_missing_path_message(
    char const *argument,
    char const *resolved_path,
    char *buffer,
    size_t buffer_size)
{
    char *directory_copy;
    char const *argument_view;
    size_t offset;
    int written;
    int suggestions_enabled;
#if defined(_WIN32) && HAVE_WINDOWS_H
    char const *target_name;
    char *windows_directory_copy;
    int result;
    sixel_option_path_candidate_t *candidates;
    sixel_option_path_candidate_t *grown;
    size_t candidate_count;
    size_t candidate_capacity;
    size_t index;
    size_t new_capacity;
    char *pattern;
    size_t directory_length;
    int needs_separator;
    size_t pattern_length;
    HANDLE directory_handle;
    WIN32_FIND_DATAA find_data;
    DWORD attributes;
    ULARGE_INTEGER mtime_utc;
    ULONGLONG unix_seconds;
    time_t min_mtime;
    time_t max_mtime;
    double recency_range;
    double percent_double;
    int percent_int;
    char time_buffer[64];
#elif HAVE_DIRENT_H && HAVE_SYS_STAT_H
    char const *target_name;
    int result;
    DIR *directory_stream;
    struct dirent *entry;
    sixel_option_path_candidate_t *candidates;
    sixel_option_path_candidate_t *grown;
    size_t candidate_count;
    size_t candidate_capacity;
    size_t index;
    size_t new_capacity;
    struct stat entry_stat;
    char *candidate_path;
    time_t min_mtime;
    time_t max_mtime;
    double recency_range;
    double percent_double;
    int percent_int;
    char time_buffer[64];
    int error_code;
    char error_buffer[128];
#else
    (void)resolved_path;
#endif

    directory_copy = sixel_option_duplicate_directory(resolved_path);
    if (directory_copy == NULL) {
        return -1;
    }
    argument_view = (argument != NULL && argument[0] != '\0')
        ? argument : resolved_path;
    offset = 0u;
    written = 0;
    suggestions_enabled = 0;

    if (buffer == NULL || buffer_size == 0u) {
        free(directory_copy);
        return -1;
    }

    sixel_trace_topic_message("suggestion",
        "build missing-path diagnostics: argument=\"%s\" resolved=\"%s\" "
        "directory=\"%s\"",
        argument_view != NULL ? argument_view : "",
        resolved_path != NULL ? resolved_path : "",
        directory_copy != NULL ? directory_copy : "");

    written = snprintf(buffer,
                       buffer_size,
                       "path \"%s\" not found.\n",
                       argument_view != NULL ? argument_view : "");
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= buffer_size) {
        offset = buffer_size - 1u;
    } else {
        offset = (size_t)written;
    }

    /*
     * Prefer Win32 enumeration on Windows builds.  MSYS variants can expose
     * dirent/stat wrappers, but those wrappers may still trigger expensive
     * path translation for missing files.
     */
#if defined(_WIN32) && HAVE_WINDOWS_H
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
    if (!suggestions_enabled) {
        sixel_trace_topic_message("suggestion",
            "skip suggestion lookup because %s is disabled",
            SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
        free(directory_copy);
        return 0;
    }
    target_name = sixel_option_basename_view(resolved_path);
    windows_directory_copy = NULL;
    sixel_trace_topic_message("suggestion",
        "windows suggestion lookup: target=\"%s\" directory=\"%s\"",
        target_name != NULL ? target_name : "",
        directory_copy != NULL ? directory_copy : "");
    windows_directory_copy = sixel_option_duplicate_win32_path(directory_copy);
    if (windows_directory_copy == NULL) {
        sixel_option_append_unavailable_suggestions_message(buffer,
                                                            buffer_size,
                                                            offset);
        free(directory_copy);
        return 0;
    }
    sixel_trace_topic_message("suggestion",
        "windows path normalization: input=\"%s\" normalized=\"%s\"",
        directory_copy != NULL ? directory_copy : "",
        windows_directory_copy != NULL ? windows_directory_copy : "");
    result = 0;
    candidates = NULL;
    grown = NULL;
    candidate_count = 0u;
    candidate_capacity = 0u;
    index = 0u;
    new_capacity = 0u;
    pattern = NULL;
    directory_length = 0u;
    needs_separator = 0;
    pattern_length = 0u;
    directory_handle = INVALID_HANDLE_VALUE;
    memset(&find_data, 0, sizeof(find_data));
    attributes = 0u;
    memset(&mtime_utc, 0, sizeof(mtime_utc));
    unix_seconds = 0u;
    min_mtime = 0;
    max_mtime = 0;
    recency_range = 0.0;
    percent_double = 0.0;
    percent_int = 0;
    memset(time_buffer, 0, sizeof(time_buffer));
    sixel_trace_topic_message("suggestion",
        "windows directory probe begin: directory=\"%s\"",
        windows_directory_copy != NULL ? windows_directory_copy : "");
    attributes = GetFileAttributesA(windows_directory_copy);
    sixel_trace_topic_message("suggestion",
        "windows directory probe end: directory=\"%s\" attrs=0x%08lx",
        windows_directory_copy != NULL ? windows_directory_copy : "",
        (unsigned long)attributes);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
        sixel_trace_topic_message("suggestion",
            "directory lookup failed before enumeration: directory=\"%s\"",
            windows_directory_copy != NULL ? windows_directory_copy : "");
        sixel_option_append_missing_directory_message(buffer,
                                                      buffer_size,
                                                      offset,
                                                      directory_copy);
        free(windows_directory_copy);
        free(directory_copy);
        return result;
    }

    directory_length = strlen(windows_directory_copy);
    if (directory_length > 0u &&
            windows_directory_copy[directory_length - 1u] != '/' &&
            windows_directory_copy[directory_length - 1u] != '\\') {
        needs_separator = 1;
    }
    pattern_length = directory_length + (size_t)needs_separator + 2u;
    pattern = (char *)malloc(pattern_length);
    if (pattern == NULL) {
        sixel_option_append_unavailable_suggestions_message(buffer,
                                                            buffer_size,
                                                            offset);
        free(windows_directory_copy);
        free(directory_copy);
        return 0;
    }
    if (directory_length > 0u) {
        memcpy(pattern, windows_directory_copy, directory_length);
    }
    if (needs_separator) {
        pattern[directory_length] = '\\';
    }
    pattern[directory_length + (size_t)needs_separator] = '*';
    pattern[directory_length + (size_t)needs_separator + 1u] = '\0';

    sixel_trace_topic_message("suggestion",
        "windows candidate scan begin: pattern=\"%s\"",
        pattern != NULL ? pattern : "");
    directory_handle = FindFirstFileA(pattern, &find_data);
    free(pattern);
    pattern = NULL;
    if (directory_handle == INVALID_HANDLE_VALUE) {
        sixel_trace_topic_message("suggestion",
            "FindFirstFileA failed: directory=\"%s\"",
            windows_directory_copy != NULL ? windows_directory_copy : "");
        sixel_option_append_missing_directory_message(buffer,
                                                      buffer_size,
                                                      offset,
                                                      directory_copy);
        free(windows_directory_copy);
        free(directory_copy);
        return result;
    }

    sixel_trace_topic_message("suggestion",
        "windows candidate scan first entry: name=\"%s\"",
        find_data.cFileName);
    do {
        if (find_data.cFileName[0] == '.' &&
                (find_data.cFileName[1] == '\0' ||
                 (find_data.cFileName[1] == '.' &&
                  find_data.cFileName[2] == '\0'))) {
            continue;
        }
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
            continue;
        }
        if (candidate_count == candidate_capacity) {
            new_capacity = (candidate_capacity == 0u)
                ? 8u
                : candidate_capacity * 2u;
            grown = (sixel_option_path_candidate_t *)realloc(
                candidates,
                new_capacity * sizeof(sixel_option_path_candidate_t));
            if (grown == NULL) {
                break;
            }
            candidates = grown;
            candidate_capacity = new_capacity;
        }
        candidates[candidate_count].path = sixel_option_join_directory_entry(
            directory_copy,
            find_data.cFileName);
        if (candidates[candidate_count].path == NULL) {
            continue;
        }
        mtime_utc.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
        mtime_utc.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
        if (mtime_utc.QuadPart >= 116444736000000000ULL) {
            unix_seconds =
                (mtime_utc.QuadPart - 116444736000000000ULL) / 10000000ULL;
        } else {
            unix_seconds = 0ULL;
        }
        candidates[candidate_count].name =
            sixel_option_basename_view(candidates[candidate_count].path);
        candidates[candidate_count].mtime = (time_t)unix_seconds;
        candidates[candidate_count].name_score = 0.0;
        candidates[candidate_count].extension_score = 0.0;
        candidates[candidate_count].recency_score = 0.0;
        candidates[candidate_count].total_score = 0.0;
        ++candidate_count;
    } while (FindNextFileA(directory_handle, &find_data) != 0);

    if (directory_handle != INVALID_HANDLE_VALUE) {
        (void)FindClose(directory_handle);
        directory_handle = INVALID_HANDLE_VALUE;
    }

    sixel_trace_topic_message("suggestion",
        "enumeration finished: directory=\"%s\" candidates=%lu",
        directory_copy != NULL ? directory_copy : "",
        (unsigned long)candidate_count);

    if (candidate_count == 0u) {
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "No nearby matches were found in \"%s\".\n",
                               directory_copy != NULL
                                   ? directory_copy
                                   : "(null)");
            if (written < 0) {
                written = 0;
            }
        }
        free(windows_directory_copy);
        free(directory_copy);
        return 0;
    }

    min_mtime = candidates[0].mtime;
    max_mtime = candidates[0].mtime;
    for (index = 0u; index < candidate_count; ++index) {
        candidates[index].name_score =
            sixel_option_normalized_levenshtein(target_name,
                                                candidates[index].name,
                                                NULL);
        candidates[index].extension_score =
            sixel_option_extension_similarity(target_name,
                                              candidates[index].name);
        if (index == 0u || candidates[index].mtime < min_mtime) {
            min_mtime = candidates[index].mtime;
        }
        if (index == 0u || candidates[index].mtime > max_mtime) {
            max_mtime = candidates[index].mtime;
        }
    }

    recency_range = (double)(max_mtime - min_mtime);
    for (index = 0u; index < candidate_count; ++index) {
        if (recency_range <= 0.0) {
            candidates[index].recency_score = 1.0;
        } else {
            candidates[index].recency_score =
                (double)(candidates[index].mtime - min_mtime) /
                recency_range;
        }
        candidates[index].total_score =
            SIXEL_OPTION_SUGGESTION_NAME_WEIGHT *
                candidates[index].name_score +
            SIXEL_OPTION_SUGGESTION_EXTENSION_WEIGHT *
                candidates[index].extension_score +
            SIXEL_OPTION_SUGGESTION_RECENCY_WEIGHT *
                candidates[index].recency_score;
    }

    sixel_trace_topic_message("suggestion",
        "windows ranking begin: candidates=%lu",
        (unsigned long)candidate_count);
    qsort(candidates,
          candidate_count,
          sizeof(sixel_option_path_candidate_t),
          sixel_option_candidate_compare);
    sixel_trace_topic_message("suggestion",
        "windows ranking end: top=\"%s\"",
        candidate_count > 0u && candidates[0].path != NULL
            ? candidates[0].path
            : "(none)");

    if (offset < buffer_size - 1u) {
        written = snprintf(buffer + offset,
                           buffer_size - offset,
                           "Suggestions:\n");
        if (written < 0) {
            written = 0;
        }
        if ((size_t)written >= buffer_size - offset) {
            offset = buffer_size - 1u;
        } else {
            offset += (size_t)written;
        }
    }

    for (index = 0u; index < candidate_count &&
            index < SIXEL_OPTION_SUGGESTION_LIMIT; ++index) {
        percent_double = candidates[index].total_score * 100.0;
        if (percent_double < 0.0) {
            percent_double = 0.0;
        }
        if (percent_double > 100.0) {
            percent_double = 100.0;
        }
        percent_int = (int)(percent_double + 0.5);
        sixel_option_format_timestamp(candidates[index].mtime,
                                      time_buffer,
                                      sizeof(time_buffer));
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "  * %s (similarity score %d%%, "
                               "modified %s)\n",
                               candidates[index].path != NULL
                                   ? candidates[index].path
                                   : "(unknown)",
                               percent_int,
                               time_buffer);
            if (written < 0) {
                written = 0;
            }
            if ((size_t)written >= buffer_size - offset) {
                offset = buffer_size - 1u;
            } else {
                offset += (size_t)written;
            }
        }
    }

    sixel_trace_topic_message("suggestion",
        "ranked suggestions ready: emitted=%lu total=%lu target=\"%s\"",
        (unsigned long)((candidate_count < SIXEL_OPTION_SUGGESTION_LIMIT)
            ? candidate_count
            : SIXEL_OPTION_SUGGESTION_LIMIT),
        (unsigned long)candidate_count,
        target_name != NULL ? target_name : "");

    if (candidates != NULL) {
        for (index = 0u; index < candidate_count; ++index) {
            free(candidates[index].path);
            candidates[index].path = NULL;
        }
        free(candidates);
        candidates = NULL;
    }
    free(windows_directory_copy);
    free(directory_copy);

    return result;
#elif HAVE_DIRENT_H && HAVE_SYS_STAT_H
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
    if (!suggestions_enabled) {
        sixel_trace_topic_message("suggestion",
            "skip suggestion lookup because %s is disabled",
            SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
        free(directory_copy);
        return 0;
    }
    target_name = sixel_option_basename_view(resolved_path);
    sixel_trace_topic_message("suggestion",
        "posix suggestion lookup: target=\"%s\" directory=\"%s\"",
        target_name != NULL ? target_name : "",
        directory_copy != NULL ? directory_copy : "");
    result = 0;
    directory_stream = NULL;
    entry = NULL;
    candidates = NULL;
    grown = NULL;
    candidate_count = 0u;
    candidate_capacity = 0u;
    index = 0u;
    new_capacity = 0u;
    memset(&entry_stat, 0, sizeof(entry_stat));
    candidate_path = NULL;
    min_mtime = 0;
    max_mtime = 0;
    recency_range = 0.0;
    percent_double = 0.0;
    percent_int = 0;
    memset(time_buffer, 0, sizeof(time_buffer));
    sixel_trace_topic_message("suggestion",
        "posix directory open begin: directory=\"%s\"",
        directory_copy != NULL ? directory_copy : "");
    directory_stream = opendir(directory_copy);
    sixel_trace_topic_message("suggestion",
        "posix directory open end: directory=\"%s\" ok=%d",
        directory_copy != NULL ? directory_copy : "",
        directory_stream != NULL ? 1 : 0);
    if (directory_stream == NULL) {
        error_code = errno;
        sixel_trace_topic_message("suggestion",
            "opendir failed: directory=\"%s\" errno=%d",
            directory_copy != NULL ? directory_copy : "",
            error_code);
        if (error_code == ENOENT) {
            if (offset < buffer_size - 1u) {
                written = snprintf(buffer + offset,
                                   buffer_size - offset,
                                   "Directory \"%s\" does not exist.\n",
                                   directory_copy != NULL
                                       ? directory_copy
                                       : "(null)");
                if (written < 0) {
                    written = 0;
                }
            }
        } else {
            if (sixel_option_strerror(error_code,
                                      error_buffer,
                                      sizeof(error_buffer)) == NULL) {
                error_buffer[0] = '\0';
            }
            if (offset < buffer_size - 1u) {
                written = snprintf(buffer + offset,
                                   buffer_size - offset,
                                   "Unable to inspect \"%s\": %s\n",
                                   directory_copy != NULL
                                       ? directory_copy
                                       : "(null)",
                                   error_buffer[0] != '\0'
                                       ? error_buffer
                                       : "unknown error");
                if (written < 0) {
                    written = 0;
                }
            }
        }
        free(directory_copy);
        return result;
    }

    sixel_trace_topic_message("suggestion",
        "posix candidate scan begin: directory=\"%s\"",
        directory_copy != NULL ? directory_copy : "");
    while ((entry = readdir(directory_stream)) != NULL) {
        if (entry->d_name[0] == '.' &&
                (entry->d_name[1] == '\0' ||
                 (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        candidate_path = sixel_option_join_directory_entry(directory_copy,
                                                            entry->d_name);
        if (candidate_path == NULL) {
            continue;
        }
        if (stat(candidate_path, &entry_stat) != 0) {
            free(candidate_path);
            candidate_path = NULL;
            continue;
        }
        if (!S_ISREG(entry_stat.st_mode)) {
#if defined(S_ISLNK)
            if (!S_ISLNK(entry_stat.st_mode)) {
                free(candidate_path);
                candidate_path = NULL;
                continue;
            }
#else
            free(candidate_path);
            candidate_path = NULL;
            continue;
#endif
        }
        if (candidate_count == candidate_capacity) {
            new_capacity = (candidate_capacity == 0u)
                ? 8u
                : candidate_capacity * 2u;
            grown = (sixel_option_path_candidate_t *)realloc(
                candidates,
                new_capacity * sizeof(sixel_option_path_candidate_t));
            if (grown == NULL) {
                free(candidate_path);
                candidate_path = NULL;
                break;
            }
            candidates = grown;
            candidate_capacity = new_capacity;
        }
        candidates[candidate_count].path = candidate_path;
        candidates[candidate_count].name =
            sixel_option_basename_view(candidate_path);
        candidates[candidate_count].mtime = entry_stat.st_mtime;
        candidates[candidate_count].name_score = 0.0;
        candidates[candidate_count].extension_score = 0.0;
        candidates[candidate_count].recency_score = 0.0;
        candidates[candidate_count].total_score = 0.0;
        ++candidate_count;
        candidate_path = NULL;
    }

    if (directory_stream != NULL) {
        (void)closedir(directory_stream);
        directory_stream = NULL;
    }

    sixel_trace_topic_message("suggestion",
        "posix candidate scan end: directory=\"%s\" candidates=%lu",
        directory_copy != NULL ? directory_copy : "",
        (unsigned long)candidate_count);
    if (candidate_count == 0u) {
        sixel_trace_topic_message("suggestion",
            "enumeration finished with no candidates: directory=\"%s\"",
            directory_copy != NULL ? directory_copy : "");
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "No nearby matches were found in \"%s\".\n",
                               directory_copy != NULL
                                   ? directory_copy
                                   : "(null)");
            if (written < 0) {
                written = 0;
            }
        }
        free(directory_copy);
        return 0;
    }

    min_mtime = candidates[0].mtime;
    max_mtime = candidates[0].mtime;
    for (index = 0u; index < candidate_count; ++index) {
        candidates[index].name_score =
            sixel_option_normalized_levenshtein(target_name,
                                                 candidates[index].name,
                                                 NULL);
        candidates[index].extension_score =
            sixel_option_extension_similarity(target_name,
                                              candidates[index].name);
        if (index == 0u || candidates[index].mtime < min_mtime) {
            min_mtime = candidates[index].mtime;
        }
        if (index == 0u || candidates[index].mtime > max_mtime) {
            max_mtime = candidates[index].mtime;
        }
    }

    recency_range = (double)(max_mtime - min_mtime);
    for (index = 0u; index < candidate_count; ++index) {
        if (recency_range <= 0.0) {
            candidates[index].recency_score = 1.0;
        } else {
            candidates[index].recency_score =
                (double)(candidates[index].mtime - min_mtime) /
                recency_range;
        }
        candidates[index].total_score =
            SIXEL_OPTION_SUGGESTION_NAME_WEIGHT *
                candidates[index].name_score +
            SIXEL_OPTION_SUGGESTION_EXTENSION_WEIGHT *
                candidates[index].extension_score +
            SIXEL_OPTION_SUGGESTION_RECENCY_WEIGHT *
                candidates[index].recency_score;
    }

    sixel_trace_topic_message("suggestion",
        "posix ranking begin: candidates=%lu",
        (unsigned long)candidate_count);
    qsort(candidates,
          candidate_count,
          sizeof(sixel_option_path_candidate_t),
          sixel_option_candidate_compare);
    sixel_trace_topic_message("suggestion",
        "posix ranking end: top=\"%s\"",
        candidate_count > 0u && candidates[0].path != NULL
            ? candidates[0].path
            : "(none)");

    if (offset < buffer_size - 1u) {
        written = snprintf(buffer + offset,
                           buffer_size - offset,
                           "Suggestions:\n");
        if (written < 0) {
            written = 0;
        }
        if ((size_t)written >= buffer_size - offset) {
            offset = buffer_size - 1u;
        } else {
            offset += (size_t)written;
        }
    }

    for (index = 0u; index < candidate_count &&
            index < SIXEL_OPTION_SUGGESTION_LIMIT; ++index) {
        percent_double = candidates[index].total_score * 100.0;
        if (percent_double < 0.0) {
            percent_double = 0.0;
        }
        if (percent_double > 100.0) {
            percent_double = 100.0;
        }
        percent_int = (int)(percent_double + 0.5);
        sixel_option_format_timestamp(candidates[index].mtime,
                                      time_buffer,
                                      sizeof(time_buffer));
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "  * %s (similarity score %d%%, "
                               "modified %s)\n",
                               candidates[index].path != NULL
                                   ? candidates[index].path
                                   : "(unknown)",
                               percent_int,
                               time_buffer);
            if (written < 0) {
                written = 0;
            }
            if ((size_t)written >= buffer_size - offset) {
                offset = buffer_size - 1u;
            } else {
                offset += (size_t)written;
            }
        }
    }

    sixel_trace_topic_message("suggestion",
        "ranked suggestions ready: emitted=%lu total=%lu target=\"%s\"",
        (unsigned long)((candidate_count < SIXEL_OPTION_SUGGESTION_LIMIT)
            ? candidate_count
            : SIXEL_OPTION_SUGGESTION_LIMIT),
        (unsigned long)candidate_count,
        target_name != NULL ? target_name : "");

    if (directory_stream != NULL) {
        (void)closedir(directory_stream);
        directory_stream = NULL;
    }
    if (candidates != NULL) {
        for (index = 0u; index < candidate_count; ++index) {
            free(candidates[index].path);
            candidates[index].path = NULL;
        }
        free(candidates);
        candidates = NULL;
    }
    free(directory_copy);

    return result;
#else
    sixel_trace_topic_message("suggestion",
        "suggestion lookup unavailable at compile time for this platform");
    sixel_option_append_unavailable_suggestions_message(buffer,
                                                        buffer_size,
                                                        offset);
    free(directory_copy);
    return 0;
#endif
}

static void
sixel_option_trace_path_probe_begin(
    char const *argument,
    char const *resolved_path,
    unsigned int flags)
{
    char const *argument_view;
    char const *resolved_view;

    if (!loader_trace_is_enabled()) {
        return;
    }

    argument_view = argument != NULL ? argument : "(null)";
    resolved_view = resolved_path != NULL ? resolved_path : "(null)";

    loader_trace_message(
        "path probe begin: arg=\"%s\" resolved=\"%s\" "
        "flags=0x%04x",
        argument_view,
        resolved_view,
        flags);
}

static void
sixel_option_trace_path_probe_end(
    char const *argument,
    char const *resolved_path,
    unsigned int flags,
    int result,
    int error_value,
    double elapsed_seconds)
{
    char const *argument_view;
    char const *resolved_view;

    if (!loader_trace_is_enabled()) {
        return;
    }

    argument_view = argument != NULL ? argument : "(null)";
    resolved_view = resolved_path != NULL ? resolved_path : "(null)";

    loader_trace_message(
        "path probe end: arg=\"%s\" resolved=\"%s\" "
        "flags=0x%04x result=%d errno=%d elapsed=%.3fs",
        argument_view,
        resolved_view,
        flags,
        result,
        error_value,
        elapsed_seconds);
}

int
sixel_option_validate_filesystem_path(
    char const *argument,
    char const *resolved_path,
    unsigned int flags)
{
#if !(HAVE_SYS_STAT_H)
    (void)argument;
    (void)resolved_path;
    (void)flags;
    return 0;
#else
    int stat_result;
    int error_value;
    int allow_stdin;
    int allow_clipboard;
    int allow_remote;
    int allow_empty;
    char const *remote_view;
    char message_buffer[1024];
    struct stat stat_buffer;
    int stat_check;
#if defined(__EMSCRIPTEN__)
    int fallback_stat_result;
    int fallback_error_value;
#endif
    clock_t start_ticks;
    clock_t end_ticks;
    double elapsed_seconds;

    stat_result = 0;
    error_value = 0;
    allow_stdin = (flags & SIXEL_OPTION_PATH_ALLOW_STDIN) != 0u;
    allow_clipboard = (flags & SIXEL_OPTION_PATH_ALLOW_CLIPBOARD) != 0u;
    allow_remote = (flags & SIXEL_OPTION_PATH_ALLOW_REMOTE) != 0u;
    allow_empty = (flags & SIXEL_OPTION_PATH_ALLOW_EMPTY) != 0u;
    remote_view = resolved_path != NULL ? resolved_path : argument;
    memset(message_buffer, 0, sizeof(message_buffer));
    start_ticks = 0;
    end_ticks = 0;
    memset(&stat_buffer, 0, sizeof(stat_buffer));
    stat_check = 0;
#if defined(__EMSCRIPTEN__)
    fallback_stat_result = 0;
    fallback_error_value = 0;
#endif
    elapsed_seconds = -1.0;

    /*
     * Reject empty path arguments unless the caller explicitly opts in.
     * Historically the CLI rejected blank -i payloads while tolerating empty
     * -m mapfile values, so the new flag preserves that behaviour without
     * reintroducing option-specific strings here.
     */
    if ((argument == NULL || argument[0] == '\0') &&
            (resolved_path == NULL || resolved_path[0] == '\0')) {
        if (!allow_empty) {
            sixel_trace_topic_message(
                "path",
                "reject empty argument and resolved path: flags=0x%04x",
                flags);
            sixel_helper_set_additional_message(
                "path argument is empty.");
            return -1;
        }
        sixel_trace_topic_message(
            "path",
            "allow empty argument and resolved path: flags=0x%04x",
            flags);
        return 0;
    }

    if (resolved_path == NULL || resolved_path[0] == '\0') {
        if (!allow_empty) {
            sixel_trace_topic_message(
                "path",
                "reject empty resolved path: argument=\"%s\"",
                argument != NULL ? argument : "");
            sixel_helper_set_additional_message(
                "path argument is empty.");
            return -1;
        }
        sixel_trace_topic_message(
            "path",
            "allow empty resolved path: argument=\"%s\"",
            argument != NULL ? argument : "");
        return 0;
    }

    sixel_trace_topic_message(
        "suggestion",
        "path validation start: argument=\"%s\" resolved=\"%s\" "
        "flags=0x%04x",
        argument != NULL ? argument : "",
        resolved_path != NULL ? resolved_path : "",
        flags);

    if (allow_stdin && argument != NULL && strcmp(argument, "-") == 0) {
        sixel_trace_topic_message(
            "path",
            "bypass probe for stdin argument '-'");
        return 0;
    }
    /*
     * Palette prefixes like "gpl:-" resolve to "-" after stripping. Allow
     * those through the stdin fast-path as well so prefixed mapfile values
     * behave the same as a bare "-" argument.
     */
    if (allow_stdin && resolved_path != NULL && strcmp(resolved_path, "-")
            == 0) {
        sixel_trace_topic_message(
            "path",
            "bypass probe for resolved stdin path '-'");
        return 0;
    }
    if (allow_clipboard && sixel_option_path_is_clipboard(argument)) {
        sixel_trace_topic_message(
            "path",
            "bypass probe for clipboard argument=\"%s\"",
            argument != NULL ? argument : "");
        return 0;
    }
    if (allow_remote && sixel_option_path_looks_remote(remote_view)) {
        sixel_trace_topic_message(
            "path",
            "bypass probe for remote path=\"%s\"",
            remote_view != NULL ? remote_view : "");
        return 0;
    }

    /*
     * Emit verbose timing around the access() probe so Windows-specific
     * hangs become visible when the CLI is invoked with -v/--verbose.
     */
    if (loader_trace_is_enabled()) {
        sixel_option_trace_path_probe_begin(argument, resolved_path, flags);
    }

    sixel_trace_topic_message(
        "suggestion",
        "path probe begin: resolved=\"%s\"",
        resolved_path);

    start_ticks = clock();
    errno = 0;
    stat_result = sixel_compat_access(resolved_path, F_OK);
    error_value = errno;
    end_ticks = clock();
    if (start_ticks != (clock_t)(-1) && end_ticks != (clock_t)(-1)) {
        elapsed_seconds = (double)(end_ticks - start_ticks) /
            (double)CLOCKS_PER_SEC;
    }
    sixel_trace_topic_message(
        "suggestion",
        "path probe end: resolved=\"%s\" result=%d errno=%d elapsed=%.3fs",
        resolved_path,
        stat_result,
        error_value,
        elapsed_seconds);

    sixel_trace_topic_message(
        "path",
        "access probe: path=\"%s\" result=%d errno=%d elapsed=%.3fs",
        resolved_path,
        stat_result,
        error_value,
        elapsed_seconds);
    /*
     * Prefer the compat layer over stat() here to avoid the Win32 path
     * resolver's UNC probes, which can block for minutes on missing or
     * non-local paths.  We only need an existence check, so access()
     * provides the lightest available probe.
     */
    if (stat_result == 0) {
        /*
         * Treat existing directories as invalid inputs because callers expect
         * regular files.  Using the compat stat() helper after a successful
         * access() confines the potentially blocking probe to paths that
         * already exist locally, keeping the earlier UNC avoidance intact.
         */
        errno = 0;
        stat_check = sixel_compat_stat(resolved_path, &stat_buffer);
        error_value = errno;
        if (stat_check == 0 && S_ISDIR(stat_buffer.st_mode)) {
            sixel_trace_topic_message(
                "path",
                "reject directory path=\"%s\"",
                resolved_path);
            sixel_helper_set_additional_message(
                "path refers to a directory; expected a file input.");
            return -1;
        }
        sixel_trace_topic_message(
            "path",
            "path exists and is accepted: path=\"%s\"",
            resolved_path);
        return 0;
    }

    error_value = errno;
#if defined(__EMSCRIPTEN__)
    if ((error_value == ENOENT || error_value == ENOTDIR)) {
        /*
         * NODERAWFS environments can report false negatives from access()
         * on newly created files. Re-check with stat() before surfacing
         * missing-path diagnostics.
         */
        errno = 0;
        fallback_stat_result = sixel_compat_stat(resolved_path, &stat_buffer);
        fallback_error_value = errno;
        if (fallback_stat_result == 0) {
            if (S_ISDIR(stat_buffer.st_mode)) {
                sixel_trace_topic_message(
                    "path",
                    "reject directory path=\"%s\" (stat fallback)",
                    resolved_path);
                sixel_helper_set_additional_message(
                    "path refers to a directory; expected a file input.");
                return -1;
            }
            sixel_trace_topic_message(
                "path",
                "path accepted via stat fallback: path=\"%s\"",
                resolved_path);
            return 0;
        }
        if (fallback_error_value != 0) {
            error_value = fallback_error_value;
        }
    }
#endif
    if (error_value != ENOENT && error_value != ENOTDIR) {
        sixel_trace_topic_message(
            "path",
            "skip missing-path diagnostics: path=\"%s\" errno=%d",
            resolved_path,
            error_value);
        sixel_trace_topic_message(
            "suggestion",
            "skip missing-path diagnostics: resolved=\"%s\" errno=%d",
            resolved_path,
            error_value);
        if (loader_trace_is_enabled()) {
            sixel_option_trace_path_probe_end(argument,
                                              resolved_path,
                                              flags,
                                              stat_result,
                                              error_value,
                                              elapsed_seconds);
        }
        return 0;
    }

    if (loader_trace_is_enabled()) {
        sixel_option_trace_path_probe_end(argument,
                                          resolved_path,
                                          flags,
                                          stat_result,
                                          error_value,
                                          elapsed_seconds);
    }

    sixel_trace_topic_message(
        "path",
        "enter missing-path diagnostics: argument=\"%s\" resolved=\"%s\"",
        argument != NULL ? argument : "",
        resolved_path != NULL ? resolved_path : "");
    sixel_trace_topic_message(
        "suggestion",
        "enter missing-path diagnostics: argument=\"%s\" resolved=\"%s\"",
        argument != NULL ? argument : "",
        resolved_path != NULL ? resolved_path : "");

    if (sixel_option_build_missing_path_message(argument,
                                                resolved_path,
                                                message_buffer,
                                                sizeof(message_buffer)) != 0) {
        sixel_helper_set_additional_message(
            "path validation failed.");
    } else {
        sixel_helper_set_additional_message(message_buffer);
    }

    return -1;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
