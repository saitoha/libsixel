/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#if HAVE_GETOPT_H
# include <getopt.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif

#include <sixel.h>

#include "getopt_stub.h"
#include "cli.h"

static size_t
cli_safe_count(cli_option_help_t const *table, size_t count)
{
    if (table == NULL) {
        return 0u;
    }

    return count;
}

cli_option_help_t const *
cli_find_option_help(cli_option_help_t const *table,
                     size_t count,
                     int short_opt)
{
    size_t index;
    size_t safe_count;

    safe_count = cli_safe_count(table, count);
    index = 0u;

    while (index < safe_count) {
        if (table[index].short_opt == short_opt) {
            return &table[index];
        }
        index += 1u;
    }

    return NULL;
}

cli_option_help_t const *
cli_find_option_help_by_long_name(cli_option_help_t const *table,
                                  size_t count,
                                  char const *long_name)
{
    size_t index;
    size_t safe_count;

    if (long_name == NULL) {
        return NULL;
    }

    safe_count = cli_safe_count(table, count);
    index = 0u;

    while (index < safe_count) {
        if (table[index].long_opt != NULL
                && strcmp(table[index].long_opt, long_name) == 0) {
            return &table[index];
        }
        index += 1u;
    }

    return NULL;
}

int
cli_option_requires_argument(char const *optstring, int short_opt)
{
    char const *cursor;

    if (optstring == NULL) {
        return 0;
    }

    cursor = optstring;

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

int
cli_token_is_known_option(cli_option_help_t const *table,
                          size_t count,
                          char const *token,
                          int *out_short_opt)
{
    cli_option_help_t const *entry;
    char const *long_name_start;
    char long_name[64];
    size_t length;

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

    entry = NULL;
    long_name_start = NULL;
    length = 0u;

    if (token[1] == '-') {
        long_name_start = token + 2;
        while (long_name_start[length] != '\0'
                && long_name_start[length] != '=') {
            length += 1u;
        }
        if (length == 0u || length >= sizeof(long_name)) {
            return 0;
        }
        memcpy(long_name, long_name_start, length);
        long_name[length] = '\0';
        entry = cli_find_option_help_by_long_name(table, count, long_name);
    } else {
        entry = cli_find_option_help(table, count,
                                     (unsigned char)token[1]);
    }

    if (entry == NULL) {
        return 0;
    }

    if (out_short_opt != NULL) {
        *out_short_opt = entry->short_opt;
    }

    return 1;
}

int
cli_guard_missing_argument(int short_opt,
                           char *const *argv,
                           char const *optstring,
                           cli_option_help_t const *table,
                           size_t table_count,
                           cli_allows_leading_dash_proc allows_cb,
                           void *allows_user_data,
                           cli_report_missing_argument_proc report_cb,
                           void *report_user_data)
{
    int recognised;
    int candidate_short_opt;
    int allows_leading_dash;

    if (cli_option_requires_argument(optstring, short_opt) == 0) {
        return 0;
    }

    if (optarg == NULL) {
        if (report_cb != NULL) {
            report_cb(short_opt, report_user_data);
        }
        return -1;
    }

    allows_leading_dash = 0;
    if (allows_cb != NULL) {
        allows_leading_dash = allows_cb(short_opt, allows_user_data);
    }
    if (allows_leading_dash != 0) {
        return 0;
    }

    recognised = cli_token_is_known_option(table,
                                           table_count,
                                           optarg,
                                           &candidate_short_opt);
    if (recognised != 0) {
        if (optind > 0 && optarg == argv[optind - 1]) {
            /*
             * getopt() stores a pointer to the original argv entry when it
             * treats a standalone token as the argument.  Rewinding optind
             * hands the token back to the parser so the next iteration can
             * interpret it as an option instead of an argument, mirroring the
             * ASCII timeline described in the converter sources.
             */
            optind -= 1;
            if (report_cb != NULL) {
                report_cb(short_opt, report_user_data);
            }
            return -1;
        }
    }

    return 0;
}

void
cli_print_option_help(FILE *stream,
                      cli_option_help_t const *table,
                      size_t count)
{
    size_t index;
    size_t safe_count;

    if (stream == NULL) {
        return;
    }

    safe_count = cli_safe_count(table, count);
    index = 0u;

    while (index < safe_count) {
        if (table[index].help != NULL) {
            fputs(table[index].help, stream);
        }
        index += 1u;
    }
}

void
cli_report_missing_argument(char const *tool_name,
                            char const *fallback_help,
                            cli_option_help_t const *table,
                            size_t count,
                            int short_opt)
{
    char buffer[1024];
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    size_t offset;
    int written;

    if (tool_name == NULL) {
        tool_name = "cli";
    }

    entry = cli_find_option_help(table, count, short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : fallback_help;
    if (help_text == NULL) {
        help_text = "";
    }

    memset(buffer, 0, sizeof(buffer));
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "%s: missing required argument for -%c,--%s option.\n\n",
                       tool_name,
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

void
cli_report_unrecognized_option(char const *tool_name,
                               int short_opt,
                               char const *token)
{
    char buffer[1024];
    char const *view;
    int written;

    if (tool_name == NULL) {
        tool_name = "cli";
    }

    memset(buffer, 0, sizeof(buffer));
    view = NULL;
    if (token != NULL && token[0] != '\0') {
        view = token;
    }

    if (view != NULL) {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "%s: unrecognized option '%s'.\n",
                           tool_name,
                           view);
    } else if (short_opt > 0 && short_opt != '?') {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "%s: unrecognized option '-%c'.\n",
                           tool_name,
                           (char)short_opt);
    } else {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "%s: unrecognized option.\n",
                           tool_name);
    }
    if (written < 0) {
        written = 0;
    }

    sixel_helper_set_additional_message(buffer);
}

/* vim: set et ts=4 sw=4: */
