/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2017 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"
#include "malloc_stub.h"
#include "getopt_stub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STDARG_H
# include <limits.h>
#endif  /* HAVE_STDARG_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_IO_H
# include <io.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>  /* getopt */
#endif
#if HAVE_GETOPT_H
# include <getopt.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif

#include <sixel.h>


static char *
cli_compat_strerror(int error_number,
                      char *buffer,
                      size_t buffer_size)
{
#if defined(_MSC_VER)
    errno_t status;
#elif defined(_WIN32)
# if defined(__STDC_LIB_EXT1__)
    errno_t status;
# else
    char *message;
    size_t copy_length;
# endif
#else
# if defined(_GNU_SOURCE) || (!defined(HAVE__STRERROR_R) && !defined(HAVE_STRERROR_R))
    char *message;
    size_t copy_length;
# endif
#endif

    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }

#if defined(_MSC_VER)
    status = strerror_s(buffer, buffer_size, error_number);
    if (status != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
#elif defined(_WIN32)
    /*
     * +----------------------------------------------------+
     * |  Windows family error messages                     |
     * +----------------------------------------------------+
     * |  CRT flavor  |  Routine we can rely on             |
     * |------------- +-------------------------------------|
     * |  Annex K     |  strerror_s()                       |
     * |  Legacy      |  strerror() + manual copy           |
     * +----------------------------------------------------+
     * The secure CRT is present both with MSVC and with
     * clang + UCRT.  When Annex K is unavailable we fall
     * back to strerror() while still clamping the output.
     */
# if defined(__STDC_LIB_EXT1__)
    status = strerror_s(buffer, buffer_size, error_number);
    if (status != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
# else
    message = strerror(error_number);
    if (message == NULL) {
        buffer[0] = '\0';
        return NULL;
    }
    copy_length = buffer_size - 1;
    (void)strncpy(buffer, message, copy_length);
    buffer[buffer_size - 1] = '\0';
    return buffer;
# endif
#else
# if defined(_GNU_SOURCE)
    message = strerror_r(error_number, buffer, buffer_size);
    if (message == NULL) {
        return NULL;
    }
    if (message != buffer) {
        copy_length = buffer_size - 1;
        (void)strncpy(buffer, message, copy_length);
        buffer[buffer_size - 1] = '\0';
    }
# elif defined(HAVE__STRERROR_R)
    if (_strerror_r(error_number, buffer, buffer_size) != 0) {
        buffer[0] = '\0';
        return NULL;
    }
# elif defined(HAVE_STRERROR_R)
    if (strerror_r(error_number, buffer, buffer_size) != 0) {
        buffer[0] = '\0';
        return NULL;
    }
# else  /* HAVE_STRERROR */
    message = strerror(error_number);
    if (message == NULL) {
        buffer[0] = '\0';
        return NULL;
    }
    copy_length = buffer_size - 1;
    (void)strncpy(buffer, message, copy_length);
    buffer[buffer_size - 1] = '\0';
# endif
    return buffer;
#endif
}


/*
 * Option-specific help snippets power both the --help output and contextual
 * diagnostics.  The following ASCII table illustrates the structure we keep
 * synchronized:
 *
 *   +-----------+-------------+-----------------------------+
 *   | short opt | long option | contextual help text        |
 *   +-----------+-------------+-----------------------------+
 *
 * With this table we can guide the user toward the relevant manual section as
 * soon as they provide an unsupported argument.
 */
typedef struct sixel2png_option_help {
    int short_opt;
    char const *long_opt;
    char const *help;
} sixel2png_option_help_t;

static sixel2png_option_help_t const g_option_help_table[] = {
    {
        'i',
        "input",
        "-i FILE, --input=FILE      specify input file\n"
        "                           use '-' to read from stdin.\n"
    },
    {
        'o',
        "output",
        "-o FILE, --output=FILE     specify output file\n"
        "                           use '-' to write to stdout or\n"
        "                           prefix the target with \"png:\"\n"
        "                           so \"png:-\" maps to stdout and\n"
        "                           \"png:<path>\" strips the prefix\n"
        "                           before saving to disk.\n"
    },
    {
        'd',
        "dequantize",
        "-d METHOD, --dequantize=METHOD\n"
        "                           apply palette dequantization.\n"
        "                           METHOD is one of:\n"
        "                             none        -> disable\n"
        "                             k_undither  -> Kornelski's\n"
        "                                            undither\n"
        "                                            (refine off)\n"
        "                             k_undither+ -> Kornelski's\n"
        "                                            undither\n"
        "                                            (refine on)\n"
    },
    {
        'S',
        "similarity",
        "-S BIAS, --similarity=BIAS specify similarity bias\n"
        "                           range: 0-1000 (default: 100).\n"
    },
    {
        's',
        "size",
        "-s SIZE, --size=SIZE       scale longer edge to SIZE pixels\n"
        "                           while preserving aspect ratio.\n"
    },
    {
        'e',
        "edge",
        "-e BIAS, --edge=BIAS       specify edge protection bias\n"
        "                           range: 0-1000 (default: 0).\n"
    },
    {
        'D',
        "direct",
        "-D, --direct               emit RGBA PNG output with\n"
        "                           direct rendering staragegy.\n"
    },
    {
        'V',
        "version",
        "-V, --version              show version and license info.\n"
    },
    {
        'H',
        "help",
        "-H, --help                 show this help.\n"
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"sixel2png -H\" for more details.\n";

static char const g_sixel2png_optstring[] = "i:o:d:S:e:s:DVH";

static sixel2png_option_help_t const *
sixel2png_find_option_help(int short_opt)
{
    size_t index;
    size_t count;

    index = 0u;
    count = sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
    while (index < count) {
        if (g_option_help_table[index].short_opt == short_opt) {
            return &g_option_help_table[index];
        }
        ++index;
    }

    return NULL;
}

static sixel2png_option_help_t const *
sixel2png_find_option_help_by_long_name(char const *long_name)
{
    size_t index;
    size_t count;

    index = 0u;
    count = sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
    while (index < count) {
        if (g_option_help_table[index].long_opt != NULL
                && long_name != NULL
                && strcmp(g_option_help_table[index].long_opt,
                          long_name) == 0) {
            return &g_option_help_table[index];
        }
        ++index;
    }

    return NULL;
}

static int
sixel2png_option_requires_argument(int short_opt)
{
    char const *cursor;

    cursor = g_sixel2png_optstring;

    while (*cursor != '\0') {
        if (*cursor == (char)short_opt) {
            if (cursor[1] == ':') {
                return 1;
            }
            return 0;
        }
        cursor += 1;
        while (*cursor == ':') {
            cursor += 1;
        }
    }

    return 0;
}

static int
sixel2png_option_allows_leading_dash(int short_opt)
{
    if (short_opt == 'o' || short_opt == 'i') {
        return 1;
    }

    return 0;
}

static int
sixel2png_token_is_known_option(char const *token, int *out_short_opt)
{
    sixel2png_option_help_t const *entry;
    char const *long_name_start;
    size_t length;
    char long_name[64];

    entry = NULL;
    long_name_start = NULL;
    length = 0u;

    if (out_short_opt != NULL) {
        *out_short_opt = 0;
    }

    if (token == NULL) {
        return 0;
    }

    if (token[0] != '-') {
        return 0;
    }

    if (token[1] == '\0') {
        return 0;
    }

    if (token[1] == '-') {
        long_name_start = token + 2;
        length = 0u;
        while (long_name_start[length] != '\0'
                && long_name_start[length] != '=') {
            length += 1u;
        }
        if (length == 0u) {
            return 0;
        }
        if (length >= sizeof(long_name)) {
            return 0;
        }
        memcpy(long_name, long_name_start, length);
        long_name[length] = '\0';
        entry = sixel2png_find_option_help_by_long_name(long_name);
    } else {
        entry = sixel2png_find_option_help((unsigned char)token[1]);
    }

    if (entry == NULL) {
        return 0;
    }

    if (out_short_opt != NULL) {
        *out_short_opt = entry->short_opt;
    }

    return 1;
}

static void
sixel2png_print_option_help(FILE *stream)
{
    size_t index;
    size_t count;

    if (stream == NULL) {
        return;
    }

    index = 0u;
    count = sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
    while (index < count) {
        if (g_option_help_table[index].help != NULL) {
            fputs(g_option_help_table[index].help, stream);
        }
        ++index;
    }
}

static void
sixel2png_print_clipboard_hint(void)
{
    fprintf(stderr,
            "The pseudo file \"clipboard:\" mirrors the desktop clipboard.\n"
            "Supported forms include \"clipboard:\", \"png:clipboard:\", and\n"
            "\"tiff:clipboard:\".\n");
}

static void
sixel2png_report_invalid_argument(int short_opt,
                                  char const *value,
                                  char const *detail)
{
    char buffer[1024];
    char detail_copy[1024];
    sixel2png_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    entry = sixel2png_find_option_help(short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    argument = (value != NULL && value[0] != '\0')
        ? value : "(missing)";
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "'%s' is invalid argument for -%c,--%s option:\n\n",
                       argument,
                       (char)short_opt,
                       long_opt);
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= sizeof(buffer)) {
        offset = sizeof(buffer) - 1u;
    } else {
        offset = (size_t)written;
    }

    if (detail != NULL && detail[0] != '\0' && offset < sizeof(buffer) - 1u) {
        (void) snprintf(detail_copy,
                        sizeof(detail_copy),
                        "%s\n",
                        detail);
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           detail_copy);
        if (written < 0) {
            written = 0;
        }
        if ((size_t)written >= sizeof(buffer) - offset) {
            offset = sizeof(buffer) - 1u;
        } else {
            offset += (size_t)written;
        }
    }

    if (offset < sizeof(buffer) - 1u) {
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           help_text);
        if (written < 0) {
            written = 0;
        }
    }

    sixel_helper_set_additional_message(buffer);
}

static void
sixel2png_report_missing_argument(int short_opt)
{
    char buffer[1024];
    sixel2png_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    entry = sixel2png_find_option_help(short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "sixel2png: missing required argument for "
                       "-%c,--%s option.\n\n",
                       (char)short_opt,
                       long_opt);
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= sizeof(buffer)) {
        offset = sizeof(buffer) - 1u;
    } else {
        offset = (size_t)written;
    }

    if (offset < sizeof(buffer) - 1u) {
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           help_text);
        if (written < 0) {
            written = 0;
        }
    }

    sixel_helper_set_additional_message(buffer);
}

static void
sixel2png_report_unrecognized_option(int short_opt, char const *token)
{
    char buffer[1024];
    char const *view;
    int written;

    memset(buffer, 0, sizeof(buffer));
    view = NULL;
    if (token != NULL && token[0] != '\0') {
        view = token;
    }

    if (view != NULL) {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "sixel2png: unrecognized option '%s'.\n",
                           view);
    } else if (short_opt > 0 && short_opt != '?') {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "sixel2png: unrecognized option '-%c'.\n",
                           (char)short_opt);
    } else {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "sixel2png: unrecognized option.\n");
    }
    if (written < 0) {
        written = 0;
    }

    sixel_helper_set_additional_message(buffer);
}

static int
sixel2png_guard_missing_argument(int short_opt, char *const *argv)
{
    int recognised;
    int candidate_short_opt;

    recognised = 0;
    candidate_short_opt = 0;

    if (sixel2png_option_requires_argument(short_opt) == 0) {
        return 0;
    }

    if (optarg == NULL) {
        sixel2png_report_missing_argument(short_opt);
        return -1;
    }

    if (sixel2png_option_allows_leading_dash(short_opt) != 0) {
        return 0;
    }

    recognised = sixel2png_token_is_known_option(optarg,
                                                 &candidate_short_opt);
    if (recognised != 0) {
        if (optind > 0 && optarg == argv[optind - 1]) {
            optind -= 1;
            sixel2png_report_missing_argument(short_opt);
            return -1;
        }
    }

    return 0;
}

static void
sixel2png_handle_getopt_error(int short_opt, char const *token)
{
    sixel2png_option_help_t const *entry;
    sixel2png_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = sixel2png_find_option_help(short_opt);
        if (entry != NULL) {
            sixel2png_report_missing_argument(short_opt);
            return;
        }
    }

    if (token != NULL && token[0] != '\0') {
        if (strncmp(token, "--", 2) == 0) {
            long_name = token + 2;
        } else if (token[0] == '-') {
            long_name = token + 1;
        }
        if (long_name != NULL && long_name[0] != '\0') {
            long_entry =
                sixel2png_find_option_help_by_long_name(long_name);
            if (long_entry != NULL) {
                sixel2png_report_missing_argument(
                    long_entry->short_opt);
                return;
            }
        }
    }

    sixel2png_report_unrecognized_option(short_opt, token);
}

static int
sixel2png_case_insensitive_equals(char const *lhs, char const *rhs)
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

static char const *
sixel2png_basename_view(char const *path)
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
    if (backward != NULL
            && (forward == NULL || backward > forward)) {
        forward = backward;
    }
#endif

    if (forward != NULL) {
        return forward + 1;
    }

    return start;
}

static char const *
sixel2png_extension_view(char const *name)
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
sixel2png_normalized_levenshtein(char const *lhs, char const *rhs)
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
    double distance_double;
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
    distance_double = 0.0;
    normalized = 0.0;

    if (lhs == NULL || rhs == NULL) {
        return 0.0;
    }

    lhs_length = strlen(lhs);
    rhs_length = strlen(rhs);
    if (lhs_length == 0u && rhs_length == 0u) {
        return 1.0;
    }

    previous = (size_t *)malloc((rhs_length + 1u) * sizeof(size_t));
    current = (size_t *)malloc((rhs_length + 1u) * sizeof(size_t));
    if (previous == NULL || current == NULL) {
        free(previous);
        free(current);
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
            cost = (lhs[row - 1u] == rhs[column - 1u]) ? 0u : 1u;
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

    distance_double = (double)previous[rhs_length];
    free(current);
    free(previous);

    normalized = 1.0 - distance_double /
        (double)((lhs_length > rhs_length) ? lhs_length : rhs_length);
    if (normalized < 0.0) {
        normalized = 0.0;
    }

    return normalized;
}

#define SIXEL2PNG_SUGGESTION_LIMIT 5u
#define SIXEL2PNG_SUGGESTION_NAME_WEIGHT 0.55
#define SIXEL2PNG_SUGGESTION_EXTENSION_WEIGHT 0.25
#define SIXEL2PNG_SUGGESTION_RECENCY_WEIGHT 0.20

typedef struct sixel2png_path_candidate {
    char *path;
    char const *name;
    time_t mtime;
    double name_score;
    double extension_score;
    double recency_score;
    double total_score;
} sixel2png_path_candidate_t;

static double
sixel2png_extension_similarity(char const *lhs, char const *rhs)
{
    char const *lhs_extension;
    char const *rhs_extension;

    lhs_extension = sixel2png_extension_view(lhs);
    rhs_extension = sixel2png_extension_view(rhs);
    if (lhs_extension == NULL || rhs_extension == NULL) {
        return 0.0;
    }
    if (sixel2png_case_insensitive_equals(lhs_extension, rhs_extension)) {
        return 1.0;
    }

    return 0.0;
}

static char *
sixel2png_duplicate_string(char const *text)
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
sixel2png_duplicate_directory(char const *path)
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
        return sixel2png_duplicate_string(".");
    }

    forward = strrchr(path, '/');
#if defined(_WIN32)
    backward = strrchr(path, '\\');
    if (backward != NULL
            && (forward == NULL || backward > forward)) {
        forward = backward;
    }
#endif

    separator = forward;
    if (separator == NULL) {
        return sixel2png_duplicate_string(".");
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

static char *
sixel2png_join_directory_entry(char const *directory, char const *entry)
{
    size_t directory_length;
    size_t entry_length;
    size_t need_separator;
    char *joined;

    directory_length = 0u;
    entry_length = 0u;
    need_separator = 0u;
    joined = NULL;

    if (directory == NULL || entry == NULL) {
        return NULL;
    }

    directory_length = strlen(directory);
    entry_length = strlen(entry);
    if (directory_length == 0u) {
        return sixel2png_duplicate_string(entry);
    }

    if (directory[directory_length - 1u] == '/'
#if defined(_WIN32)
            || directory[directory_length - 1u] == '\\'
#endif
            ) {
        need_separator = 0u;
    } else {
        need_separator = 1u;
    }

    joined = (char *)malloc(directory_length + need_separator +
                            entry_length + 1u);
    if (joined == NULL) {
        return NULL;
    }

    memcpy(joined, directory, directory_length);
    if (need_separator != 0u) {
        joined[directory_length] = '/';
    }
    memcpy(joined + directory_length + need_separator,
           entry,
           entry_length);
    joined[directory_length + need_separator + entry_length] = '\0';

    return joined;
}

static void
sixel2png_format_timestamp(time_t value,
                           char *buffer,
                           size_t buffer_size)
{
    struct tm *time_pointer;
#if defined(HAVE_LOCALTIME_R)
    struct tm time_view;
#endif

    if (buffer == NULL || buffer_size == 0u) {
        return;
    }

#if defined(HAVE_LOCALTIME_R)
    if (localtime_r(&value, &time_view) != NULL) {
        (void) strftime(buffer, buffer_size, "%Y-%m-%d %H:%M",
                        &time_view);
        return;
    }
#endif
    time_pointer = localtime(&value);
    if (time_pointer != NULL) {
        (void) strftime(buffer, buffer_size, "%Y-%m-%d %H:%M",
                        time_pointer);
        return;
    }

    (void) snprintf(buffer, buffer_size, "unknown");
}

static int
sixel2png_candidate_compare(void const *lhs, void const *rhs)
{
    sixel2png_path_candidate_t const *left;
    sixel2png_path_candidate_t const *right;

    left = (sixel2png_path_candidate_t const *)lhs;
    right = (sixel2png_path_candidate_t const *)rhs;

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

static int
sixel2png_build_missing_file_message(char const *option_label,
                                     char const *argument,
                                     char const *resolved_path,
                                     char *buffer,
                                     size_t buffer_size)
{
    char *directory_copy;
    char const *argument_view;
    char const *target_name;
    size_t offset;
    int written;
    int result;
#if HAVE_DIRENT_H
    DIR *directory_stream;
    struct dirent *entry;
    sixel2png_path_candidate_t *candidates;
    sixel2png_path_candidate_t *grown;
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
    (void)option_label;
    (void)argument;
    (void)resolved_path;
    (void)buffer;
    (void)buffer_size;
#endif

#if !HAVE_DIRENT_H
    directory_copy = NULL;
    argument_view = NULL;
    target_name = NULL;
    offset = 0u;
    written = 0;
    result = 0;
#endif

#if !HAVE_DIRENT_H
    (void)directory_copy;
    (void)argument_view;
    (void)target_name;
    (void)offset;
    (void)written;
    if (buffer != NULL && buffer_size > 0u) {
        written = snprintf(buffer,
                           buffer_size,
                           "sixel2png: %s \"%s\" not found.\n"
                           "Suggestion lookup unavailable on this build.\n",
                           option_label != NULL ? option_label : "path",
                           argument != NULL ? argument : "");
        if (written < 0) {
            written = 0;
        }
    }
    return 0;
#else
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
    error_code = 0;
    memset(error_buffer, 0, sizeof(error_buffer));

    directory_copy = sixel2png_duplicate_directory(resolved_path);
    if (directory_copy == NULL) {
        return -1;
    }
    argument_view = (argument != NULL && argument[0] != '\0')
        ? argument : resolved_path;
    target_name = sixel2png_basename_view(resolved_path);
    offset = 0u;
    written = 0;
    result = 0;

    if (buffer == NULL || buffer_size == 0u) {
        free(directory_copy);
        return -1;
    }

    written = snprintf(buffer,
                       buffer_size,
                       "sixel2png: %s \"%s\" not found.\n",
                       option_label != NULL ? option_label : "path",
                       argument_view != NULL ? argument_view : "");
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= buffer_size) {
        offset = buffer_size - 1u;
    } else {
        offset = (size_t)written;
    }

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
            if (cli_compat_strerror(error_code,
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
        return 0;
    }

    while ((entry = readdir(directory_stream)) != NULL) {
        if (entry->d_name[0] == '.' &&
                (entry->d_name[1] == '\0' ||
                 (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        candidate_path = sixel2png_join_directory_entry(directory_copy,
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
            grown = (sixel2png_path_candidate_t *)realloc(
                candidates,
                new_capacity * sizeof(sixel2png_path_candidate_t));
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
            sixel2png_basename_view(candidate_path);
        candidates[candidate_count].mtime = entry_stat.st_mtime;
        candidates[candidate_count].name_score = 0.0;
        candidates[candidate_count].extension_score = 0.0;
        candidates[candidate_count].recency_score = 0.0;
        candidates[candidate_count].total_score = 0.0;
        ++candidate_count;
        candidate_path = NULL;
    }

    if (directory_stream != NULL) {
        (void) closedir(directory_stream);
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
            sixel2png_normalized_levenshtein(target_name,
                                             candidates[index].name);
        candidates[index].extension_score =
            sixel2png_extension_similarity(target_name,
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
        /*
         *            name (55%)
         *             /   \
         *        ext       recency
         *       (25%)       (20%)
         *
         * The triad above sketches how fuzzy matchers weigh the inputs.
         * Textual similarity dominates while the extension and recency
         * scores act as smaller tie breakers so recently modified files
         * with matching suffixes bubble toward the top.
         */
        candidates[index].total_score =
            SIXEL2PNG_SUGGESTION_NAME_WEIGHT *
                candidates[index].name_score +
            SIXEL2PNG_SUGGESTION_EXTENSION_WEIGHT *
                candidates[index].extension_score +
            SIXEL2PNG_SUGGESTION_RECENCY_WEIGHT *
                candidates[index].recency_score;
    }

    qsort(candidates,
          candidate_count,
          sizeof(sixel2png_path_candidate_t),
          sixel2png_candidate_compare);

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

    for (index = 0u; index < candidate_count
            && index < SIXEL2PNG_SUGGESTION_LIMIT; ++index) {
        percent_double = candidates[index].total_score * 100.0;
        if (percent_double < 0.0) {
            percent_double = 0.0;
        }
        if (percent_double > 100.0) {
            percent_double = 100.0;
        }
        percent_int = (int)(percent_double + 0.5);
        sixel2png_format_timestamp(candidates[index].mtime,
                                   time_buffer,
                                   sizeof(time_buffer));
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "  * %s (similarity score %d%%,"
                               " modified %s)\n",
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
        (void) closedir(directory_stream);
    }
    if (candidates != NULL) {
        for (index = 0u; index < candidate_count; ++index) {
            free(candidates[index].path);
            candidates[index].path = NULL;
        }
        free(candidates);
    }
    free(directory_copy);
    return result;
#endif
}

static void
sixel2png_normalise_windows_drive_path(char *path)
{
#if defined(_WIN32)
    size_t length;

    length = 0u;

    if (path == NULL) {
        return;
    }

    length = strlen(path);
    if (length >= 3u
            && path[0] == '/'
            && ((path[1] >= 'A' && path[1] <= 'Z')
                || (path[1] >= 'a' && path[1] <= 'z'))
            && path[2] == '/') {
        path[0] = path[1];
        path[1] = ':';
    }
#else
    (void)path;
#endif
}

static int
sixel2png_path_looks_remote(char const *path)
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
        if (!(isalpha(value) || value == '+' || value == '-'
                || value == '.')) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int
sixel2png_spec_is_clipboard(char const *argument)
{
    char const *marker;

    /*
     * Keep the decoder aligned with the clipboard grammar accepted by the
     * shared clipboard helpers.  Both "clipboard:" and "format:clipboard:" are
     * valid pseudo paths that should bypass filesystem validation.
     */
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
sixel2png_validate_input_argument(char const *argument)
{
    struct stat path_stat;
    int stat_result;
    int error_value;
    char message_buffer[1024];

    memset(&path_stat, 0, sizeof(path_stat));
    stat_result = 0;
    error_value = 0;
    memset(message_buffer, 0, sizeof(message_buffer));

    if (argument == NULL || argument[0] == '\0') {
        sixel_helper_set_additional_message(
            "sixel2png: input path is empty.");
        return -1;
    }
    if (strcmp(argument, "-") == 0) {
        return 0;
    }
    if (sixel2png_spec_is_clipboard(argument)) {
        return 0;
    }
    if (sixel2png_path_looks_remote(argument)) {
        return 0;
    }

    errno = 0;
    stat_result = stat(argument, &path_stat);
    if (stat_result == 0) {
        return 0;
    }

    error_value = errno;
    if (error_value != ENOENT && error_value != ENOTDIR) {
        return 0;
    }

    if (sixel2png_build_missing_file_message("input file",
                                             argument,
                                             argument,
                                             message_buffer,
                                             sizeof(message_buffer)) != 0) {
        sixel_helper_set_additional_message(
            "sixel2png: input path validation failed.");
    } else {
        sixel_helper_set_additional_message(message_buffer);
    }

    return -1;
}

static SIXELSTATUS
sixel2png_decoder_setopt(sixel_decoder_t *decoder,
                         int option,
                         char const *argument)
{
    SIXELSTATUS status;
    char detail_buffer[1024];
    char const *detail_source;
    char const *effective_argument;
    char *allocated_argument;
    int validation_result;

    detail_source = NULL;
    effective_argument = argument;
    allocated_argument = NULL;
    validation_result = 0;

    if (option == 'i') {
        validation_result = sixel2png_validate_input_argument(argument);
        if (validation_result != 0) {
            return SIXEL_BAD_ARGUMENT;
        }
    }

    if (option == 'o' && argument != NULL) {
        char const *payload;
        size_t length;

        payload = argument;
        if (strncmp(argument, "png:", 4) == 0) {
            payload = argument + 4;
            if (payload[0] == '\0') {
                sixel_helper_set_additional_message(
                    "sixel2png: missing target after the \"png:\" prefix.");
                return SIXEL_BAD_ARGUMENT;
            }
        }
        if (payload != argument) {
            length = strlen(payload);
            allocated_argument = (char *)malloc(length + 1u);
            if (allocated_argument == NULL) {
                sixel_helper_set_additional_message(
                    "sixel2png: malloc() failed for output path.");
                return SIXEL_BAD_ALLOCATION;
            }
            memcpy(allocated_argument, payload, length + 1u);
            sixel2png_normalise_windows_drive_path(allocated_argument);
            effective_argument = allocated_argument;
        } else {
            effective_argument = argument;
        }
    }

    status = sixel_decoder_setopt(decoder, option, effective_argument);
    if (SIXEL_FAILED(status)) {
        detail_buffer[0] = '\0';
        detail_source = sixel_helper_get_additional_message();
        if (detail_source != NULL && detail_source[0] != '\0') {
            (void) snprintf(detail_buffer,
                            sizeof(detail_buffer),
                            "%s",
                            detail_source);
        }
        if (status == SIXEL_BAD_ARGUMENT) {
            sixel2png_report_invalid_argument(
                option,
                effective_argument,
                detail_buffer[0] != '\0' ? detail_buffer : NULL);
        }
    }

    if (allocated_argument != NULL) {
        free(allocated_argument);
        allocated_argument = NULL;
    }

    return status;
}

/* output version info to STDOUT */
static
void show_version(void)
{
    printf("sixel2png " PACKAGE_VERSION "\n"
           "\n"
           "configured with:\n"
           "  libpng: "
#ifdef HAVE_LIBPNG
           "yes\n"
#else
           "no\n"
#endif
           "\n"
           "Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.\n"
           "Copyright (C) 2014-2017 Hayaki Saito <saitoha@me.com>.\n"
           "\n"
           "Permission is hereby granted, free of charge, to any person "
           "obtaining a copy of\n"
           "this software and associated documentation files "
           "(the \"Software\"), to deal in\n"
           "the Software without restriction, including without limitation "
           "the rights to\n"
           "use, copy, modify, merge, publish, distribute, sublicense, and/or "
           "sell copies of\n"
           "the Software, and to permit persons to whom the Software is "
           "furnished to do so,\n"
           "subject to the following conditions:\n"
           "\n"
           "The above copyright notice and this permission notice shall be "
           "included in all\n"
           "copies or substantial portions of the Software.\n"
           "\n"
           "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, "
           "EXPRESS OR\n"
           "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
           "MERCHANTABILITY," " FITNESS\n"
           "FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE"
           " AUTHORS OR\n"
           "COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
           "LIABILITY, WHETHER\n"
           "IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF"
           " OR IN\n"
           "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
           "SOFTWARE.\n"
          );
}


static void
show_help(void)
{
    fprintf(stderr,
            "Usage: sixel2png -i input.sixel -o output.png\n"
            "       sixel2png < input.sixel > output.png\n"
            "\n"
            "Options:\n");
    sixel2png_print_option_help(stderr);
}


int
main(int argc, char *argv[])
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    sixel_decoder_t *decoder;
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    char const *optstring;

#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"input",            required_argument,  &long_opt, 'i'},
        {"output",           required_argument,  &long_opt, 'o'},
        {"dequantize",       required_argument,  &long_opt, 'd'},
        {"similarity",       required_argument,  &long_opt, 'S'},
        {"size",             required_argument,  &long_opt, 's'},
        {"edge",             required_argument,  &long_opt, 'e'},
        {"direct",           no_argument,        &long_opt, 'D'},
        {"version",          no_argument,        &long_opt, 'V'},
        {"help",             no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */

    optstring = g_sixel2png_optstring;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    for (;;) {

#if HAVE_GETOPT_LONG
        n = getopt_long(argc, argv, optstring,
                        long_options, &option_index);
#else
        n = getopt(argc, argv, optstring);
#endif  /* HAVE_GETOPT_LONG */

        if (n == (-1)) {
            /* parsed successfully */
            break;
        }
#if HAVE_GETOPT_LONG
        if (n == 0) {
            n = long_opt;
        }
#endif  /* HAVE_GETOPT_LONG */

        if (n > 0) {
            if (sixel2png_guard_missing_argument(n, argv) != 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
        }

        switch (n) {
        case 'V':
            show_version();
            status = SIXEL_OK;
            goto end;
        case 'H':
            show_help();
            status = SIXEL_OK;
            goto end;
        case '?':
            sixel2png_handle_getopt_error(
                optopt,
                (optind > 0 && optind <= argc)
                    ? argv[optind - 1]
                    : NULL);
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        default:
            status = sixel2png_decoder_setopt(decoder, n, optarg);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }

        if (optind >= argc) {
            break;
        }

    }

    if (optind < argc) {
        char const *argument;

        argument = argv[optind];
        if (sixel2png_validate_input_argument(argument) != 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        }
        status = sixel2png_decoder_setopt(decoder, 'i', argument);
        ++optind;
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (optind < argc) {
        char const *argument;

        argument = argv[optind];
        status = sixel2png_decoder_setopt(decoder, 'o', argument);
        ++optind;
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (optind != argc) {
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }

    status = sixel_decoder_decode(decoder);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    goto end;

error:
    fprintf(stderr, "%s\n%s\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    if (status == SIXEL_BAD_CLIPBOARD) {
        sixel2png_print_clipboard_hint();
        fprintf(stderr, "\n");
    }
    status = (-1);

end:
    sixel_decoder_unref(decoder);
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
