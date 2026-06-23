/*
 * SPDX-License-Identifier: MIT
 *
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "malloc_stub.h"
#include "getopt_stub.h"
#include "aborttrace.h"

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
#include "cli.h"

/* for msvc */
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

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
static cli_option_help_t const g_option_help_table[] = {
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
        "                             none              -> disable\n"
        "                             k_undither        -> Kornelski's\n"
        "                                                  undither\n"
        "                             lso_undither:Vfs\n"
        "                             l:Vf              -> full FS undither\n"
        "                             lso_undither:Vlight\n"
        "                             l:Vl              -> light causal\n"
        "                                                  undither\n"
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
        '=',
        "threads",
        "-= COUNT, --threads=COUNT|auto\n"
        "                           override decoder worker\n"
        "                           count. COUNT>=1 keeps\n"
        "                           deterministic ordering\n"
        "                           while larger values enable\n"
        "                           band parallelism. Use\n"
        "                           'auto' to match the\n"
        "                           hardware thread count.\n"
    },
    {
        '%',
        "env",
        "-% KEY=VALUE, --env=KEY=VALUE\n"
        "                           set process environment variable\n"
        "                           before decoding. Repeatable.\n"
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

static size_t
sixel2png_option_help_count(void)
{
    return sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
}

static char const g_sixel2png_optstring[] = "i:o:d:S:e:s:=:%:DVH";

typedef struct sixel2png_parsed_option {
    int code;
    int optopt_value;
    char const *argument;
    char const *token;
} sixel2png_parsed_option_t;

static int
sixel2png_option_allows_leading_dash(int short_opt)
{
    if (short_opt == 'o' || short_opt == 'i') {
        return 1;
    }

    return 0;
}

static void sixel2png_report_missing_argument(int short_opt);

static int
sixel2png_allows_leading_dash_cb(int short_opt, void *user_data)
{
    (void)user_data;

    return sixel2png_option_allows_leading_dash(short_opt);
}

static void
sixel2png_report_missing_argument_callback(int short_opt, void *user_data)
{
    (void)user_data;

    sixel2png_report_missing_argument(short_opt);
}

static void
sixel2png_print_option_help(FILE *stream)
{
    cli_print_option_help(stream,
                          g_option_help_table,
                          sixel2png_option_help_count());
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
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    entry = cli_find_option_help(g_option_help_table,
                                 sixel2png_option_help_count(),
                                 short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    argument = (value != NULL && value[0] != '\0')
        ? value : "(missing)";
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "\\fW'%s'\\fP is invalid argument for "
                       "\\fB-%c\\fP,\\fB--%s\\fP option:\n\n",
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
    cli_report_missing_argument("sixel2png",
                                g_option_help_fallback,
                                g_option_help_table,
                                sixel2png_option_help_count(),
                                short_opt);
}

static void
sixel2png_report_unrecognized_option(int short_opt, char const *token)
{
    cli_report_unrecognized_option("sixel2png", short_opt, token);
}

static int
sixel2png_guard_missing_argument(int short_opt, char *const *argv)
{
    return cli_guard_missing_argument(short_opt,
                                      argv,
                                      optarg,
                                      &optind,
                                      g_sixel2png_optstring,
                                      g_option_help_table,
                                      sixel2png_option_help_count(),
                                      sixel2png_allows_leading_dash_cb,
                                      NULL,
                                      sixel2png_report_missing_argument_callback,
                                      NULL);
}

static void
sixel2png_handle_getopt_error(int short_opt, char const *token)
{
    cli_option_help_t const *entry;
    cli_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = cli_find_option_help(g_option_help_table,
                                     sixel2png_option_help_count(),
                                     short_opt);
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
            long_entry = cli_find_option_help_by_long_name(
                g_option_help_table,
                sixel2png_option_help_count(),
                long_name);
            if (long_entry != NULL) {
                sixel2png_report_missing_argument(
                    long_entry->short_opt);
                return;
            }
        }
    }

    sixel2png_report_unrecognized_option(short_opt, token);
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
          );
}


static void
show_help(void)
{
    /*
     * Help text must go to stdout so shell pipelines like `sixel2png -H
     * | less` work without redirection. Keep error details on stderr in
     * other code paths.
     */
    fprintf(stdout,
            "Usage: sixel2png -i input.sixel -o output.png\n"
            "       sixel2png < input.sixel > output.png\n"
            "\n"
            "Options:\n");
    sixel2png_print_option_help(stdout);
}


int
main(int argc, char *argv[])
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    sixel_decoder_t *decoder;
#if HAVE_GETOPT_LONG
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    char const *optstring;
    char detail_buffer[1024];
    char const *detail_source = NULL;
    int detail_limit;
    sixel2png_parsed_option_t *parsed_options;
    sixel2png_parsed_option_t *grown_options;
    sixel2png_parsed_option_t current_option;
    size_t parsed_count;
    size_t parsed_capacity;
    size_t parsed_index;
    int parse_unknown_option;
    int parse_terminal_optind;

    decoder = NULL;
    parsed_options = NULL;
    grown_options = NULL;
    parsed_count = 0u;
    parsed_capacity = 0u;
    parsed_index = 0u;
    parse_unknown_option = 0;
    parse_terminal_optind = 1;
    current_option.code = 0;
    current_option.optopt_value = 0;
    current_option.argument = NULL;
    current_option.token = NULL;

    sixel_tty_init_output_device(STDERR_FILENO);
    sixel_aborttrace_install_if_unhandled();

#if HAVE_GETOPT_LONG
    /*
     * Keep getopt_long() in "return val directly" mode by passing NULL
     * as the flag field. This avoids reading an intermediate flag value
     * and keeps MSan from reporting a false uninitialized read in main.
     */
    struct option long_options[] = {
        {"input",            required_argument,  NULL, 'i'},
        {"output",           required_argument,  NULL, 'o'},
        {"dequantize",       required_argument,  NULL, 'd'},
        {"similarity",       required_argument,  NULL, 'S'},
        {"size",             required_argument,  NULL, 's'},
        {"edge",             required_argument,  NULL, 'e'},
        {"direct",           no_argument,        NULL, 'D'},
        {"threads",          required_argument,  NULL, '='},
        {"env",              required_argument,  NULL, '%'},
        {"version",          no_argument,        NULL, 'V'},
        {"help",             no_argument,        NULL, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */

    optstring = g_sixel2png_optstring;

    optind = 1;
    for (;;) {

#if HAVE_GETOPT_LONG
        n = getopt_long(argc, argv, optstring,
                        long_options, &option_index);
#else
        n = getopt(argc, argv, optstring);
#endif  /* HAVE_GETOPT_LONG */

        if (n == (-1)) {
            break;
        }
        if (n > 0) {
            if (sixel2png_guard_missing_argument(n, argv) != 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
        }

        current_option.code = n;
        current_option.optopt_value = optopt;
        current_option.argument = optarg;
        current_option.token = (optind > 0 && optind <= argc)
            ? argv[optind - 1]
            : NULL;

        if (parsed_count == parsed_capacity) {
            size_t new_capacity;

            new_capacity = parsed_capacity == 0u
                ? 16u
                : parsed_capacity * 2u;
            grown_options = (sixel2png_parsed_option_t *)realloc(
                parsed_options,
                new_capacity * sizeof(*parsed_options));
            if (grown_options == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto error;
            }
            parsed_options = grown_options;
            parsed_capacity = new_capacity;
        }

        parsed_options[parsed_count] = current_option;
        ++parsed_count;

        if (n == '?') {
            parse_unknown_option = 1;
            break;
        }
    }
    parse_terminal_optind = optind;

    for (parsed_index = 0u; parsed_index < parsed_count; ++parsed_index) {
        switch (parsed_options[parsed_index].code) {
        case '%':
            if (cli_apply_env_assignment(parsed_options[parsed_index].argument,
                                         detail_buffer,
                                         sizeof(detail_buffer)) != 0) {
                sixel_helper_set_additional_message(detail_buffer);
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
            break;
        case 'V':
            show_version();
            status = SIXEL_OK;
            goto end;
        case 'H':
            show_help();
            status = SIXEL_OK;
            goto end;
        default:
            break;
        }
    }

    if (parse_unknown_option != 0) {
        sixel2png_handle_getopt_error(
            parsed_options[parsed_count - 1u].optopt_value,
            parsed_options[parsed_count - 1u].token);
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_option_apply_cli_suggestion_defaults();

    for (parsed_index = 0u; parsed_index < parsed_count; ++parsed_index) {
        n = parsed_options[parsed_index].code;
        optarg = (char *)parsed_options[parsed_index].argument;
        switch (n) {
        case 'V':
        case 'H':
        case '%':
            break;
        case '?':
            sixel2png_handle_getopt_error(
                parsed_options[parsed_index].optopt_value,
                parsed_options[parsed_index].token);
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        default:
            status = sixel_decoder_setopt(decoder, n, optarg);
            if (SIXEL_FAILED(status)) {
                detail_buffer[0] = '\0';
                detail_source = sixel_helper_get_additional_message();
                if (detail_source != NULL && detail_source[0] != '\0') {
                    detail_limit = (int)(sizeof(detail_buffer) - 1);
                    (void) snprintf(detail_buffer,
                                    sizeof(detail_buffer),
                                    "%.*s",
                                    detail_limit,
                                    detail_source);
                }
                if (status == SIXEL_BAD_ARGUMENT) {
                    sixel2png_report_invalid_argument(
                        n,
                        optarg,
                        detail_buffer[0] != '\0' ? detail_buffer : NULL);
                }
                goto error;
            }
        }
    }
    optind = parse_terminal_optind;

    if (optind < argc) {
        char const *argument;

        argument = argv[optind];
        status = sixel_decoder_setopt(decoder, 'i', argument);
        ++optind;
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (optind < argc) {
        char const *argument;

        argument = argv[optind];
        status = sixel_decoder_setopt(decoder, 'o', argument);
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
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    if (parsed_options != NULL) {
        free(parsed_options);
        parsed_options = NULL;
    }
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
