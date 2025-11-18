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

#ifndef LIBSIXEL_CONVERTERS_CLI_H
#define LIBSIXEL_CONVERTERS_CLI_H

#include <stddef.h>
#include <stdio.h>

typedef struct cli_option_help {
    int short_opt;
    char const *long_opt;
    char const *help;
} cli_option_help_t;

typedef int (*cli_allows_leading_dash_proc)(int short_opt, void *user_data);
typedef void (*cli_report_missing_argument_proc)(int short_opt,
                                                 void *user_data);

cli_option_help_t const *
cli_find_option_help(cli_option_help_t const *table,
                     size_t count,
                     int short_opt);

cli_option_help_t const *
cli_find_option_help_by_long_name(cli_option_help_t const *table,
                                  size_t count,
                                  char const *long_name);

int
cli_option_requires_argument(char const *optstring, int short_opt);

int
cli_token_is_known_option(cli_option_help_t const *table,
                          size_t count,
                          char const *token,
                          int *out_short_opt);

int
cli_guard_missing_argument(int short_opt,
                           char *const *argv,
                           char const *optstring,
                           cli_option_help_t const *table,
                           size_t table_count,
                           cli_allows_leading_dash_proc allows_cb,
                           void *allows_user_data,
                           cli_report_missing_argument_proc report_cb,
                           void *report_user_data);

void
cli_print_option_help(FILE *stream,
                      cli_option_help_t const *table,
                      size_t count);

void
cli_report_missing_argument(char const *tool_name,
                            char const *fallback_help,
                            cli_option_help_t const *table,
                            size_t count,
                            int short_opt);

void
cli_report_unrecognized_option(char const *tool_name,
                               int short_opt,
                               char const *token);

#endif  /* LIBSIXEL_CONVERTERS_CLI_H */
