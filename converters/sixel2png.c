/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2017 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include "malloc_stub.h"
#include "getopt_stub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* memset */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
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
        "-i FILE, --input=FILE       specify input file\n"
        "                           use '-' to read from stdin.\n"
    },
    {
        'o',
        "output",
        "-o FILE, --output=FILE      specify output file\n"
        "                           use '-' to write to stdout.\n"
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
        "-S BIAS, --similarity=BIAS  specify similarity bias\n"
        "                           range: 0-1000 (default: 100).\n"
    },
    {
        's',
        "size",
        "-s SIZE, --size=SIZE        scale longer edge to SIZE pixels\n"
        "                           while preserving aspect ratio.\n"
    },
    {
        'e',
        "edge",
        "-e BIAS, --edge=BIAS        specify edge protection bias\n"
        "                           range: 0-1000 (default: 0).\n"
    },
    {
        'V',
        "version",
        "-V, --version               show version and license info.\n"
    },
    {
        'H',
        "help",
        "-H, --help                  show this help.\n"
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"sixel2png -H\" for more details.\n";

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

static SIXELSTATUS
sixel2png_decoder_setopt(sixel_decoder_t *decoder,
                         int option,
                         char const *argument)
{
    SIXELSTATUS status;
    char detail_buffer[1024];
    char const *detail_source;

    status = sixel_decoder_setopt(decoder, option, argument);
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
                argument,
                detail_buffer[0] != '\0' ? detail_buffer : NULL);
        }
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
           "Permission is hereby granted, free of charge, to any person obtaining a copy of\n"
           "this software and associated documentation files (the \"Software\"), to deal in\n"
           "the Software without restriction, including without limitation the rights to\n"
           "use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of\n"
           "the Software, and to permit persons to whom the Software is furnished to do so,\n"
           "subject to the following conditions:\n"
           "\n"
           "The above copyright notice and this permission notice shall be included in all\n"
           "copies or substantial portions of the Software.\n"
           "\n"
           "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
           "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS\n"
           "FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR\n"
           "COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER\n"
           "IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"
           "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
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
    char const *optstring = "i:o:d:S:e:s:VH";

#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"input",            required_argument,  &long_opt, 'i'},
        {"output",           required_argument,  &long_opt, 'o'},
        {"dequantize",       required_argument,  &long_opt, 'd'},
        {"similarity",       required_argument,  &long_opt, 'S'},
        {"size",             required_argument,  &long_opt, 's'},
        {"edge",             required_argument,  &long_opt, 'e'},
        {"version",          no_argument,        &long_opt, 'V'},
        {"help",             no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */

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

        switch (n) {
        case 'V':
            show_version();
            status = SIXEL_OK;
            goto end;
        case 'H':
            show_help();
            status = SIXEL_OK;
            goto end;
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
        status = sixel2png_decoder_setopt(decoder, 'i', argv[optind++]);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (optind < argc) {
        status = sixel2png_decoder_setopt(decoder, 'o', argv[optind++]);
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
