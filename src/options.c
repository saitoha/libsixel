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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

sixel_option_choice_result_t
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

    index = 0u;
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

void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size)
{
    int written;
    int suggestions_enabled;
    char const *active_candidates;

    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PREFIX_SUGGESTIONS);
    active_candidates = suggestions_enabled ? candidates : NULL;
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

void
sixel_option_report_invalid_choice(
    char const *base_message,
    char const *suggestions,
    char *buffer,
    size_t buffer_size)
{
    int written;

    if (base_message == NULL) {
        return;
    }

    if (suggestions != NULL && suggestions[0] != '\0' && buffer != NULL &&
        buffer_size > 0u) {
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

static char *
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
#if HAVE_DIRENT_H && HAVE_SYS_STAT_H
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
#if defined(_WIN32) && HAVE_WINDOWS_H
    char const *target_name;
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
#else
    (void)resolved_path;
#endif
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

#if !(HAVE_DIRENT_H && HAVE_SYS_STAT_H)
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
    if (!suggestions_enabled) {
        free(directory_copy);
        return 0;
    }
#if defined(_WIN32) && HAVE_WINDOWS_H
    target_name = sixel_option_basename_view(resolved_path);
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
    attributes = GetFileAttributesA(directory_copy);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
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
        free(directory_copy);
        return result;
    }

    directory_length = strlen(directory_copy);
    if (directory_length > 0u &&
            directory_copy[directory_length - 1u] != '/' &&
            directory_copy[directory_length - 1u] != '\\') {
        needs_separator = 1;
    }
    pattern_length = directory_length + (size_t)needs_separator + 2u;
    pattern = (char *)malloc(pattern_length);
    if (pattern == NULL) {
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "Suggestion lookup unavailable "
                               "on this build.\n");
            if (written < 0) {
                written = 0;
            }
        }
        free(directory_copy);
        return 0;
    }
    if (directory_length > 0u) {
        memcpy(pattern, directory_copy, directory_length);
    }
    if (needs_separator) {
        pattern[directory_length] = '\\';
    }
    pattern[directory_length + (size_t)needs_separator] = '*';
    pattern[directory_length + (size_t)needs_separator + 1u] = '\0';

    directory_handle = FindFirstFileA(pattern, &find_data);
    free(pattern);
    pattern = NULL;
    if (directory_handle == INVALID_HANDLE_VALUE) {
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
        free(directory_copy);
        return result;
    }

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

    qsort(candidates,
          candidate_count,
          sizeof(sixel_option_path_candidate_t),
          sixel_option_candidate_compare);

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
    if (offset < buffer_size - 1u) {
        written = snprintf(buffer + offset,
                           buffer_size - offset,
                           "Suggestion lookup unavailable on this build.\n");
        if (written < 0) {
            written = 0;
        }
    }
    free(directory_copy);
    return 0;
#endif
#else
    suggestions_enabled = sixel_option_environment_is_enabled(
        SIXEL_OPTION_ENV_PATH_SUGGESTIONS);
    if (!suggestions_enabled) {
        free(directory_copy);
        return 0;
    }
    target_name = sixel_option_basename_view(resolved_path);
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
    directory_stream = opendir(directory_copy);
    if (directory_stream == NULL) {
        error_code = errno;
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

    qsort(candidates,
          candidate_count,
          sizeof(sixel_option_path_candidate_t),
          sixel_option_candidate_compare);

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
            sixel_helper_set_additional_message(
                "path argument is empty.");
            return -1;
        }
        return 0;
    }

    if (resolved_path == NULL || resolved_path[0] == '\0') {
        if (!allow_empty) {
            sixel_helper_set_additional_message(
                "path argument is empty.");
            return -1;
        }
        return 0;
    }
    if (allow_stdin && argument != NULL && strcmp(argument, "-") == 0) {
        return 0;
    }
    /*
     * Palette prefixes like "gpl:-" resolve to "-" after stripping. Allow
     * those through the stdin fast-path as well so prefixed mapfile values
     * behave the same as a bare "-" argument.
     */
    if (allow_stdin && resolved_path != NULL && strcmp(resolved_path, "-")
            == 0) {
        return 0;
    }
    if (allow_clipboard && sixel_option_path_is_clipboard(argument)) {
        return 0;
    }
    if (allow_remote && sixel_option_path_looks_remote(remote_view)) {
        return 0;
    }

    /*
     * Emit verbose timing around the access() probe so Windows-specific
     * hangs become visible when the CLI is invoked with -v/--verbose.
     */
    if (loader_trace_is_enabled()) {
        sixel_option_trace_path_probe_begin(argument, resolved_path, flags);
    }

    start_ticks = clock();
    errno = 0;
    stat_result = sixel_compat_access(resolved_path, F_OK);
    error_value = errno;
    end_ticks = clock();
    if (start_ticks != (clock_t)(-1) && end_ticks != (clock_t)(-1)) {
        elapsed_seconds = (double)(end_ticks - start_ticks) /
            (double)CLOCKS_PER_SEC;
    }
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
            sixel_helper_set_additional_message(
                "path refers to a directory; expected a file input.");
            return -1;
        }
        return 0;
    }

    error_value = errno;
    if (error_value != ENOENT && error_value != ENOTDIR) {
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
