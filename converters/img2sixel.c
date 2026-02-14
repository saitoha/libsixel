/*
 * SPDX-License-Identifier: MIT
 *
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

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
#include "aborttrace.h"
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
static cli_option_help_t const g_option_help_table[] = {
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
        '=',
        "threads",
        "-= COUNT, --threads=COUNT|auto\n"
        "                           choose the encoder thread count.\n"
        "                           COUNT>=1 keeps deterministic order\n"
        "                           while values above one enable band\n"
        "                           parallelism. Use 'auto' to match\n"
        "                           the hardware thread count.\n"
    },
    {
        '.',
        "precision",
        "-., --precision=MODE\n"
        "                           control quantization precision.\n"
        "                             auto    -> honor the\n"
        "                                         SIXEL_FLOAT32_DITHER\n"
        "                                         environment (default).\n"
        "                             8bit    -> force the historical\n"
        "                                         integer pipeline.\n"
        "                             float32 -> request the\n"
        "                                         experimental\n"
        "                                         high-precision path.\n"
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
        'Q',
        "quantize-model",
        "-Q MODEL, --quantize-model=MODEL\n"
        "                           choose the palette solver.\n"
        "                             auto     -> choose quantize model\n"
        "                                         automatically (default)\n"
        "                                         auto maps to the heckbert\n"
        "                             heckbert -> traditional Heckbert\n"
        "                                         median-cut implementation.\n"
        "                             kmeans   -> k-means++ clustering.\n"
        "                               sub-option:\n"
        "                                 :inittype=TYPE (:i=TYPE)\n"
        "                                   choose k-means seed mode:\n"
        "                                     auto|none|pca\n"
        "                                 :threshold=VALUE (:t=VALUE)\n"
        "                                   stop refinement when delta\n"
        "                                   reaches VALUE (0.0-0.5).\n"
    },
    {
        'F',
        "final-merge",
        "-F MODE, --final-merge=MODE\n"
        "                           control the post-merge stage.\n"
        "                             auto -> choose post-merge strategy\n"
        "                                     automatically (default)\n"
        "                                     auto skips post-merge\n"
        "                                     reduction unless future\n"
        "                                     heuristics enable it.\n"
        "                             none -> skip post-merge reduction.\n"
        "                             ward -> merge clusters using\n"
        "                                     Ward's minimum variance\n"
        "                                     criterion.\n"
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
        "                             bluenoise -> tileable blue-noise\n"
        "                                         ordered dither\n"
        "                             lso2     -> libsixel method based on\n"
        "                                         variable error diffusion\n"
        "                                         tables, optimized for size\n"
    },
    {
        'y',
        "diffusion-scan",
        "-y SCANTYPE, --diffusion-scan=SCANTYPE\n"
        "                           choose scan order for diffusion\n"
        "                           SCANTYPE is one of them:\n"
        "                             auto       -> choose scan order\n"
        "                                           automatically (default)\n"
        "                             raster     -> left-to-right scan\n"
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
        "                             pca  -> split along the first\n"
        "                                     principal component and\n"
        "                                     cut at weighted median\n"
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
        "                             <number>c  -> scale width with\n"
        "                                           terminal cell count\n"
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
        "                             <number>c  -> scale height with\n"
        "                                           terminal cell count\n"
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
        '~',
        "lookup-policy",
        "-~ LOOKUPPOLICY, --lookup-policy=LOOKUPPOLICY\n"
        "                           choose histogram lookup width\n"
        "                           LOOKUPPOLICY is one of them:\n"
        "                             auto      -> follow pixel depth\n"
        "                             5bit      -> force classic 5-bit\n"
        "                                          buckets\n"
        "                             6bit      -> favor 6-bit RGB\n"
        "                                          buckets\n"
        "                             none      -> disable LUT caching\n"
        "                                          and scan directly\n"
        "                             certlut   -> certified hierarchical\n"
        "                                          lookup tree with\n"
        "                                          zero error\n"
        "                             eytzinger -> implicit binary tree\n"
        "                                             lookup with local\n"
        "                                             neighbour scan\n"
        "                                             (default)\n"
        "                             vpte      -> Voronoi grid built via\n"
        "                                          3D EDT with optional\n"
        "                                          boundary refinement\n"
        "                             vptree    -> VP-tree lookup built\n"
        "                                          from palette entries\n"
        "                             rbc       -> Random Ball Cover\n"
        "                                          cluster pruning\n"
        "                             mahalanobis -> RBC clusters with\n"
        "                                            Mahalanobis lower\n"
        "                                            bounds\n"
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
        "-v, --verbose              show debugging info and the planner DAG\n"
        "                           (DAG = Directed Acyclic Graph).\n"
    },
    {
        'L',
        "loaders",
        "-L LIST, --loaders=LIST    choose loader priority order\n"
        "                           LIST is a comma separated list of\n"
        "                           loader names (prefixes accepted).\n"
        "                           Append \"!\" to disable fallbacks.\n"
        "                           Use -H to list available loaders.\n"
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
        'X',
        "clustering-colorspace",
        "-X COLORSPACE, --clustering-colorspace=COLORSPACE\n"
        "                           choose palette clustering color space\n"
        "                             gamma  -> sRGB gamma(default)\n"
        "                             linear -> linear RGB color space\n"
        "                             oklab  -> OKLab color space\n"
        "                             cielab -> CIELAB color space\n"
        "                             din99d -> DIN99d color space\n"
    },
    {
        'W',
        "working-colorspace",
        "-W WORKING_COLORSPACE, --working-colorspace=WORKING_COLORSPACE\n"
        "                           choose internal working color space\n"
        "                             gamma  -> sRGB gamma(default)\n"
        "                             linear -> linear RGB color space\n"
        "                             oklab  -> OKLab color space\n"
        "                             cielab -> CIELAB color space\n"
        "                             din99d -> DIN99d color space\n"
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
        "-H, --help                 show this help and available loaders\n"
    }
};

/*
 * Environment variable help entries mirror the option table so we can
 * assemble the --help output without one gigantic literal.
 */
typedef struct {
    char const *name;
    char const *help;
} cli_env_help_t;

static cli_env_help_t const g_env_help_table[] = {
    {
        "SIXEL_OPTION_PREFIX_SUGGESTIONS",
        "toggle prefix disambiguation hints for ambiguous option values.\n"
        "Set to '1' (default) to print candidates or '0' to silence them."
    },
    {
        "SIXEL_OPTION_FUZZY_SUGGESTIONS",
        "toggle normalized Levenshtein suggestions after typos.\n"
        "Set to '1' (default) to show hints or '0' to disable them."
    },
    {
        "SIXEL_OPTION_PATH_SUGGESTIONS",
        "toggle filesystem diagnostics when a path cannot be resolved.\n"
        "Set to '1' to explain failures or '0' (default) to suppress them."
    },
    {
        "SIXEL_STATUS_FORCE_COLORS",
        "force ANSI colorized diagnostics from status markup output.\n"
        "Set to '1' to emit color sequences without TTY detection."
    },
    {
        "IMG2SIXEL_COMPLETION_BASH",
        "override the bash completion source path. When set, completion\n"
        "commands load this file before packaged defaults."
    },
    {
        "IMG2SIXEL_COMPLETION_ZSH",
        "override the zsh completion source path. Shares priority rules with\n"
        "the bash override."
    },
    {
        "IMG2SIXEL_COMPLETION_DIR",
        "provide a directory containing bash/img2sixel and zsh/_img2sixel\n"
        "entries. Consulted after shell-specific overrides."
    },
    {
        "IMG2SIXEL_COMPLETION_HOME",
        "fake the home directory during completion install/remove steps to\n"
        "test workflows in sandboxed locations."
    },
    {
        "IMG2SIXEL_BASH_VERSION_OVERRIDE",
        "override bash version parsing used by completion installers.\n"
        "mainly intended for tests that need deterministic legacy behavior."
    },
    {
        "SIXEL_ABORT_TRACE",
        "dump abort backtraces when img2sixel terminates abnormally.\n"
        "Defaults to auto (enabled). Set to 0/false/off to disable explicit\n"
        "traces; truthy values force them on."
    },
    {
        "SIXEL_NO_ABORT_TRACE",
        "legacy inverse toggle for abort tracing. Any non-zero/true value\n"
        "suppresses dumps while 0/false/off keeps tracing enabled."
    },
    {
        "SIXEL_BGCOLOR",
        "specify background color.\n"
        "overrided by -B(--bgcolor) option.\n"
        "represented by the following syntax:\n"
        "#rgb\n"
        "#rrggbb\n"
        "#rrrgggbbb\n"
        "#rrrrggggbbbb\n"
        "rgb:r/g/b\n"
        "rgb:rr/gg/bb\n"
        "rgb:rrr/ggg/bbb\n"
        "rgb:rrrr/gggg/bbbb"
    },
    {
        "SIXEL_COLORS",
        "specify palette size (default 256). Overrides -p/--colors when set."
    },
    {
        "SIXEL_FLOAT32_DITHER",
        "prefer the float32 quantization path. Any non-zero/true string\n"
        "enables it while 0, off, false, or no keep the 8-bit pipeline."
    },
    {
        "SIXEL_PLANNER_RESIZE_PRECISION_MODE",
        "force the resize precision planner. Accepts 1 (preserve integer\n"
        "buffers), 2 (linear float workspace), or 3 (float32 working\n"
        "colorspace). Defaults follow precision and working-colorspace\n"
        "choices."
    },
    {
        "SIXEL_THREADS",
        "override encoder thread count.\n"
        "Accepts positive integers or the word 'auto' to match the\n"
        "hardware thread count."
    },
    {
        "SIXEL_DITHER_PIN_THREADS",
        "pin pipeline worker threads to their initial CPUs (0 or 1;\n"
        "default 1)."
    },
    {
        "SIXEL_DITHER_PARALLEL_THREADS_MAX",
        "cap the number of dither workers when band parallelism is active.\n"
        "Accepts positive integers."
    },
    {
        "SIXEL_DITHER_PARALLEL_BAND_WIDTH",
        "override the band height assigned to each dither worker.\n"
        "Values are rounded to a multiple of six scanlines."
    },
    {
        "SIXEL_DITHER_PARALLEL_BAND_OVERWRAP",
        "set overlap between adjacent dither bands to smooth seams. Accepts\n"
        "non-negative integers."
    },
    {
        "SIXEL_DITHER_BLUENOISE_STRENGTH",
        "scale bluenoise ordered dither strength when -d bluenoise is set.\n"
        "Accepts 0.0-2.0; defaults to 0.055."
    },
    {
        "SIXEL_DITHER_A_DITHER_STRENGTH",
        "override positional a_dither strength. Defaults to 0.150 when\n"
        "unset."
    },
    {
        "SIXEL_DITHER_X_DITHER_STRENGTH",
        "override positional x_dither strength. Defaults to 0.100 when\n"
        "unset."
    },
    {
        "SIXEL_DITHER_BLUENOISE_PHASE",
        "phase offset for bluenoise tiles when -d bluenoise is set.\n"
        "Format is \"ox,oy\" with signed integers; defaults to 0,0."
    },
    {
        "SIXEL_DITHER_BLUENOISE_SEED",
        "seed used to derive a bluenoise phase when -d bluenoise is set and\n"
        "SIXEL_DITHER_BLUENOISE_PHASE is unset."
    },
    {
        "SIXEL_DITHER_BLUENOISE_CHANNEL",
        "choose bluenoise channel mode when -d bluenoise is set.\n"
        "Accepts mono (shared) or rgb (independent) samples; defaults mono."
    },
    {
        "SIXEL_DITHER_BLUENOISE_SIZE",
        "bluenoise tile size when -d bluenoise is set. Only 64 is embedded,\n"
        "other values fall back to 64."
    },
    {
        "SIXEL_SCALE_PARALLEL_MIN_BYTES",
        "delay parallel resize until the frame exceeds this byte threshold.\n"
        "Default 0 keeps eager threading."
    },
    {
        "SIXEL_PARALLEL_FACTOR",
        "override the row span assigned to each resize worker. Accepts\n"
        "positive integers."
    },
    {
        "SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS",
        "Defer RGBFLOAT32 colorspace fan-out until the frame reaches this\n"
        "pixel count. Defaults to 65537 so tiny frames stay\n"
        "single-threaded unless overridden."
    },
    {
        "SIXEL_PARALLEL_SKEW",
        "bias parallel decode spans by +/-20 percent so trailing workers\n"
        "take a larger share. Defaults to 0 (balanced)."
    },
    {
        "SIXEL_SIMD_LEVEL",
        "force SIMD selection. Accepts auto, none/scalar, sse2, avx, or\n"
        "neon."
    },
    {
        "SIXEL_THUMBNAILER_HINT_SIZE",
        "adjust the thumbnail target size used by loader helpers. Accepts\n"
        "positive integers; defaults to the built-in hint."
    },
    {
        "SIXEL_LOADER_PRIORITY_LIST",
        "Override default loader search order. Accepts the same comma\n"
        "separated names as -L. Ignored when -L/--loaders is provided."
    },
    {
        "SIXEL_PALETTE_SAMPLE_TARGET",
        "request a specific sample count for palette estimation. Positive\n"
        "integers override automatic sizing."
    },
    {
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        "Scale provisional palette size before the final merge. Accepts\n"
        "1.0-3.0, default 1.81."
    },
    {
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        "Repeat Lloyd refinement after the final merge. Accepts 0-30.\n"
        "Default is 3."
    },
    {
        "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX",
        "Cap Lloyd passes in the primary k-means solver. Accepts 1-30,\n"
        "default 20."
    },
    {
        "SIXEL_PALETTE_KMEANS_THRESHOLD",
        "Break condition for k-means refinement (0.0-0.5, default 0.125)."
    },
    {
        "SIXEL_PALETTE_KMEANS_INITTYPE",
        "choose k-means seed selection: auto, pca, or none (default auto)."
    },
    {
        "SIXEL_PALETTE_LUMIN_FACTOR_R",
        "Override the luminosity weighting for the red channel (0.0-1.0)."
    },
    {
        "SIXEL_PALETTE_LUMIN_FACTOR_G",
        "Override the luminosity weighting for the green channel\n"
        "(0.0-1.0). The blue factor becomes 1 - R - G; negative results\n"
        "ignore both overrides."
    },
    {
        "SIXEL_PALETTE_SNAP_TARGET_POLICY",
        "Control palette snap target search. Accepts 'reversible' for\n"
        "legacy fixed points or 'nearest'/'auto' for nearby fixed points\n"
        "in the working colorspace."
    },
    {
        "SIXEL_PALETTE_SNAP_TIMING_POLICY",
        "Decide when snaps run: 'once', 'polish', 'merge', 'resolve', or\n"
        "'all'. Defaults to 'once'."
    },
    {
        "SIXEL_PALETTE_SNAP_APPROACH_RATE",
        "Blend factor (0.0-1.0) toward the snap target; 1.0 snaps fully,\n"
        "lower values ease toward the target."
    },
    {
        "SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L",
        "Weight L* relative to a/b* when snapping in Lab-family\n"
        "colorspaces. Accepts 0.0-1.0, default 0.85."
    },
    {
        "SIXEL_PALETTE_CHANNEL_FACTOR_L",
        "emphasise L* weight in Lab-family distances before snapping\n"
        "(0.0-1.0)."
    },
    {
        "SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L",
        "emphasise L* weight during final merge distances independently of\n"
        "snap tuning (0.0-1.0)."
    },
    {
        "SIXEL_PALETTE_DISABLE_TABLES",
        "disable palette expansion lookup tables and exercise the\n"
        "shift-based fallback used for testing. Non-zero values skip table\n"
        "initialisation."
    },
    {
        "SIXEL_DITHER_LOOKUP_POLICY",
        "select palette lookup policy (auto, 5bit, 6bit, none, certlut,\n"
        "eytzinger, vpte, vptree, rbc, or mahalanobis; default is certlut)."
    },
    {
        "SIXEL_LOOKUP_PACKING",
        "choose dense LUT packing for 5bit/6bit policies\n"
        "(`linear`, `morton`, or `hilbert`; default `linear`)."
    },
    {
        "SIXEL_LOOKUP_VPTE_RESOLUTION",
        "choose VPTE grid resolution (64, 128, or 256; default 64)."
    },
    {
        "SIXEL_LOOKUP_VPTE_REFINE",
        "enable corner refinement on VPTE boundary cells (0 or 1; default 1)."
    },
    {
        "SIXEL_LOOKUP_VPTE_USE_DIST2",
        "reuse EDT distances to skip corner refinement (0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_VPTE_USE_CACHE",
        "enable a small VPTE lookup cache keyed by voxel coordinates\n"
        "(0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_VPTE_SHARED",
        "share the VPTE grid between workers after building it once\n"
        "(0 or 1; default 1)."
    },
    {
        "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE",
        "share the CERTLUT cache across threads (0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE",
        "share the 5bit LUT across threads without locks\n"
        "(0 or 1; default 1)."
    },
    {
        "SIXEL_LOOKUP_6BIT_SHARED_INSTANCE",
        "share the 6bit LUT across threads without locks\n"
        "(0 or 1; default 1)."
    },
    {
        "SIXEL_LOOKUP_VPTE_TILE_XY",
        "set VPTE tile width/height; adaptive defaults shrink tiles for\n"
        "diverse palettes and grow them for skewed palettes."
    },
    {
        "SIXEL_LOOKUP_VPTE_TILE_DEPTH",
        "set VPTE tile depth; follows the same adaptive policy as\n"
        "SIXEL_LOOKUP_VPTE_TILE_XY."
    },
    {
        "SIXEL_LOOKUP_VPTE_FIRST_TOUCH",
        "zero VPTE tiles before the EDT so NUMA systems can place pages on\n"
        "the worker that will consume them (0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_VPTE_PIN_THREADS",
        "pin VPTE worker threads at startup to reduce migration\n"
        "(0 or 1; default 0)."
    },
    {
        "SIXEL_VPTE_TILE_XY",
        "compatibility alias for SIXEL_LOOKUP_VPTE_TILE_XY. Shares the same\n"
        "semantics and adaptive defaults."
    },
    {
        "SIXEL_VPTE_TILE_DEPTH",
        "compatibility alias for SIXEL_LOOKUP_VPTE_TILE_DEPTH. Shares the\n"
        "same semantics and adaptive defaults."
    },
    {
        "SIXEL_VPTE_FIRST_TOUCH",
        "compatibility alias for SIXEL_LOOKUP_VPTE_FIRST_TOUCH. Same\n"
        "behaviour and defaults."
    },
    {
        "SIXEL_VPTE_PIN_THREADS",
        "compatibility alias for SIXEL_LOOKUP_VPTE_PIN_THREADS. Same\n"
        "behaviour and defaults."
    },
    {
        "SIXEL_LOG_PATH",
        "write a JSON timeline for VPTE or LUT builds when set."
    },
    {
        "SIXEL_LOG_LINES",
        "log every Nth line in the timeline; 0 disables line events."
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"img2sixel -H\" for more details.\n";

static size_t
img2sixel_option_help_count(void)
{
    return sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
}

static char const g_img2sixel_optstring[] =
    "o:"
    "=:"
    ".:"
    "L:786Rp:m:M:eb:Id:f:s:c:w:h:r:q:Q:F:~:kil:t:ugvSn:PE:U:B:C:D@:"
    "OVX:W:HY:y:";

static int
img2sixel_option_allows_leading_dash(int short_opt)
{
    /*
     * The options below accept file paths and we must not treat values that
     * start with '-' as missing arguments.  Users legitimately pass
     * "-p"-prefixed names through -o when they want files that happen to
     * look like short options.
     */
    if (short_opt == 'o') {
        return 1;
    }

    return 0;
}

static int
img2sixel_allows_leading_dash_cb(int short_opt, void *user_data)
{
    (void)user_data;

    return img2sixel_option_allows_leading_dash(short_opt);
}

static void img2sixel_report_missing_argument(int short_opt);

static void
img2sixel_report_missing_argument_callback(int short_opt, void *user_data)
{
    (void)user_data;

    img2sixel_report_missing_argument(short_opt);
}

static int
img2sixel_guard_missing_argument(int short_opt, char *const *argv)
{
    return cli_guard_missing_argument(short_opt,
                                      argv,
                                      optarg,
                                      &optind,
                                      g_img2sixel_optstring,
                                      g_option_help_table,
                                      img2sixel_option_help_count(),
                                      img2sixel_allows_leading_dash_cb,
                                      NULL,
                                      img2sixel_report_missing_argument_callback,
                                      NULL);
}

static void
img2sixel_print_option_help(FILE *stream)
{
    cli_print_option_help(stream,
                          g_option_help_table,
                          img2sixel_option_help_count());
}

static size_t
img2sixel_env_help_count(void)
{
    return sizeof(g_env_help_table) /
        sizeof(g_env_help_table[0]);
}

static void
img2sixel_print_env_help(FILE *stream)
{
    size_t name_width;
    size_t index;

    name_width = 27u;

    fprintf(stream, "Environment variables:\n");

    for (index = 0u; index < img2sixel_env_help_count(); ++index) {
        cli_env_help_t const *entry;
        char const *line_start;

        entry = &g_env_help_table[index];
        line_start = entry->help;

        while (line_start != NULL && line_start[0] != '\0') {
            char const *line_end;
            char const *name_field;
            size_t line_length;

            line_end = strchr(line_start, '\n');
            if (line_end == NULL) {
                line_length = strlen(line_start);
            } else {
                line_length = (size_t)(line_end - line_start);
            }

            if (line_start == entry->help) {
                name_field = entry->name;
            } else {
                name_field = "";
            }

            fprintf(stream,
                    "%-*s %.*s\n",
                    (int)name_width,
                    name_field,
                    (int)line_length,
                    line_start);

            if (line_end == NULL) {
                break;
            }

            line_start = line_end + 1u;
        }
    }
}

static void
img2sixel_print_clipboard_hint(void)
{
    fprintf(stderr,
            "The pseudo file \"clipboard:\" mirrors the desktop clipboard.\n"
            "Use \"clipboard:\" for SIXEL text, or prefix with \"png:\" or\n"
            "\"tiff:\" to request image snapshots.\n");
}

static size_t
img2sixel_utf8_expected_length(unsigned char byte)
{
    size_t length;

    length = 1u;

    if ((byte & 0x80u) == 0u) {
        length = 1u;
    } else if ((byte & 0xE0u) == 0xC0u) {
        length = 2u;
    } else if ((byte & 0xF0u) == 0xE0u) {
        length = 3u;
    } else if ((byte & 0xF8u) == 0xF0u) {
        length = 4u;
    }

    return length;
}

/*
 * Trim a byte length to a UTF-8 boundary so diagnostics never keep
 * half of a multibyte sequence when a preview is clipped.
 */
static size_t
img2sixel_utf8_trim_length(char const *text, size_t length)
{
    size_t index;
    size_t available;
    size_t required;
    unsigned char byte;

    index = length;
    available = 0u;
    required = 0u;
    byte = 0u;

    while (index > 0u) {
        byte = (unsigned char)text[index - 1u];
        if ((byte & 0xC0u) != 0x80u) {
            required = img2sixel_utf8_expected_length(byte);
            available = length - (index - 1u);
            if (available >= required) {
                return length;
            }
            length = index - 1u;
            index = length;
            continue;
        }
        --index;
    }

    return length;
}

static void
img2sixel_report_invalid_argument(int short_opt,
                                  char const *value,
                                  char const *detail)
{
    char buffer[2048];
    char detail_copy[2048];
    char argument_copy[1024];
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t argument_length;
    size_t argument_copy_length;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    memset(argument_copy, 0, sizeof(argument_copy));
    entry = cli_find_option_help(g_option_help_table,
                                 img2sixel_option_help_count(),
                                 short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    argument = (value != NULL && value[0] != '\0')
        ? value : "(missing)";
    argument_length = strlen(argument);
    argument_copy_length = argument_length;
    if (argument_copy_length >= sizeof(argument_copy)) {
        argument_copy_length = sizeof(argument_copy) - 1u;
        argument_copy_length = img2sixel_utf8_trim_length(argument,
                                                          argument_copy_length);
    }
    memcpy(argument_copy, argument, argument_copy_length);
    argument_copy[argument_copy_length] = '\0';
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "\\fW'%s'\\fP is invalid argument for "
                       "\\fB-%c\\fP,\\fB--%s\\fP option:\n\n",
                       argument_copy,
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
                           "%s\n",
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
    cli_report_missing_argument("img2sixel",
                                g_option_help_fallback,
                                g_option_help_table,
                                img2sixel_option_help_count(),
                                short_opt);
}

static void
img2sixel_report_unrecognized_option(int short_opt, char const *token)
{
    cli_report_unrecognized_option("img2sixel", short_opt, token);
}

static void
img2sixel_handle_getopt_error(int short_opt, char const *token)
{
    cli_option_help_t const *entry;
    cli_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = cli_find_option_help(g_option_help_table,
                                     img2sixel_option_help_count(),
                                     short_opt);
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
            long_entry = cli_find_option_help_by_long_name(
                g_option_help_table,
                img2sixel_option_help_count(),
                long_name);
            if (long_entry != NULL) {
                img2sixel_report_missing_argument(long_entry->short_opt);
                return;
            }
        }
    }

    img2sixel_report_unrecognized_option(short_opt, token);
}



/* output available loader names to the target stream */
static
void img2sixel_print_available_loaders(FILE *stream)
{
    size_t loader_count;
    char const **loader_names;
    size_t loader_index;

    loader_count = 0;
    loader_names = NULL;
    loader_index = 0;

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

    fprintf(stream, "available loaders:\n");
    if (loader_names != NULL && loader_count > 0) {
        for (loader_index = 0; loader_index < loader_count;
             ++loader_index) {
            fprintf(stream, "  %s\n", loader_names[loader_index]);
        }
    } else if (loader_count > 0) {
        fprintf(stream, "  (enumeration failed)\n");
    } else {
        fprintf(stream, "  (none)\n");
    }
    free(loader_names);
    fprintf(stream, "\n");
}

/* output version info to STDOUT */
static
void show_version(void)
{
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
           "  libwebp: "
#ifdef HAVE_WEBP
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
           "  CoreGraphics: "
#ifdef HAVE_COREGRAPHICS
           "yes\n"
#else
           "no\n"
#endif
           "\n"
          );

    img2sixel_print_available_loaders(stdout);
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
            "Special targets:\n"
            "  clipboard:             exchange data with the desktop clipboard.\n"
            "                         Prefix with png: or tiff: when writing to\n"
            "                         request image snapshots.\n"
            "\n");
    img2sixel_print_env_help(stdout);
    fprintf(stdout, "\n");
    img2sixel_print_available_loaders(stdout);
}


#if HAVE_SIGNAL

static int signaled = 0;

static void
signal_handler(int sig)
{
    signaled = sig;
}

#endif

static int
img2sixel_exit_code(SIXELSTATUS status)
{
    /*
     * Map SIXEL_STATUS to stable process exit codes.
     *
     * - 0: success.
     * - 1: generic runtime failure.
     * - 2: invalid arguments or usage.
     * - 3: clipboard-related failures.
     *
     * This avoids returning -1, which yields platform-dependent exit
     * codes (e.g., 255 on POSIX, 127 on Windows).
     */
    switch (status) {
    case SIXEL_OK:
        return 0;
    case SIXEL_BAD_ARGUMENT:
        return 2;
    case SIXEL_BAD_CLIPBOARD:
        return 3;
    default:
        return 1;
    }
}

int
main(int argc, char *argv[])
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    int exit_code;
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
        {"outfile",               required_argument,  &long_opt, 'o'},
        {"threads",               required_argument,  &long_opt, '='},
        {"precision",             required_argument,  &long_opt, '.'},
        {"7bit-mode",             no_argument,        &long_opt, '7'},
        {"8bit-mode",             no_argument,        &long_opt, '8'},
        {"gri-limit",             no_argument,        &long_opt, 'R'},
        {"6reversible",           no_argument,        &long_opt, '6'},
        {"colors",                required_argument,  &long_opt, 'p'},
        {"quantize-model",        required_argument,  &long_opt, 'Q'},
        {"final-merge",           required_argument,  &long_opt, 'F'},
        {"mapfile",               required_argument,  &long_opt, 'm'},
        {"mapfile-output",        required_argument,  &long_opt, 'M'},
        {"monochrome",            no_argument,        &long_opt, 'e'},
        {"high-color",            no_argument,        &long_opt, 'I'},
        {"builtin-palette",       required_argument,  &long_opt, 'b'},
        {"diffusion",             required_argument,  &long_opt, 'd'},
        {"diffusion-scan",        required_argument,  &long_opt, 'y'},
        {"diffusion-carry",       required_argument,  &long_opt, 'Y'},
        {"find-largest",          required_argument,  &long_opt, 'f'},
        {"select-color",          required_argument,  &long_opt, 's'},
        {"crop",                  required_argument,  &long_opt, 'c'},
        {"width",                 required_argument,  &long_opt, 'w'},
        {"height",                required_argument,  &long_opt, 'h'},
        {"resampling",            required_argument,  &long_opt, 'r'},
        {"quality",               required_argument,  &long_opt, 'q'},
        {"lookup-policy",         required_argument,  &long_opt, '~'},
        {"palette-type",          required_argument,  &long_opt, 't'},
        {"insecure",              no_argument,        &long_opt, 'k'},
        {"invert",                no_argument,        &long_opt, 'i'},
        {"loop-control",          required_argument,  &long_opt, 'l'},
        {"use-macro",             no_argument,        &long_opt, 'u'},
        {"ignore-delay",          no_argument,        &long_opt, 'g'},
        {"verbose",               no_argument,        &long_opt, 'v'},
        {"loaders",               required_argument,  &long_opt, 'L'},
        {"static",                no_argument,        &long_opt, 'S'},
        {"macro-number",          required_argument,  &long_opt, 'n'},
        {"penetrate",             no_argument,        &long_opt, 'P'}, /* deprecated */
        {"encode-policy",         required_argument,  &long_opt, 'E'},
        {"output-colorspace",     required_argument,  &long_opt, 'U'},
        {"clustering-colorspace", required_argument,  &long_opt, 'X'},
        {"working-colorspace",    required_argument,  &long_opt, 'W'},
        {"bgcolor",               required_argument,  &long_opt, 'B'},
        {"complexion-score",      required_argument,  &long_opt, 'C'}, /* deprecated */
        {"pipe-mode",             no_argument,        &long_opt, 'D'}, /* deprecated */
        {"drcs",                  required_argument,  &long_opt, '@'},
        {"ormode",                no_argument,        &long_opt, 'O'},
        {"version",               no_argument,        &long_opt, 'V'},
        {"help",                  no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */
    char detail_buffer[2048];
    char const *detail_source = NULL;
    int detail_limit;

    n = 0;
    completion_cli_result = 0;
    completion_exit_status = 0;
#if HAVE_GETOPT_LONG
    long_opt = 0;
    option_index = 0;
#endif  /* HAVE_GETOPT_LONG */

    sixel_tty_init_output_device(STDERR_FILENO);
    sixel_aborttrace_install_if_unhandled();

    optstring = g_img2sixel_optstring;
    completion_exit_status = 0;
    completion_cli_result = img2sixel_handle_completion_cli(
        argc, argv, &completion_exit_status);
    if (completion_cli_result != 0) {
        status = completion_exit_status;
        goto end;
    }

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_option_apply_cli_suggestion_defaults();

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
            goto unknown_option_error;

        default:
            status = sixel_encoder_setopt(encoder, n, optarg);
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
#endif
    if (optind >= argc) {
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

    /* mark as success */
    status = SIXEL_OK;
    goto end;

error:
    fprintf(stderr, "\n%s\n%s\n\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    if (status == SIXEL_BAD_CLIPBOARD) {
        img2sixel_print_clipboard_hint();
        fprintf(stderr, "\n");
    }
    goto end;

unknown_option_error:
    fprintf(stderr,
            "\n"
            "usage: img2sixel [-78eIkiugvSPDOVH] [-= threads] [-. precision] [-p colors] [-m file]\n"
            "                 [-d diffusiontype] [-Q model] [-F mode]\n"
            "                 [-y scantype]\n"
            "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
            "                 [-h height] [-r resamplingtype] [-q quality]\n"
            "                 [-~ lookuppolicy] [-l loopmode]\n"
            "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
            "                 [-E encodepolicy] [-L loaderlist]\n"
            "                 [-@ mmv:charset:path] [-1 shell] [-2 shell]\n"
            "                 [-3 shell] [-X clusteringcolorspace]\n"
            "                 [-W workingcolorspace] [-U outputcolorspace]\n"
            "                 [-B bgcolor] [-o outfile] [filename ...]\n\n"
            "for more details, type: 'img2sixel -H'.\n\n");
    goto end;

end:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    exit_code = img2sixel_exit_code(status);
    return exit_code;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
