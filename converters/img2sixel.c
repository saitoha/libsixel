/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2018 Hayaki Saito
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

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif
#if HAVE_GETOPT_H
# include <getopt.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_SIGNAL_H
# include <signal.h>
#elif HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#include <sixel.h>
#include "malloc_stub.h"
#include "getopt_stub.h"
#include "completion_utils.h"

/*
 * Option-specific help snippets drive both the --help output and
 * contextual error reporting.  The layout below mirrors a table:
 *
 *   +-----------+-------------+-----------------------------+
 *   | short opt | long option | contextual help text        |
 *   +-----------+-------------+-----------------------------+
 *
 * When the user supplies an invalid argument we weave the table entry
 * into the diagnostic so they see the relevant manual section without
 * hunting for "img2sixel -H".  The diagram above acts as a quick cheat
 * sheet for the structure we maintain.
 */
typedef struct img2sixel_option_help {
    int short_opt;
    char const *long_opt;
    char const *help;
} img2sixel_option_help_t;

static img2sixel_option_help_t const g_option_help_table[] = {
    {
        'o',
        "outfile",
        "-o, --outfile              specify output file name.\n"
        "                           (default:stdout)\n"
        "                           Use a name ending in \".png\"\n"
        "                           or the literal \"png:-\" or prefix a path\n"
        "                           with \"png:\" to emit PNG data recreated\n"
        "                           from the SIXEL pipeline. The PNG keeps\n"
        "                           the palette indices so every color\n"
        "                           matches the quantized SIXEL output\n"
        "                           exactly. \"png:-\" writes to stdout\n"
        "                           while \"png:<path>\" saves the PNG next\n"
        "                           to the SIXEL output.\n"
        "                           Supplying \"-o output.png\" writes the\n"
        "                           PNG directly to that path.\n"
    },
    {
        'a',
        "assessment",
        "-a LIST, --assessment=LIST emit assessment JSON report.\n"
        "                           LIST is a comma separated set of\n"
        "                           sections (basic, performance, size,\n"
        "                           quality, quality@quantized, all).\n"
        "                           Unknown names are ignored so the\n"
        "                           same command works across builds.\n"
    },
    {
        'J',
        "assessment-file",
        "-J PATH, --assessment-file=PATH\n"
        "                           write assessment JSON to PATH.\n"
        "                           use '-' to write to stdout.\n"
    },
    {
        '7',
        "7bit-mode",
        "-7, --7bit-mode            generate a sixel image for 7bit\n"
        "                           terminals or printers (default)\n"
    },
    {
        '8',
        "8bit-mode",
        "-8, --8bit-mode            generate a sixel image for 8bit\n"
        "                           terminals or printers\n"
    },
    {
        'R',
        "gri-limit",
        "-R, --gri-limit            limit arguments of DECGRI('!') to 255\n"
    },
    {
        '6',
        "6reversible",
        "-6, --6reversible          quantize via the SIXEL reversible tone set\n"
        "                           so decoding and re-encoding keeps the\n"
        "                           palette stable; diffusion carries the\n"
        "                           small residuals.\n"
    },
    {
        'p',
        "colors",
        "-p COLORS, --colors=COLORS specify number of colors to reduce\n"
        "                           the image to (default=256)\n"
    },
    {
        'm',
        "mapfile",
        "-m FILE, --mapfile=FILE    transform image colors to match\n"
        "                           this set of colors. Accepts\n"
        "                           image files and palette files\n"
        "                           in ACT, PAL, and GPL formats.\n"
        "                           Use TYPE:PATH (act:, pal:,\n"
        "                           pal-jasc:, pal-riff:, gpl:)\n"
        "                           to force a format. Without\n"
        "                           TYPE the extension or file\n"
        "                           contents decide. TYPE:- reads\n"
        "                           palette bytes from stdin (for\n"
        "                           example, gpl:-).\n"
    },
    {
        'M',
        "mapfile-output",
        "-M FILE, --mapfile-output=FILE\n"
        "                           export the computed palette.\n"
        "                           TYPE:PATH prefixes or file\n"
        "                           extensions (.act, .pal, .gpl)\n"
        "                           choose the format. .pal\n"
        "                           defaults to JASC text; use\n"
        "                           pal-riff: for RIFF output.\n"
        "                           Writing to '-' needs TYPE:PATH;\n"
        "                           TYPE:- sends the palette to\n"
        "                           stdout.\n"
    },
    {
        'e',
        "monochrome",
        "-e, --monochrome           output monochrome sixel image\n"
        "                           this option assumes the terminal\n"
        "                           background color is black\n"
    },
    {
        'k',
        "insecure",
        "-k, --insecure             allow to connect to SSL sites without\n"
        "                           certs(enabled only when configured\n"
        "                           with --with-libcurl)\n"
    },
    {
        'i',
        "invert",
        "-i, --invert               assume the terminal background color\n"
        "                           is white, make sense only when -e\n"
        "                           option is given\n"
    },
    {
        'I',
        "high-color",
        "-I, --high-color           output 15bpp sixel image\n"
    },
    {
        'u',
        "use-macro",
        "-u, --use-macro            use DECDMAC and DECINVM sequences to\n"
        "                           optimize GIF animation rendering\n"
    },
    {
        'n',
        "macro-number",
        "-n MACRONO, --macro-number=MACRONO\n"
        "                           specify an number argument for\n"
        "                           DECDMAC and make terminal memorize\n"
        "                           SIXEL image. No image is shown if\n"
        "                           this option is specified\n"
    },
    {
        'C',
        "complexion-score",
        "-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE\n"
        "                           [[deprecated]] specify an number\n"
        "                           argument for the score of\n"
        "                           complexion correction.\n"
        "                           COMPLEXIONSCORE must be 1 or more.\n"
    },
    {
        'g',
        "ignore-delay",
        "-g, --ignore-delay         render GIF animation without delay\n"
    },
    {
        'S',
        "static",
        "-S, --static               render animated GIF as a static image\n"
    },
    {
        'd',
        "diffusion",
        "-d DIFFUSIONTYPE, --diffusion=DIFFUSIONTYPE\n"
        "                           choose diffusion method which used\n"
        "                           with -p option (color reduction)\n"
        "                           DIFFUSIONTYPE is one of them:\n"
        "                             auto     -> choose diffusion type\n"
        "                                         automatically (default)\n"
        "                             none     -> do not diffuse\n"
        "                             fs       -> Floyd-Steinberg method\n"
        "                             atkinson -> Bill Atkinson's method\n"
        "                             jajuni   -> Jarvis, Judice & Ninke\n"
        "                             stucki   -> Stucki's method\n"
        "                             burkes   -> Burkes' method\n"
        "                             sierra1  -> Sierra Lite method\n"
        "                             sierra2  -> Sierra Two-row method\n"
        "                             sierra3  -> Sierra-3 method\n"
        "                             a_dither -> positionally stable\n"
        "                                         arithmetic dither\n"
        "                             x_dither -> positionally stable\n"
        "                                         arithmetic xor based dither\n"
        "                             lso1     -> libsixel's original method\n"
        "                             lso2     -> libsixel method based on\n"
        "                                         variable error diffusion\n"
        "                                         tables, optimized for size\n"
        "                             lso3     -> libsixel method based on\n"
        "                                         variable error diffusion\n"
        "                                         tables + jitter, optimized\n"
        "                                         for image quality\n"
    },
    {
        'y',
        "diffusion-scan",
        "-y SCANTYPE, --diffusion-scan=SCANTYPE\n"
        "                           choose scan order for diffusion\n"
        "                           SCANTYPE is one of them:\n"
        "                             auto -> choose scan order\n"
        "                                     automatically\n"
        "                             raster -> left-to-right scan\n"
        "                             serpentine -> alternate direction\n"
        "                                           on each line\n"
    },
    {
        'Y',
        "diffusion-carry",
        "-Y CARRYTYPE, --diffusion-carry=CARRYTYPE\n"
        "                           control carry buffers for diffusion\n"
        "                           CARRYTYPE is one of them:\n"
        "                             auto   -> choose carry mode\n"
        "                                        automatically\n"
        "                             direct -> write error back\n"
        "                                        to pixel data\n"
        "                                        immediately\n"
        "                             carry  -> accumulate error in\n"
        "                                        workspace buffers\n"
    },
    {
        'f',
        "find-largest",
        "-f FINDTYPE, --find-largest=FINDTYPE\n"
        "                           choose method for finding the largest\n"
        "                           dimension of median cut boxes for\n"
        "                           splitting, make sense only when -p\n"
        "                           option (color reduction) is\n"
        "                           specified\n"
        "                           FINDTYPE is one of them:\n"
        "                             auto -> choose finding method\n"
        "                                     automatically (default)\n"
        "                             norm -> simply comparing the\n"
        "                                     range in RGB space\n"
        "                             lum  -> transforming into\n"
        "                                     luminosities before the\n"
        "                                     comparison\n"
    },
    {
        's',
        "select-color",
        "-s SELECTTYPE, --select-color=SELECTTYPE\n"
        "                           choose the method for selecting\n"
        "                           representative color from each\n"
        "                           median-cut box, make sense only\n"
        "                           when -p option (color reduction) is\n"
        "                           specified\n"
        "                           SELECTTYPE is one of them:\n"
        "                             auto      -> choose selecting\n"
        "                                          method automatically\n"
        "                                          (default)\n"
        "                             center    -> choose the center of\n"
        "                                          the box\n"
        "                             average    -> calculate the color\n"
        "                                          average into the box\n"
        "                             histogram -> similar with average\n"
        "                                          but considers color\n"
        "                                          histogram\n"
    },
    {
        'c',
        "crop",
        "-c REGION, --crop=REGION   crop source image to fit the\n"
        "                           specified geometry. REGION should\n"
        "                           be formatted as '%dx%d+%d+%d'\n"
    },
    {
        'w',
        "width",
        "-w WIDTH, --width=WIDTH    resize image to specified width\n"
        "                           WIDTH is represented by the\n"
        "                           following syntax\n"
        "                             auto       -> preserving aspect\n"
        "                                           ratio (default)\n"
        "                             <number>%  -> scale width with\n"
        "                                           given percentage\n"
        "                             <number>   -> scale width with\n"
        "                                           pixel counts\n"
        "                             <number>px -> scale width with\n"
        "                                           pixel counts\n"
    },
    {
        'h',
        "height",
        "-h HEIGHT, --height=HEIGHT resize image to specified height\n"
        "                           HEIGHT is represented by the\n"
        "                           following syntax\n"
        "                             auto       -> preserving aspect\n"
        "                                           ratio (default)\n"
        "                             <number>%  -> scale height with\n"
        "                                           given percentage\n"
        "                             <number>   -> scale height with\n"
        "                                           pixel counts\n"
        "                             <number>px -> scale height with\n"
        "                                           pixel counts\n"
    },
    {
        'r',
        "resampling",
        "-r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE\n"
        "                           choose resampling filter used\n"
        "                           with -w or -h option (scaling)\n"
        "                           RESAMPLINGTYPE is one of them:\n"
        "                             nearest  -> Nearest-Neighbor\n"
        "                                         method\n"
        "                             gaussian -> Gaussian filter\n"
        "                             hanning  -> Hanning filter\n"
        "                             hamming  -> Hamming filter\n"
        "                             bilinear -> Bilinear filter\n"
        "                                         (default)\n"
        "                             welsh    -> Welsh filter\n"
        "                             bicubic  -> Bicubic filter\n"
        "                             lanczos2 -> Lanczos-2 filter\n"
        "                             lanczos3 -> Lanczos-3 filter\n"
        "                             lanczos4 -> Lanczos-4 filter\n"
    },
    {
        'q',
        "quality",
        "-q QUALITYMODE, --quality=QUALITYMODE\n"
        "                           select quality of color\n"
        "                           quanlization.\n"
        "                             auto -> decide quality mode\n"
        "                                     automatically (default)\n"
        "                             low  -> low quality and high\n"
        "                                     speed mode\n"
        "                             high -> high quality and low\n"
        "                                     speed mode\n"
        "                             full -> full quality and careful\n"
        "                                     speed mode\n"
    },
    {
        'L',
        "lut-policy",
        "-L LUTPOLICY, --lut-policy=LUTPOLICY\n"
        "                           choose histogram lookup width\n"
        "                           LUTPOLICY is one of them:\n"
        "                             auto      -> follow pixel depth\n"
        "                             5bit      -> force classic 5-bit\n"
        "                                          buckets\n"
        "                             6bit      -> favor 6-bit RGB\n"
        "                                          buckets\n"
        "                             robinhood -> retain 8-bit\n"
        "                                          precision via\n"
        "                                          Robin Hood hashing\n"
        "                             hopscotch -> retain 8-bit\n"
        "                                          precision via\n"
        "                                          Hopscotch hashing\n"
    },
    {
        'l',
        "loop-control",
        "-l LOOPMODE, --loop-control=LOOPMODE\n"
        "                           select loop control mode for GIF\n"
        "                           animation.\n"
        "                             auto    -> honor the setting of\n"
        "                                        GIF header (default)\n"
        "                             force   -> always enable loop\n"
        "                             disable -> always disable loop\n"
    },
    {
        't',
        "palette-type",
        "-t PALETTETYPE, --palette-type=PALETTETYPE\n"
        "                           select palette color space type\n"
        "                             auto -> choose palette type\n"
        "                                     automatically (default)\n"
        "                             hls  -> use HLS color space\n"
        "                             rgb  -> use RGB color space\n"
    },
    {
        'b',
        "builtin-palette",
        "-b BUILTINPALETTE, --builtin-palette=BUILTINPALETTE\n"
        "                           select built-in palette type\n"
        "                             xterm16    -> X default 16 color map\n"
        "                             xterm256   -> X default 256 color map\n"
        "                             vt340mono  -> VT340 monochrome map\n"
        "                             vt340color -> VT340 color map\n"
        "                             gray1      -> 1bit grayscale map\n"
        "                             gray2      -> 2bit grayscale map\n"
        "                             gray4      -> 4bit grayscale map\n"
        "                             gray8      -> 8bit grayscale map\n"
    },
    {
        'E',
        "encode-policy",
        "-E ENCODEPOLICY, --encode-policy=ENCODEPOLICY\n"
        "                           select encoding policy\n"
        "                             auto -> choose encoding policy\n"
        "                                     automatically (default)\n"
        "                             fast -> encode as fast as possible\n"
        "                             size -> encode to as small sixel\n"
        "                                     sequence as possible\n"
    },
    {
        'B',
        "bgcolor",
        "-B BGCOLOR, --bgcolor=BGCOLOR\n"
        "                           specify background color\n"
        "                           BGCOLOR is represented by the\n"
        "                           following syntax\n"
        "                             #rgb\n"
        "                             #rrggbb\n"
        "                             #rrrgggbbb\n"
        "                             #rrrrggggbbbb\n"
        "                             rgb:r/g/b\n"
        "                             rgb:rr/gg/bb\n"
        "                             rgb:rrr/ggg/bbb\n"
        "                             rgb:rrrr/gggg/bbbb\n"
    },
    {
        'P',
        "penetrate",
        "-P, --penetrate            [[deprecated]] penetrate GNU Screen\n"
        "                           using DCS pass-through sequence\n"
    },
    {
        'D',
        "pipe-mode",
        "-D, --pipe-mode            [[deprecated]] read source images from\n"
        "                           stdin continuously\n"
    },
    {
        'v',
        "verbose",
        "-v, --verbose              show debugging info\n"
    },
    {
        'j',
        "loaders",
        "-j LIST, --loaders=LIST    choose loader priority order\n"
        "                           LIST is a comma separated set of\n"
        "                           loader names like 'gd,builtin'\n"
    },
    {
        '@',
        "drcs",
        "-@ MMV:CHARSET:PATH, --drcs=MMV:CHARSET:PATH\n"
        "                           emit DRCS tiles instead of SIXEL output.\n"
        "                           MMV selects the mapping revision (0..2,\n"
        "                           default 2). CHARSET chooses the slot\n"
        "                           (1-126 when MMV=0, 1-63 when MMV=1,\n"
        "                           1-158 when MMV=2; default 1). PATH routes\n"
        "                           tile data (\"-\" keeps stdout; blank disables\n"
        "                           the external sink).\n"
    },
    {
        'O',
        "ormode",
        "-O, --ormode               enables sixel output in \"ormode\"\n"
    },
    {
        'W',
        "working-colorspace",
        "-W WORKING_COLORSPACE, --working-colorspace=WORKING_COLORSPACE\n"
        "                           choose internal working color space\n"
        "                             gamma  -> sRGB gamma(default)\n"
        "                             linear -> linear RGB color space\n"
        "                             oklab  -> OKLab color space\n"
    },
    {
        'U',
        "output-colorspace",
        "-U OUTPUT_COLORSPACE, --output-colorspace=OUTPUT_COLORSPACE\n"
        "                           choose output color space\n"
        "                             gamma   -> sRGB gamma(default)\n"
        "                             linear  -> linear RGB color space\n"
        "                             smpte-c -> SMPTE-C gamma color space\n"
    },
    {
        '1',
        "show-completion",
        "-1, --show-completion[=bash|zsh|all]\n"
        "                           print shell completion script\n"
    },
    {
        '2',
        "install-completion",
        "-2, --install-completion[=bash|zsh|all]\n"
        "                           install shell completion script\n"
    },
    {
        '3',
        "uninstall-completion",
        "-3, --uninstall-completion[=bash|zsh|all]\n"
        "                           uninstall shell completion script\n"
    },
    {
        'V',
        "version",
        "-V, --version              show version and license info\n"
    },
    {
        'H',
        "help",
        "-H, --help                 show this help\n"
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"img2sixel -H\" for more details.\n";

static char const g_img2sixel_optstring[] =
    "o:a:J:j:786Rp:m:M:eb:Id:f:s:c:w:h:r:q:L:kil:t:ugvSn:PE:U:B:C:D@:"
    "OVW:HY:y:";

static img2sixel_option_help_t const *
img2sixel_find_option_help(int short_opt)
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

static img2sixel_option_help_t const *
img2sixel_find_option_help_by_long_name(char const *long_name)
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
img2sixel_option_requires_argument(int short_opt)
{
    char const *cursor;

    cursor = g_img2sixel_optstring;

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
img2sixel_option_allows_leading_dash(int short_opt)
{
    /*
     * The options below accept file paths and we must not treat values that
     * start with '-' as missing arguments.  Users legitimately pass
     * "-p"-prefixed names through -o or -J when they want files that happen to
     * look like short options.
     */
    if (short_opt == 'o' || short_opt == 'J') {
        return 1;
    }

    return 0;
}

static int
img2sixel_token_is_known_option(char const *token, int *out_short_opt)
{
    img2sixel_option_help_t const *entry;
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
        entry = img2sixel_find_option_help_by_long_name(long_name);
    } else {
        entry = img2sixel_find_option_help((unsigned char)token[1]);
    }

    if (entry == NULL) {
        return 0;
    }

    if (out_short_opt != NULL) {
        *out_short_opt = entry->short_opt;
    }

    return 1;
}

static void img2sixel_report_missing_argument(int short_opt);

static int
img2sixel_guard_missing_argument(int short_opt, char *const *argv)
{
    int recognised;
    int candidate_short_opt;

    recognised = 0;
    candidate_short_opt = 0;

    if (img2sixel_option_requires_argument(short_opt) == 0) {
        return 0;
    }

    if (optarg == NULL) {
        img2sixel_report_missing_argument(short_opt);
        return -1;
    }

    if (img2sixel_option_allows_leading_dash(short_opt) != 0) {
        return 0;
    }

    recognised = img2sixel_token_is_known_option(optarg,
                                                 &candidate_short_opt);
    if (recognised != 0) {
        if (optind > 0 && optarg == argv[optind - 1]) {
            /*
             * When getopt() consumed a separate token as the argument it
             * stores a pointer to the original argv entry in optarg.  The
             * ASCII timeline below illustrates the state we are about to fix:
             *
             *   argv index : 0     1     2     3
             *   token     : img2sixel  -m    -w    100
             *                       ^optarg
             *
             * By rewinding optind we hand control of "-w" back to getopt() so
             * the parser can interpret it as the option the user intended.
             */
            optind -= 1;
            img2sixel_report_missing_argument(short_opt);
            return -1;
        }
    }

    return 0;
}

static void
img2sixel_print_option_help(FILE *stream)
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
img2sixel_report_invalid_argument(int short_opt,
                                  char const *value,
                                  char const *detail)
{
    char buffer[1024];
    char detail_copy[1024];
    img2sixel_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    entry = img2sixel_find_option_help(short_opt);
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
img2sixel_report_missing_argument(int short_opt)
{
    char buffer[1024];
    img2sixel_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    size_t offset;
    int written;

    /*
     * States which option is missing an argument
     */
    memset(buffer, 0, sizeof(buffer));
    entry = img2sixel_find_option_help(short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "img2sixel: missing required argument for "
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
img2sixel_report_unrecognized_option(int short_opt, char const *token)
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
                           "img2sixel: unrecognized option '%s'.\n",
                           view);
    } else if (short_opt > 0 && short_opt != '?') {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "img2sixel: unrecognized option '-%c'.\n",
                           (char)short_opt);
    } else {
        written = snprintf(buffer,
                           sizeof(buffer),
                           "img2sixel: unrecognized option.\n");
    }
    if (written < 0) {
        written = 0;
    }

    sixel_helper_set_additional_message(buffer);
}

static void
img2sixel_handle_getopt_error(int short_opt, char const *token)
{
    img2sixel_option_help_t const *entry;
    img2sixel_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = img2sixel_find_option_help(short_opt);
        if (entry != NULL) {
            img2sixel_report_missing_argument(short_opt);
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
            long_entry = img2sixel_find_option_help_by_long_name(long_name);
            if (long_entry != NULL) {
                img2sixel_report_missing_argument(long_entry->short_opt);
                return;
            }
        }
    }

    img2sixel_report_unrecognized_option(short_opt, token);
}


static int
img2sixel_case_insensitive_equals(char const *lhs, char const *rhs)
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
img2sixel_basename_view(char const *path)
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
img2sixel_extension_view(char const *name)
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
img2sixel_normalized_levenshtein(char const *lhs, char const *rhs)
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

#define IMG2SIXEL_SUGGESTION_LIMIT 5u
#define IMG2SIXEL_SUGGESTION_NAME_WEIGHT 0.55
#define IMG2SIXEL_SUGGESTION_EXTENSION_WEIGHT 0.25
#define IMG2SIXEL_SUGGESTION_RECENCY_WEIGHT 0.20

typedef struct img2sixel_path_candidate {
    char *path;
    char const *name;
    time_t mtime;
    double name_score;
    double extension_score;
    double recency_score;
    double total_score;
} img2sixel_path_candidate_t;

static double
img2sixel_extension_similarity(char const *lhs, char const *rhs)
{
    char const *lhs_extension;
    char const *rhs_extension;

    lhs_extension = img2sixel_extension_view(lhs);
    rhs_extension = img2sixel_extension_view(rhs);
    if (lhs_extension == NULL || rhs_extension == NULL) {
        return 0.0;
    }
    if (img2sixel_case_insensitive_equals(lhs_extension, rhs_extension)) {
        return 1.0;
    }

    return 0.0;
}

static char *
img2sixel_duplicate_string(char const *text)
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
img2sixel_duplicate_directory(char const *path)
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
        return img2sixel_duplicate_string(".");
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
        return img2sixel_duplicate_string(".");
    }
    if (separator == path) {
        return img2sixel_duplicate_string("/");
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
img2sixel_join_directory_entry(char const *directory, char const *entry)
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

static void
img2sixel_format_timestamp(time_t value,
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
        (void) strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", &time_view);
        return;
    }
#endif
    time_pointer = localtime(&value);
    if (time_pointer != NULL) {
        (void) strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", time_pointer);
        return;
    }

    (void) snprintf(buffer, buffer_size, "unknown");
}

static int
img2sixel_candidate_compare(void const *lhs, void const *rhs)
{
    img2sixel_path_candidate_t const *left;
    img2sixel_path_candidate_t const *right;

    left = (img2sixel_path_candidate_t const *)lhs;
    right = (img2sixel_path_candidate_t const *)rhs;

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
# if defined(_GNU_SOURCE)
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
    return buffer;
# else
    if (strerror_r(error_number, buffer, buffer_size) != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
# endif
#endif
}

static int
img2sixel_build_missing_file_message(char const *option_label,
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
    img2sixel_path_candidate_t *candidates;
    img2sixel_path_candidate_t *grown;
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

    directory_copy = img2sixel_duplicate_directory(resolved_path);
    if (directory_copy == NULL) {
        return -1;
    }
    argument_view = (argument != NULL && argument[0] != '\0')
        ? argument : resolved_path;
    target_name = img2sixel_basename_view(resolved_path);
    offset = 0u;
    written = 0;
    result = 0;

    if (buffer == NULL || buffer_size == 0u) {
        free(directory_copy);
        return -1;
    }

    written = snprintf(buffer,
                       buffer_size,
                       "img2sixel: %s \"%s\" not found.\n",
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

#if !HAVE_DIRENT_H
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
        goto cleanup;
    }

    while ((entry = readdir(directory_stream)) != NULL) {
        if (entry->d_name[0] == '.' &&
                (entry->d_name[1] == '\0' ||
                 (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        candidate_path = img2sixel_join_directory_entry(directory_copy,
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
            grown = (img2sixel_path_candidate_t *)realloc(
                candidates,
                new_capacity * sizeof(img2sixel_path_candidate_t));
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
            img2sixel_basename_view(candidate_path);
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
        goto cleanup;
    }

    min_mtime = candidates[0].mtime;
    max_mtime = candidates[0].mtime;
    for (index = 0u; index < candidate_count; ++index) {
        candidates[index].name_score =
            img2sixel_normalized_levenshtein(target_name,
                                             candidates[index].name);
        candidates[index].extension_score =
            img2sixel_extension_similarity(target_name,
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
         * how the weights trace fuzzy finder scoring research:
         * textual similarity dominates while the
         * extension and recency values provide smaller tie breakers.  The
         * percentages mirror the balance used by popular tools such as
         * Sublime Text's Goto Anything and the "fzy" matcher.
         */
        candidates[index].total_score =
            IMG2SIXEL_SUGGESTION_NAME_WEIGHT *
                candidates[index].name_score +
            IMG2SIXEL_SUGGESTION_EXTENSION_WEIGHT *
                candidates[index].extension_score +
            IMG2SIXEL_SUGGESTION_RECENCY_WEIGHT *
                candidates[index].recency_score;
    }

    qsort(candidates,
          candidate_count,
          sizeof(img2sixel_path_candidate_t),
          img2sixel_candidate_compare);

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
            && index < IMG2SIXEL_SUGGESTION_LIMIT; ++index) {
        percent_double = candidates[index].total_score * 100.0;
        if (percent_double < 0.0) {
            percent_double = 0.0;
        }
        if (percent_double > 100.0) {
            percent_double = 100.0;
        }
        percent_int = (int)(percent_double + 0.5);
        img2sixel_format_timestamp(candidates[index].mtime,
                                   time_buffer,
                                   sizeof(time_buffer));
        if (offset < buffer_size - 1u) {
            written = snprintf(buffer + offset,
                               buffer_size - offset,
                               "  * %s (similarity score %d%%, modified %s)\n",
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

cleanup:
    if (directory_stream != NULL) {
        (void) closedir(directory_stream);
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

static char const *
img2sixel_palette_payload_view(char const *spec)
{
    char const *colon;
    size_t type_length;
    size_t index;
    char lowered[16];

    colon = NULL;
    type_length = 0u;
    index = 0u;
    memset(lowered, 0, sizeof(lowered));

    if (spec == NULL) {
        return NULL;
    }

    colon = strchr(spec, ':');
    if (colon == NULL) {
        return spec;
    }

    type_length = (size_t)(colon - spec);
    if (type_length == 0u || type_length >= sizeof(lowered)) {
        return spec;
    }

    while (index < type_length) {
        lowered[index] = (char)tolower((unsigned char)spec[index]);
        ++index;
    }
    lowered[type_length] = '\0';

    if (strcmp(lowered, "act") == 0) {
        return colon + 1;
    }
    if (strcmp(lowered, "pal") == 0) {
        return colon + 1;
    }
    if (strcmp(lowered, "pal-jasc") == 0) {
        return colon + 1;
    }
    if (strcmp(lowered, "pal-riff") == 0) {
        return colon + 1;
    }
    if (strcmp(lowered, "gpl") == 0) {
        return colon + 1;
    }

    return spec;
}

static int
img2sixel_path_looks_remote(char const *path)
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
img2sixel_validate_mapfile_argument(char const *argument)
{
    char const *payload;
    struct stat path_stat;
    int stat_result;
    int error_value;
    char message_buffer[1024];

    payload = NULL;
    memset(&path_stat, 0, sizeof(path_stat));
    stat_result = 0;
    error_value = 0;
    memset(message_buffer, 0, sizeof(message_buffer));

    if (argument == NULL || argument[0] == '\0') {
        return 0;
    }

    payload = img2sixel_palette_payload_view(argument);
    if (payload == NULL || payload[0] == '\0') {
        return 0;
    }
    if (strcmp(payload, "-") == 0) {
        return 0;
    }
    if (img2sixel_path_looks_remote(payload)) {
        return 0;
    }

    errno = 0;
    stat_result = stat(payload, &path_stat);
    if (stat_result == 0) {
        return 0;
    }

    error_value = errno;
    if (error_value != ENOENT && error_value != ENOTDIR) {
        return 0;
    }

    if (img2sixel_build_missing_file_message("-m/--mapfile",
                                             argument,
                                             payload,
                                             message_buffer,
                                             sizeof(message_buffer)) != 0) {
        sixel_helper_set_additional_message(
            "img2sixel: mapfile path validation failed.");
    } else {
        sixel_helper_set_additional_message(message_buffer);
    }

    return -1;
}

static int
img2sixel_validate_input_argument(char const *argument)
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
            "img2sixel: input path is empty.");
        return -1;
    }
    if (strcmp(argument, "-") == 0) {
        return 0;
    }
    if (img2sixel_path_looks_remote(argument)) {
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

    if (img2sixel_build_missing_file_message("input file",
                                             argument,
                                             argument,
                                             message_buffer,
                                             sizeof(message_buffer)) != 0) {
        sixel_helper_set_additional_message(
            "img2sixel: input path validation failed.");
    } else {
        sixel_helper_set_additional_message(message_buffer);
    }

    return -1;
}


/* output version info to STDOUT */
static
void show_version(void)
{
    size_t loader_count;
    char const **loader_names;
    size_t loader_index;

    loader_count = 0;
    loader_names = NULL;
    loader_index = 0;

    printf("img2sixel " PACKAGE_VERSION "\n"
           "\n"
           "configured with:\n"
           "  libcurl: "
#ifdef HAVE_LIBCURL
           "yes\n"
#else
           "no\n"
#endif
           "  WinHTTP: "
#ifdef HAVE_WINHTTP
           "yes\n"
#else
           "no\n"
#endif
           "  libpng: "
#ifdef HAVE_LIBPNG
           "yes\n"
#else
           "no\n"
#endif
           "  libjpeg: "
#ifdef HAVE_JPEG
           "yes\n"
#else
           "no\n"
#endif
           "  gdk-pixbuf2: "
#ifdef HAVE_GDK_PIXBUF2
           "yes\n"
#else
           "no\n"
#endif
           "  GD: "
#ifdef HAVE_GD
           "yes\n"
#else
           "no\n"
#endif
           "  WIC: "
#ifdef HAVE_WIC
           "yes\n"
#else
           "no\n"
#endif
           "  ONNX Runtime: "
#ifdef HAVE_ONNXRUNTIME
           "yes\n"
#else
           "no\n"
#endif
           "  CoreGraphics: "
#ifdef HAVE_COREGRAPHICS
           "yes\n"
#else
           "no\n"
#endif
           "\n"
          );

    loader_count = sixel_helper_get_available_loader_names(NULL, 0);
    if (loader_count > 0) {
        loader_names = (char const **)malloc(loader_count *
                                             sizeof(char const *));
        if (loader_names != NULL) {
            if (sixel_helper_get_available_loader_names(loader_names,
                                                        loader_count)
                != loader_count) {
                free(loader_names);
                loader_names = NULL;
                loader_count = 0;
            }
        }
    }

    printf("available loaders:\n");
    if (loader_names != NULL && loader_count > 0) {
        for (loader_index = 0; loader_index < loader_count;
             ++loader_index) {
            printf("  %s\n", loader_names[loader_index]);
        }
    } else if (loader_count > 0) {
        printf("  (enumeration failed)\n");
    } else {
        printf("  (none)\n");
    }
    free(loader_names);

    printf("\n"
           "Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.\n"
           "Copyright (C) 2014-2020 Hayaki Saito <saitoha@me.com>.\n"
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


/* output help messages to STDOUT */
static
void show_help(void)
{
    fprintf(stdout,
            "Usage: img2sixel [Options] imagefiles\n"
            "       img2sixel [Options] < imagefile\n"
            "\n"
            "Options:\n");
    img2sixel_print_option_help(stdout);
    fprintf(stdout,
            "\n"
            "Environment variables:\n"
            "SIXEL_BGCOLOR              specify background color.\n"
            "                           overrided by -B(--bgcolor) option.\n"
            "                           represented by the following\n"
            "                           syntax:\n"
            "                             #rgb\n"
            "                             #rrggbb\n"
            "                             #rrrgggbbb\n"
            "                             #rrrrggggbbbb\n"
            "                             rgb:r/g/b\n"
            "                             rgb:rr/gg/bb\n"
            "                             rgb:rrr/ggg/bbb\n"
            "                             rgb:rrrr/gggg/bbbb\n"
            );
}


#if HAVE_SIGNAL

static int signaled = 0;

static void
signal_handler(int sig)
{
    signaled = sig;
}

#endif

int
main(int argc, char *argv[])
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    sixel_encoder_t *encoder = NULL;
    int completion_cli_result;
    int completion_exit_status;
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    char const *optstring;
#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"outfile",            required_argument,  &long_opt, 'o'},
        {"assessment",         required_argument,  &long_opt, 'a'},
        {"assessment-file",    required_argument,  &long_opt, 'J'},
        {"7bit-mode",          no_argument,        &long_opt, '7'},
        {"8bit-mode",          no_argument,        &long_opt, '8'},
        {"gri-limit",          no_argument,        &long_opt, 'R'},
        {"6reversible",        no_argument,        &long_opt, '6'},
        {"colors",             required_argument,  &long_opt, 'p'},
        {"mapfile",            required_argument,  &long_opt, 'm'},
        {"mapfile-output",     required_argument,  &long_opt, 'M'},
        {"monochrome",         no_argument,        &long_opt, 'e'},
        {"high-color",         no_argument,        &long_opt, 'I'},
        {"builtin-palette",    required_argument,  &long_opt, 'b'},
        {"diffusion",          required_argument,  &long_opt, 'd'},
        {"diffusion-scan",     required_argument,  &long_opt, 'y'},
        {"diffusion-carry",    required_argument,  &long_opt, 'Y'},
        {"find-largest",       required_argument,  &long_opt, 'f'},
        {"select-color",       required_argument,  &long_opt, 's'},
        {"crop",               required_argument,  &long_opt, 'c'},
        {"width",              required_argument,  &long_opt, 'w'},
        {"height",             required_argument,  &long_opt, 'h'},
        {"resampling",         required_argument,  &long_opt, 'r'},
        {"quality",            required_argument,  &long_opt, 'q'},
        {"lut-policy",         required_argument,  &long_opt, 'L'},
        {"palette-type",       required_argument,  &long_opt, 't'},
        {"insecure",           no_argument,        &long_opt, 'k'},
        {"invert",             no_argument,        &long_opt, 'i'},
        {"loop-control",       required_argument,  &long_opt, 'l'},
        {"use-macro",          no_argument,        &long_opt, 'u'},
        {"ignore-delay",       no_argument,        &long_opt, 'g'},
        {"verbose",            no_argument,        &long_opt, 'v'},
        {"loaders",            required_argument,  &long_opt, 'j'},
        {"static",             no_argument,        &long_opt, 'S'},
        {"macro-number",       required_argument,  &long_opt, 'n'},
        {"penetrate",          no_argument,        &long_opt, 'P'}, /* deprecated */
        {"encode-policy",      required_argument,  &long_opt, 'E'},
        {"output-colorspace",  required_argument,  &long_opt, 'U'},
        {"working-colorspace", required_argument,  &long_opt, 'W'},
        {"bgcolor",            required_argument,  &long_opt, 'B'},
        {"complexion-score",   required_argument,  &long_opt, 'C'}, /* deprecated */
        {"pipe-mode",          no_argument,        &long_opt, 'D'}, /* deprecated */
        {"drcs",               required_argument,  &long_opt, '@'},
        {"ormode",             no_argument,        &long_opt, 'O'},
        {"version",            no_argument,        &long_opt, 'V'},
        {"help",               no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */
    char detail_buffer[2048];
    char const *detail_source = NULL;

    optstring = g_img2sixel_optstring;

    completion_exit_status = 0;
    completion_cli_result = img2sixel_handle_completion_cli(
        argc, argv, &completion_exit_status);
    if (completion_cli_result < 0) {
        return completion_exit_status;
    }
    if (completion_cli_result > 0) {
        return completion_exit_status;
    }

    status = sixel_encoder_new(&encoder, NULL);
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
            break;
        }
#if HAVE_GETOPT_LONG
        if (n == 0) {
            n = long_opt;
        }
#endif  /* HAVE_GETOPT_LONG */

        if (n > 0) {
            if (img2sixel_guard_missing_argument(n, argv) != 0) {
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
            img2sixel_handle_getopt_error(
                optopt,
                (optind > 0 && optind <= argc)
                    ? argv[optind - 1]
                    : NULL);
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        case 'a':
            status = sixel_encoder_setopt(encoder,
                                          SIXEL_OPTFLAG_ASSESSMENT,
                                          optarg);
            if (status == SIXEL_BAD_ARGUMENT) {
                img2sixel_report_invalid_argument(
                    'a',
                    optarg,
                    "img2sixel: invalid assessment section list.");
                goto error;
            }
            break;
        case 'J':
            status = sixel_encoder_setopt(encoder,
                                          SIXEL_OPTFLAG_ASSESSMENT_FILE,
                                          optarg);
            break;
        case 'j':
            status = sixel_encoder_setopt(encoder,
                                          SIXEL_OPTFLAG_LOADERS,
                                          optarg);
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
                    img2sixel_report_invalid_argument(
                        'j',
                        optarg,
                        detail_buffer[0] != '\0'
                            ? detail_buffer
                            : NULL);
                }
                goto error;
            }
            break;
        case 'm':
            if (img2sixel_validate_mapfile_argument(optarg) != 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
            status = sixel_encoder_setopt(encoder, n, optarg);
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
                    img2sixel_report_invalid_argument(
                        n,
                        optarg,
                        detail_buffer[0] != '\0'
                            ? detail_buffer
                            : NULL);
                }
                goto error;
            }
            break;
        case 'o':
            status = sixel_encoder_setopt(encoder, n, optarg);
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
                    img2sixel_report_invalid_argument(
                        n,
                        optarg,
                        detail_buffer[0] != '\0'
                            ? detail_buffer
                            : NULL);
                }
                goto end;
            }
            break;
        default:
            status = sixel_encoder_setopt(encoder, n, optarg);
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
                    img2sixel_report_invalid_argument(
                        n,
                        optarg,
                        detail_buffer[0] != '\0'
                            ? detail_buffer
                            : NULL);
                }
                goto error;
            }
            break;
        }
    }

#if 0  /* check multiple input files with assessment option */
        input_count = argc - optind;
        if (input_count > 1) {
            sixel_helper_set_additional_message(
                "img2sixel: assessment accepts at most one input file.");
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        }
#endif


    /* set signal handler to handle SIGINT/SIGTERM/SIGHUP */
#if HAVE_SIGNAL
# if HAVE_DECL_SIGINT
    signal(SIGINT, signal_handler);
# endif
# if HAVE_DECL_SIGTERM
    signal(SIGTERM, signal_handler);
# endif
# if HAVE_DECL_SIGHUP
    signal(SIGHUP, signal_handler);
# endif
    status = sixel_encoder_set_cancel_flag(encoder, &signaled);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
#else
    (void) signal_handler;
#endif
    if (optind == argc) {
        status = sixel_encoder_encode(encoder, NULL);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else {
        for (n = optind; n < argc; n++) {
            if (img2sixel_validate_input_argument(argv[n]) != 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
            status = sixel_encoder_encode(encoder, argv[n]);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }
    }

    /* mark as success */
    status = SIXEL_OK;
    goto end;

error:
    fprintf(stderr, "\n%s\n%s\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    status = (-1);
    fprintf(stderr,
            "usage: img2sixel [-78eIkiugvSPDOVH] [-p colors] [-m file] [-d diffusiontype]\n"
            "                 [-y scantype] [-a assessmentlist] [-J assessmentfile]\n"
            "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
            "                 [-h height] [-r resamplingtype] [-q quality] [-l loopmode]\n"
            "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
            "                 [-E encodepolicy] [-j loaderlist] [-J jsonfile]\n"
            "                 [-@ mmv:charset:path] [-1 shell] [-2 shell]\n"
            "                 [-3 shell] [-W workingcolorspace] [-U outputcolorspace]\n"
            "                 [-B bgcolor] [-o outfile] [filename ...]\n\n"
            "for more details, type: 'img2sixel -H'.\n");

end:
    sixel_encoder_unref(encoder);
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
