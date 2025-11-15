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

    detail_source = NULL;
    effective_argument = argument;
    allocated_argument = NULL;

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
