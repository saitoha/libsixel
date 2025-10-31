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

#include "config.h"
#include "malloc_stub.h"
#include "getopt_stub.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

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
#if HAVE_GETOPT_H
# include <getopt.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_SIGNAL_H
# include <signal.h>
#elif HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#include <sixel.h>
#include "../src/frame.h"
#include "../src/assessment.h"
#include "completion_utils.h"

#if defined(HAVE_MKSTEMP)
int mkstemp(char *);
#endif

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
        'p',
        "colors",
        "-p COLORS, --colors=COLORS specify number of colors to reduce\n"
        "                           the image to (default=256)\n"
    },
    {
        'm',
        "mapfile",
        "-m FILE, --mapfile=FILE    transform image colors to match this\n"
        "                           set of colorsspecify map\n"
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

static int
is_png_target(char const *path)
{
    size_t len;
    int matched;

    len = 0;
    matched = 0;

    if (path == NULL) {
        return 0;
    }

    if (strcmp(path, "png:-") == 0) {
        return 1;
    }

    len = strlen(path);
    if (len >= 4) {
        matched = (tolower((unsigned char)path[len - 4]) == '.')
            && (tolower((unsigned char)path[len - 3]) == 'p')
            && (tolower((unsigned char)path[len - 2]) == 'n')
            && (tolower((unsigned char)path[len - 1]) == 'g');
    }

    return matched;
}

static int
is_dev_null_path(char const *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
#if defined(_WIN32)
    if (_stricmp(path, "nul") == 0) {
        return 1;
    }
#endif
    return strcmp(path, "/dev/null") == 0;
}

static char *
create_temp_template(void)
{
    char const *tmpdir;
    size_t tmpdir_len;
    size_t suffix_len;
    size_t template_len;
    char *template_path;
    int needs_separator;
    size_t maximum_tmpdir_len;

    tmpdir = getenv("TMPDIR");
#if defined(_WIN32)
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = getenv("TEMP");
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = getenv("TMP");
    }
#endif
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#if defined(_WIN32)
        tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }

    tmpdir_len = strlen(tmpdir);
    suffix_len = strlen("img2sixel-XXXXXX");
    maximum_tmpdir_len = (size_t)INT_MAX;

    if (maximum_tmpdir_len <= suffix_len + 2) {
        return NULL;
    }
    if (tmpdir_len > maximum_tmpdir_len - (suffix_len + 2)) {
        return NULL;
    }
    needs_separator = 1;
    if (tmpdir_len > 0) {
        if (tmpdir[tmpdir_len - 1] == '/' || tmpdir[tmpdir_len - 1] == '\\') {
            needs_separator = 0;
        }
    }

    template_len = tmpdir_len + suffix_len + 2;
    template_path = (char *)malloc(template_len);
    if (template_path == NULL) {
        return NULL;
    }

    if (needs_separator) {
#if defined(_WIN32)
        (void) snprintf(template_path, template_len,
                        "%s\\%s", tmpdir, "img2sixel-XXXXXX");
#else
        (void) snprintf(template_path, template_len,
                        "%s/%s", tmpdir, "img2sixel-XXXXXX");
#endif
    } else {
        (void) snprintf(template_path, template_len,
                        "%s%s", tmpdir, "img2sixel-XXXXXX");
    }

    return template_path;
}

/*
 * Build a tee for encoded-assessment output:
 *
 *     +-------------+     +-------------------+     +------------+
 *     | encoder FD  | --> | temporary SIXEL   | --> | tee sink   |
 *     +-------------+     +-------------------+     +------------+
 *
 * The tee sink can be stdout or a user-provided file such as /dev/null.
 */
static SIXELSTATUS
copy_file_to_stream(char const *path,
                    FILE *stream,
                    sixel_assessment_t *assessment)
{
    FILE *source;
    unsigned char buffer[4096];
    size_t nread;
    size_t nwritten;
    double started_at;
    double finished_at;
    double duration;

    source = NULL;
    nread = 0;
    nwritten = 0;
    started_at = 0.0;
    finished_at = 0.0;
    duration = 0.0;

    source = fopen(path, "rb");
    if (source == NULL) {
        sixel_helper_set_additional_message(
            "img2sixel: failed to open assessment staging file.");
        return SIXEL_LIBC_ERROR;
    }

    for (;;) {
        nread = fread(buffer, 1, sizeof(buffer), source);
        if (nread == 0) {
            if (ferror(source)) {
                sixel_helper_set_additional_message(
                    "img2sixel: failed while reading assessment staging file.");
                (void) fclose(source);
                return SIXEL_LIBC_ERROR;
            }
            break;
        }
        if (assessment != NULL) {
            started_at = sixel_assessment_timer_now();
        }
        nwritten = fwrite(buffer, 1, nread, stream);
        if (nwritten != nread) {
            sixel_helper_set_additional_message(
                "img2sixel: failed while copying assessment staging file.");
            (void) fclose(source);
            return SIXEL_LIBC_ERROR;
        }
        if (assessment != NULL) {
            finished_at = sixel_assessment_timer_now();
            duration = finished_at - started_at;
            if (duration < 0.0) {
                duration = 0.0;
            }
            sixel_assessment_record_output_write(assessment,
                                                 nwritten,
                                                 duration);
        }
    }

    if (fclose(source) != 0) {
        sixel_helper_set_additional_message(
            "img2sixel: failed to close assessment staging file.");
        return SIXEL_LIBC_ERROR;
    }

    return SIXEL_OK;
}

typedef struct assessment_json_sink {
    FILE *stream;
    int failed;
} assessment_json_sink_t;

static void
assessment_json_callback(char const *chunk,
                         size_t length,
                         void *user_data)
{
    assessment_json_sink_t *sink;

    sink = (assessment_json_sink_t *)user_data;
    if (sink == NULL || sink->stream == NULL) {
        return;
    }
    if (sink->failed) {
        return;
    }
    if (fwrite(chunk, 1, length, sink->stream) != length) {
        sink->failed = 1;
    }
}

static SIXELSTATUS
write_png_from_sixel(char const *sixel_path, char const *output_path)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;

    status = SIXEL_FALSE;
    decoder = NULL;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, sixel_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, output_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_decode(decoder);

end:
    sixel_decoder_unref(decoder);

    return status;
}

/*
 * parse_assessment_sections() translates a comma-separated section list into
 * the bitmask understood by the assessment back-end.  The accepted grammar is
 * intentionally small so that the CLI contract stays predictable:
 *
 *     list := section {"," section}
 *     section := name | name "@" view
 *
 * Each name maps to a bit flag while the optional view toggles the encoded vs
 * quantized quality comparison.  The helper folds case, trims ASCII
 * whitespace, and rejects mixed encoded/quantized requests so that the caller
 * can rely on a single coherent capture strategy.
 */
static int
parse_assessment_sections(char const *spec,
                          unsigned int *out_sections)
{
    char *copy;
    char *cursor;
    char *token;
    unsigned int sections;
    unsigned int view;
    int result;
    size_t length;
    size_t spec_len;
    char *at;
    char *base;
    char *variant;
    char *p;

    if (spec == NULL || out_sections == NULL) {
        return -1;
    }
    spec_len = strlen(spec);
    copy = (char *)malloc(spec_len + 1u);
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, spec, spec_len + 1u);
    cursor = copy;
    sections = 0u;
    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
    result = 0;
    while (result == 0) {
        token = strtok(cursor, ",");
        if (token == NULL) {
            break;
        }
        cursor = NULL;
        while (*token == ' ' || *token == '\t') {
            token += 1;
        }
        length = strlen(token);
        while (length > 0u &&
               (token[length - 1u] == ' ' || token[length - 1u] == '\t')) {
            token[length - 1u] = '\0';
            length -= 1u;
        }
        if (*token == '\0') {
            result = -1;
            break;
        }
        for (p = token; *p != '\0'; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }
        at = strchr(token, '@');
        if (at != NULL) {
            *at = '\0';
            variant = at + 1;
            if (*variant == '\0') {
                result = -1;
                break;
            }
        } else {
            variant = NULL;
        }
        base = token;
        if (strcmp(base, "all") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_ALL;
            if (variant != NULL && variant[0] != '\0') {
                if (strcmp(variant, "quantized") == 0) {
                    if (view != SIXEL_ASSESSMENT_VIEW_ENCODED &&
                            view != SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_QUANTIZED;
                } else if (strcmp(variant, "encoded") == 0) {
                    if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
                } else {
                    result = -1;
                }
            }
        } else if (strcmp(base, "basic") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_BASIC;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "performance") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_PERFORMANCE;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "size") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_SIZE;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "quality") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_QUALITY;
            if (variant != NULL && variant[0] != '\0') {
                if (strcmp(variant, "quantized") == 0) {
                    if (view != SIXEL_ASSESSMENT_VIEW_ENCODED &&
                            view != SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_QUANTIZED;
                } else if (strcmp(variant, "encoded") == 0) {
                    if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
                } else {
                    result = -1;
                }
            } else if (variant != NULL) {
                if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                    result = -1;
                }
                view = SIXEL_ASSESSMENT_VIEW_ENCODED;
            }
        } else {
            result = -1;
        }
    }
    if (result == 0) {
        if (sections == 0u) {
            result = -1;
        } else {
            if ((sections & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u &&
                    view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                sections |= SIXEL_ASSESSMENT_VIEW_QUANTIZED;
            }
            *out_sections = sections;
        }
    }
    free(copy);
    return result;
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
    char const *optstring =
        "o:a:J:j:78Rp:m:eb:Id:f:s:c:w:h:r:q:L:kil:t:ugvSn:PE:U:B:C:D@:"
        "OVW:HY:y:";
#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"outfile",            required_argument,  &long_opt, 'o'},
        {"assessment",         required_argument,  &long_opt, 'a'},
        {"assessment-file",    required_argument,  &long_opt, 'J'},
        {"7bit-mode",          no_argument,        &long_opt, '7'},
        {"8bit-mode",          no_argument,        &long_opt, '8'},
        {"gri-limit",          no_argument,        &long_opt, 'R'},
        {"colors",             required_argument,  &long_opt, 'p'},
        {"mapfile",            required_argument,  &long_opt, 'm'},
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
    int output_is_png;
    int output_png_to_stdout;
    char *png_output_path;
    char *png_temp_path;
    int png_temp_fd;
    char const *png_final_path;
    unsigned int assessment_sections;
    unsigned int assessment_section_mask;
    int assessment_enabled;
    int assessment_need_source_capture;
    int assessment_need_quantized_capture;
    int assessment_need_quality;
    int assessment_quality_quantized;
    sixel_allocator_t *assessment_allocator;
    sixel_frame_t *assessment_source_frame;
    sixel_frame_t *assessment_target_frame;
    sixel_frame_t *assessment_expanded_frame;
    sixel_assessment_t *assessment;
    assessment_json_sink_t assessment_sink;
    char const *assessment_json_path;
    FILE *assessment_json_file;
    FILE *assessment_forward_stream;
    int assessment_json_owned;
    char *assessment_temp_path;
    int assessment_temp_fd;
    sixel_assessment_spool_mode_t assessment_spool_mode;
    char *assessment_forward_path;
    char *sixel_output_path;
    double assessment_parse_start;
    double assessment_parse_duration;
    size_t assessment_output_bytes;
#if HAVE_SYS_STAT_H
    struct stat assessment_stat;
    int assessment_stat_result;
    char const *assessment_size_path;
#endif
    char detail_buffer[1024];
    char const *detail_source;

    completion_exit_status = 0;
    completion_cli_result = img2sixel_handle_completion_cli(
        argc, argv, &completion_exit_status);
    if (completion_cli_result < 0) {
        return completion_exit_status;
    }
    if (completion_cli_result > 0) {
        return completion_exit_status;
    }

    output_is_png = 0;
    output_png_to_stdout = 0;
    png_output_path = NULL;
    png_temp_path = NULL;
    png_temp_fd = (-1);
    png_final_path = NULL;
    assessment_sections = SIXEL_ASSESSMENT_SECTION_NONE;
    assessment_section_mask = SIXEL_ASSESSMENT_SECTION_NONE;
    assessment_enabled = 0;
    assessment_need_source_capture = 0;
    assessment_need_quantized_capture = 0;
    assessment_need_quality = 0;
    assessment_quality_quantized = 0;
    assessment_allocator = NULL;
    assessment_source_frame = NULL;
    assessment_target_frame = NULL;
    assessment_expanded_frame = NULL;
    assessment = NULL;
    assessment_sink.stream = NULL;
    assessment_sink.failed = 0;
    assessment_json_path = NULL;
    assessment_json_file = NULL;
    assessment_forward_stream = NULL;
    assessment_json_owned = 0;
    assessment_temp_path = NULL;
    assessment_temp_fd = (-1);
    assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_NONE;
    assessment_forward_path = NULL;
    sixel_output_path = NULL;
    assessment_parse_start = sixel_assessment_timer_now();
    assessment_parse_duration = 0.0;
    assessment_output_bytes = 0u;
#if HAVE_SYS_STAT_H
    assessment_stat_result = 0;
    assessment_size_path = NULL;
#endif

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

        switch (n) {
        case 'V':
            show_version();
            status = SIXEL_OK;
            goto end;
        case 'H':
            show_help();
            status = SIXEL_OK;
            goto end;
        case 'a':
            if (parse_assessment_sections(optarg,
                                          &assessment_sections) != 0) {
                img2sixel_report_invalid_argument(
                    'a',
                    optarg,
                    "img2sixel: invalid assessment section list.");
                status = SIXEL_BAD_ARGUMENT;
                goto error;
            }
            break;
        case 'J':
            assessment_json_path = optarg;
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
        case 'o':
            if (is_png_target(optarg)) {
                output_is_png = 1;
                output_png_to_stdout = (strcmp(optarg, "png:-") == 0);
                free(png_output_path);
                png_output_path = NULL;
                free(sixel_output_path);
                sixel_output_path = NULL;
                if (!output_png_to_stdout) {
                    png_output_path = (char *)malloc(strlen(optarg) + 1);
                    if (png_output_path == NULL) {
                        sixel_helper_set_additional_message(
                            "img2sixel: malloc() failed for PNG output path.");
                        status = SIXEL_BAD_ALLOCATION;
                        goto error;
                    }
                    strcpy(png_output_path, optarg);
                }
            } else {
                output_is_png = 0;
                output_png_to_stdout = 0;
                free(png_output_path);
                png_output_path = NULL;
                free(sixel_output_path);
                sixel_output_path = NULL;
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
                if (strcmp(optarg, "-") != 0) {
                    sixel_output_path = (char *)malloc(strlen(optarg) + 1);
                    if (sixel_output_path == NULL) {
                        sixel_helper_set_additional_message(
                            "img2sixel: malloc() failed for output path.");
                        status = SIXEL_BAD_ALLOCATION;
                        goto error;
                    }
                    strcpy(sixel_output_path, optarg);
                }
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

    assessment_section_mask =
        assessment_sections & SIXEL_ASSESSMENT_SECTION_MASK;
    if (assessment_section_mask != 0u) {
        int input_count;
        int spool_required;

        assessment_enabled = 1;
        assessment_need_quality =
            (assessment_section_mask & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u;
        assessment_quality_quantized =
            (assessment_sections & SIXEL_ASSESSMENT_VIEW_QUANTIZED) != 0u;
        assessment_need_quantized_capture =
            ((assessment_section_mask &
              (SIXEL_ASSESSMENT_SECTION_BASIC |
               SIXEL_ASSESSMENT_SECTION_SIZE)) != 0u) ||
            assessment_quality_quantized;
        assessment_need_source_capture =
            (assessment_section_mask &
             (SIXEL_ASSESSMENT_SECTION_BASIC |
              SIXEL_ASSESSMENT_SECTION_PERFORMANCE |
              SIXEL_ASSESSMENT_SECTION_SIZE |
              SIXEL_ASSESSMENT_SECTION_QUALITY)) != 0u;
        input_count = argc - optind;
        if (input_count > 1) {
            sixel_helper_set_additional_message(
                "img2sixel: assessment accepts at most one input file.");
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        }
        if (assessment_need_quality && !assessment_quality_quantized &&
                output_is_png) {
            sixel_helper_set_additional_message(
                "img2sixel: encoded quality assessment requires SIXEL output.");
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        }
        status = sixel_encoder_enable_source_capture(encoder,
                                                     assessment_need_source_capture);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_STATIC, NULL);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        if (assessment_need_quantized_capture) {
            status = sixel_encoder_enable_quantized_capture(encoder, 1);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }
        assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_NONE;
        spool_required = 0;
        if (assessment_need_quality && !assessment_quality_quantized) {
            if (sixel_output_path == NULL) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT;
                spool_required = 1;
            } else if (strcmp(sixel_output_path, "-") == 0) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT;
                spool_required = 1;
                free(sixel_output_path);
                sixel_output_path = NULL;
            } else if (is_dev_null_path(sixel_output_path)) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_PATH;
                spool_required = 1;
                assessment_forward_path = sixel_output_path;
                sixel_output_path = NULL;
            }
        }
    if (spool_required) {
        assessment_temp_path = create_temp_template();
        if (assessment_temp_path == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: malloc() failed for assessment staging path.");
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
#if defined(HAVE_MKSTEMP)
        assessment_temp_fd = mkstemp(assessment_temp_path);
            if (assessment_temp_fd < 0) {
                sixel_helper_set_additional_message(
                    "img2sixel: mkstemp() failed for assessment staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto error;
            }
            (void) close(assessment_temp_fd);
            assessment_temp_fd = (-1);
#elif defined(HAVE__MKTEMP)
            if (_mktemp(assessment_temp_path) == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: _mktemp() failed for assessment staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto error;
            }
#elif defined(HAVE_MKTEMP)
            if (mktemp(assessment_temp_path) == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: mktemp() failed for assessment staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto error;
            }
#else
                {
                    char *generated;

                    generated = tmpnam(NULL);
                    if (generated == NULL) {
                        sixel_helper_set_additional_message(
                            "img2sixel: tmpnam() failed for assessment staging file.");
                        status = SIXEL_RUNTIME_ERROR;
                        goto error;
                    }
                    free(assessment_temp_path);
                    assessment_temp_path = (char *)malloc(strlen(generated) + 1);
                    if (assessment_temp_path == NULL) {
                        sixel_helper_set_additional_message(
                            "img2sixel: malloc() failed for assessment staging copy.");
                        status = SIXEL_BAD_ALLOCATION;
                        goto error;
                    }
                    strcpy(assessment_temp_path, generated);
                }
#endif
                status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE,
                                              assessment_temp_path);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
                sixel_output_path = (char *)malloc(
                    strlen(assessment_temp_path) + 1);
                if (sixel_output_path == NULL) {
                    sixel_helper_set_additional_message(
                        "img2sixel: malloc() failed for assessment staging name.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto error;
                }
                strcpy(sixel_output_path, assessment_temp_path);
        }
    }

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

    if (output_is_png) {
        png_temp_path = create_temp_template();
        if (png_temp_path == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: malloc() failed for PNG staging path.");
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
#if defined(HAVE_MKSTEMP)
        png_temp_fd = mkstemp(png_temp_path);
        if (png_temp_fd < 0) {
            sixel_helper_set_additional_message(
                "img2sixel: mkstemp() failed for PNG staging file.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
        (void) close(png_temp_fd);
        png_temp_fd = (-1);
#elif defined(HAVE__MKTEMP)
        if (_mktemp(png_temp_path) == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: _mktemp() failed for PNG staging file.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
#elif defined(HAVE_MKTEMP)
        if (mktemp(png_temp_path) == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: mktemp() failed for PNG staging file.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
#else
        {
            char *generated;

            generated = tmpnam(NULL);
            if (generated == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: tmpnam() failed for PNG staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto error;
            }
            free(png_temp_path);
            png_temp_path = (char *)malloc(strlen(generated) + 1);
            if (png_temp_path == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: malloc() failed for PNG staging path copy.");
                status = SIXEL_BAD_ALLOCATION;
                goto error;
            }
            strcpy(png_temp_path, generated);
        }
#endif

        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE,
                                      png_temp_path);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    assessment_parse_duration =
        sixel_assessment_timer_now() - assessment_parse_start;
    if (assessment_enabled) {
        status = sixel_allocator_new(&assessment_allocator,
                                     malloc,
                                     calloc,
                                     realloc,
                                     free);
        if (SIXEL_FAILED(status) || assessment_allocator == NULL) {
            goto error;
        }
        status = sixel_assessment_new(&assessment, assessment_allocator);
        if (SIXEL_FAILED(status) || assessment == NULL) {
            goto error;
        }
        sixel_assessment_select_sections(assessment, assessment_sections);
        sixel_assessment_record_stage_duration(
            assessment,
            SIXEL_ASSESSMENT_STAGE_ARGUMENT_PARSE,
            assessment_parse_duration);
        sixel_assessment_attach_encoder(assessment, encoder);
        if (argv != NULL && argv[0] != NULL) {
            status = sixel_assessment_setopt(assessment,
                                             SIXEL_ASSESSMENT_OPT_EXEC_PATH,
                                             argv[0]);
            if (SIXEL_FAILED(status)) {
                fprintf(stderr,
                        "img2sixel: warning: unable to locate models (%s).\n",
                        sixel_helper_format_error(status));
            }
        }
    }

    if (optind == argc) {
        status = sixel_encoder_encode(encoder, NULL);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else {
        for (n = optind; n < argc; n++) {
            status = sixel_encoder_encode(encoder, argv[n]);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }
    }

    if (output_is_png) {
        png_final_path = output_png_to_stdout ? "-" : png_output_path;
        if (!output_png_to_stdout && png_final_path == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: missing PNG output path.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
        status = write_png_from_sixel(png_temp_path, png_final_path);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (assessment_enabled) {
        if (assessment_allocator == NULL || assessment == NULL) {
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
        if (assessment_need_source_capture) {
            status = sixel_encoder_copy_source_frame(encoder,
                                                     &assessment_source_frame);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
            sixel_assessment_record_source_frame(assessment,
                                                 assessment_source_frame);
        }
        if (assessment_need_quality) {
            if (assessment_quality_quantized) {
                status = sixel_encoder_copy_quantized_frame(
                    encoder, assessment_allocator, &assessment_target_frame);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
                status = sixel_assessment_expand_quantized_frame(
                    assessment_target_frame,
                    assessment_allocator,
                    &assessment_expanded_frame);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
                sixel_frame_unref(assessment_target_frame);
                assessment_target_frame = assessment_expanded_frame;
                assessment_expanded_frame = NULL;
                sixel_assessment_record_quantized_capture(assessment, encoder);
            } else {
                status = sixel_assessment_load_single_frame(
                    sixel_output_path,
                    assessment_allocator,
                    &assessment_target_frame);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
            }
            if (!assessment_quality_quantized &&
                    assessment_need_quantized_capture) {
                sixel_assessment_record_quantized_capture(assessment, encoder);
            }
        } else if (assessment_need_quantized_capture) {
            sixel_assessment_record_quantized_capture(assessment, encoder);
        }
        if (assessment != NULL &&
                assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
            sixel_assessment_stage_transition(
                assessment,
                SIXEL_ASSESSMENT_STAGE_OUTPUT);
        }
        if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT) {
            status = copy_file_to_stream(assessment_temp_path,
                                         stdout,
                                         assessment);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
            if (assessment_forward_path == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: missing assessment spool target.");
                status = SIXEL_RUNTIME_ERROR;
                goto error;
            }
            assessment_forward_stream = fopen(assessment_forward_path, "wb");
            if (assessment_forward_stream == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: failed to open assessment spool sink.");
                status = SIXEL_LIBC_ERROR;
                goto error;
            }
            status = copy_file_to_stream(assessment_temp_path,
                                         assessment_forward_stream,
                                         assessment);
            if (fclose(assessment_forward_stream) != 0) {
                if (SIXEL_SUCCEEDED(status)) {
                    sixel_helper_set_additional_message(
                        "img2sixel: failed to close assessment spool sink.");
                    status = SIXEL_LIBC_ERROR;
                }
            }
            assessment_forward_stream = NULL;
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }
        if (assessment != NULL &&
                assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
            sixel_assessment_stage_finish(assessment);
        }
#if HAVE_SYS_STAT_H
        assessment_output_bytes = 0u;
        assessment_size_path = NULL;
        if (assessment_need_quality && !assessment_quality_quantized) {
            if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT ||
                    assessment_spool_mode ==
                        SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
                assessment_size_path = assessment_temp_path;
            } else if (sixel_output_path != NULL &&
                    strcmp(sixel_output_path, "-") != 0) {
                assessment_size_path = sixel_output_path;
            }
        } else {
            if (sixel_output_path != NULL &&
                    strcmp(sixel_output_path, "-") != 0) {
                assessment_size_path = sixel_output_path;
            } else if (assessment_spool_mode ==
                    SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT ||
                    assessment_spool_mode ==
                        SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
                assessment_size_path = assessment_temp_path;
            }
        }
        if (assessment_size_path != NULL) {
            assessment_stat_result = stat(assessment_size_path,
                                          &assessment_stat);
            if (assessment_stat_result == 0 &&
                    assessment_stat.st_size >= 0) {
                assessment_output_bytes =
                    (size_t)assessment_stat.st_size;
            }
        }
#else
        assessment_output_bytes = 0u;
#endif
        sixel_assessment_record_output_size(assessment,
                                            assessment_output_bytes);
        if (assessment_need_quality) {
            status = sixel_assessment_analyze(assessment,
                                              assessment_source_frame,
                                              assessment_target_frame);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
        }
        sixel_assessment_stage_finish(assessment);
        if (assessment_json_path != NULL &&
                strcmp(assessment_json_path, "-") != 0) {
            assessment_json_file = fopen(assessment_json_path, "wb");
            if (assessment_json_file == NULL) {
                sixel_helper_set_additional_message(
                    "img2sixel: failed to open assessment JSON file.");
                status = SIXEL_LIBC_ERROR;
                goto error;
            }
            assessment_json_owned = 1;
            assessment_sink.stream = assessment_json_file;
        } else {
            assessment_sink.stream = stdout;
        }
        assessment_sink.failed = 0;
        status = sixel_assessment_get_json(assessment,
                                           assessment_sections,
                                           assessment_json_callback,
                                           &assessment_sink);
        if (SIXEL_FAILED(status) || assessment_sink.failed) {
            sixel_helper_set_additional_message(
                "img2sixel: failed to emit assessment JSON.");
            goto error;
        }
    } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT) {
        status = copy_file_to_stream(assessment_temp_path,
                                     stdout,
                                     assessment);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
        if (assessment_forward_path == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: missing assessment spool target.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
        assessment_forward_stream = fopen(assessment_forward_path, "wb");
        if (assessment_forward_stream == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: failed to open assessment spool sink.");
            status = SIXEL_LIBC_ERROR;
            goto error;
        }
        status = copy_file_to_stream(assessment_temp_path,
                                     assessment_forward_stream,
                                     assessment);
        if (fclose(assessment_forward_stream) != 0) {
            if (SIXEL_SUCCEEDED(status)) {
                sixel_helper_set_additional_message(
                    "img2sixel: failed to close assessment spool sink.");
                status = SIXEL_LIBC_ERROR;
            }
        }
        assessment_forward_stream = NULL;
        if (SIXEL_FAILED(status)) {
            goto error;
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
    if (png_temp_path != NULL) {
        (void) unlink(png_temp_path);
    }
    free(png_temp_path);
    free(png_output_path);
    if (assessment_forward_stream != NULL) {
        (void) fclose(assessment_forward_stream);
    }
    if (assessment_temp_path != NULL &&
            assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
        (void) unlink(assessment_temp_path);
    }
    free(assessment_temp_path);
    free(assessment_forward_path);
    if (assessment_json_owned && assessment_json_file != NULL) {
        (void) fclose(assessment_json_file);
    }
    free(sixel_output_path);
    if (assessment_target_frame != NULL) {
        sixel_frame_unref(assessment_target_frame);
    }
    if (assessment_expanded_frame != NULL) {
        sixel_frame_unref(assessment_expanded_frame);
    }
    if (assessment_source_frame != NULL) {
        sixel_frame_unref(assessment_source_frame);
    }
    if (assessment != NULL) {
        sixel_assessment_unref(assessment);
    }
    if (assessment_allocator != NULL) {
        sixel_allocator_unref(assessment_allocator);
    }
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
