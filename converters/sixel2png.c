/*
 * Copyright (c) 2014,2015 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* strcpy */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
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


static
void show_version(void)
{
    printf("sixel2png " PACKAGE_VERSION "\n"
           "Copyright (C) 2014,2015 Hayaki Saito <user@zuse.jp>.\n"
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
            "Options:\n"
            "-i, --input     specify input file\n"
            "-o, --output    specify output file\n"
            "-V, --version   show version and license information\n"
            "-H, --help      show this help\n"
           );
}


int
main(int argc, char *argv[])
{
    int n;
    sixel_decode_settings_t *settings;
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    int nret = 0;
    char const *optstring = "i:o:VH";

#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"input",        required_argument,  &long_opt, 'i'},
        {"output",       required_argument,  &long_opt, 'o'},
        {"version",      no_argument,        &long_opt, 'V'},
        {"help",         no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };

    settings = sixel_decode_settings_create();
    if (settings == NULL) {
        nret = (-1);
        goto end;
    }

#endif  /* HAVE_GETOPT_LONG */
    for (;;) {
#if HAVE_GETOPT_LONG
        n = getopt_long(argc, argv, optstring,
                        long_options, &option_index);
#else
        n = getopt(argc, argv, optstring);
#endif  /* HAVE_GETOPT_LONG */

        if (n == -1) {
            nret = (-1);
            break;
        }
#if HAVE_GETOPT_LONG
        if (n == 0) {
            n = long_opt;
        }
#endif  /* HAVE_GETOPT_LONG */

        nret = sixel_easy_decode_setopt(settings, n, optarg);
        if (nret != 0) {
            goto argerr;
        }

        if (optind >= argc) {
            break;
        }

    }

    if (sixel_decode_settings_has_version(settings)) {
        show_version();
        goto end;
    }

    if (sixel_decode_settings_has_help(settings)) {
        show_help();
        goto end;
    }

    if (optind < argc) {
        nret = sixel_easy_decode_setopt(settings, 'i', argv[optind++]);
        if (nret != 0) {
            goto argerr;
        }
    }
    if (optind < argc) {
        nret = sixel_easy_decode_setopt(settings, 'o', argv[optind++]);
        if (nret != 0) {
            goto argerr;
        }
    }
    if (optind != argc) {
        nret = (-1);
        goto argerr;
    }

    nret = sixel_easy_decode(settings);
    goto end;

argerr:
    show_help();

end:
    sixel_decode_settings_unref(settings);
    return nret;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
