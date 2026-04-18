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

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
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

const char *img2sixel_compat_getenv(const char *name);
int img2sixel_compat_setenv(const char *name, const char *value);
int img2sixel_trace_topic_is_enabled(char const *topic);
void img2sixel_trace_topic_message(const char *topic, const char *format, ...);

#if !defined(LIBSIXEL_OPTIONS_H)
/*
 * Keep the converter loosely coupled from private src/ headers while still
 * reusing the shared option-matching implementation from libsixel.
 */
typedef struct sixel_option_choice {
    char const *name;
    int value;
} sixel_option_choice_t;

typedef enum sixel_option_choice_result {
    SIXEL_OPTION_CHOICE_MATCH = 0,
    SIXEL_OPTION_CHOICE_AMBIGUOUS = 1,
    SIXEL_OPTION_CHOICE_NONE = 2
} sixel_option_choice_result_t;

sixel_option_choice_result_t
sixel_option_match_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size);

void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size);

void
sixel_option_report_invalid_choice(
    char const *base_message,
    char const *suggestions,
    char *buffer,
    size_t buffer_size);
#endif /* !defined(LIBSIXEL_OPTIONS_H) */

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
/*
 * The quantize-model section is long enough that adjacent literals exceed
 * the C99 minimum translation limit for string literal length.
 * Keep this warning suppression local to the help table.
 */
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Woverlength-strings"
#elif defined(__GNUC__) && !defined(__PCC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
static cli_option_help_t const g_option_help_table[] = {
    {
        'o',
        "outfile",
        "-o, --outfile\n"
        "    specify output file name. (default:stdout) Use a name ending in \".png\" or the literal\n"
        "    \"png:-\" or prefix a path with \"png:\" to emit PNG data recreated from the SIXEL\n"
        "    pipeline. The PNG keeps the palette indices so every color matches the quantized\n"
        "    SIXEL output exactly. \"png:-\" writes to stdout while \"png:<path>\" saves the PNG next\n"
        "    to the SIXEL output. Supplying \"-o output.png\" writes the PNG directly to that path.\n"
    },
    {
        '=',
        "threads",
        "-= COUNT, --threads=COUNT|auto\n"
        "    choose the encoder thread count. COUNT>=1 keeps deterministic order while values\n"
        "    above one enable band parallelism. Use 'auto' to match the hardware thread count.\n"
    },
    {
        '.',
        "precision",
        "-., --precision=MODE\n"
        "    control quantization precision.\n"
        "      auto    -> honor the SIXEL_FLOAT32_DITHER environment (default).\n"
        "      8bit    -> force the historical integer pipeline.\n"
        "      float32 -> request the experimental high-precision path.\n"
    },
    {
        '7',
        "7bit-mode",
        "-7, --7bit-mode\n"
        "    generate a sixel image for 7bit terminals or printers (default)\n"
    },
    {
        '8',
        "8bit-mode",
        "-8, --8bit-mode\n"
        "    generate a sixel image for 8bit terminals or printers\n"
    },
    {
        'R',
        "gri-limit",
        "-R, --gri-limit\n"
        "    limit arguments of DECGRI('!') to 255\n"
    },
    {
        '6',
        "6reversible",
        "-6, --6reversible\n"
        "    quantize via the SIXEL reversible tone set so decoding and re-encoding keeps the\n"
        "    palette stable; diffusion carries the small residuals.\n"
    },
    {
        'p',
        "colors",
        "-p COLORS, --colors=COLORS\n"
        "    specify number of colors to reduce the image to (default=256)\n"
    },
    {
        'Q',
        "quantize-model",
        "-Q MODEL, --quantize-model=MODEL\n"
        "    choose the palette solver.\n"
        "      auto     -> choose quantize model automatically (default) auto maps to the heckbert\n"
        "      heckbert -> traditional Heckbert median-cut implementation. auto/heckbert\n"
        "      sub-option:\n"
        "        :animation_mode=0|1 default 0\n"
        "        :scene_cut_threshold=VALUE 0.0-1.0 default 0.20\n"
        "        :merge=MODE (:g=MODE) auto, none, ward\n"
        "        :merge_oversplit=FACTOR (:o=FACTOR) 1.0-3.0\n"
        "        :merge_lloyd=COUNT (:l=COUNT) 0-30\n"
        "      kmeans   -> k-means clustering. sub-option:\n"
        "          :inittype=TYPE (:i=TYPE) choose k-means seed mode:\n"
        "              auto -> choose seed mode automatically (default)\n"
        "              none -> disable specialized seeding\n"
        "              pca  -> choose seeds from PCA axis\n"
        "          :threshold=VALUE (:t=VALUE) stop refinement when delta reaches VALUE (0.0-0.5).\n"
        "          :binning=MODE (:b=MODE) histogram pre-binning mode:\n"
        "              auto -> choose binning mode automatically (default)\n"
        "              none -> disable histogram pre-binning\n"
        "              hard -> hard-assignment histogram bins\n"
        "              soft -> trilinear soft histogram bins\n"
        "          :binbits=BITS (:n=BITS) histogram bits per channel (4-8, default 6).\n"
        "          :mapping=SPACE (:m=SPACE) histogram mapping space:\n"
        "              uniform -> use linear RGB cube mapping\n"
        "              srgb    -> use sRGB gamma-aware mapping\n"
        "          :softdist=KIND (:d=KIND) soft binning kernel:\n"
        "              trilinear -> trilinear kernel\n"
        "          :autoratio=RATIO (:r=RATIO) auto mode threshold ratio (1-1048576, default 32).\n"
        "          :feedback=MODE (:f=MODE) residual histogram feedback:\n"
        "              off -> disable feedback (default)\n"
        "              on  -> enable feedback\n"
        "          :prune=POLICY k-means pruning policy:\n"
        "              auto    -> choose Hamerly pruning (default)\n"
        "              none    -> disable pruning\n"
        "              hamerly -> use upper/lower-bound pruning\n"
        "              elkan   -> use per-center bound pruning\n"
        "              yinyang -> use grouped bound pruning\n"
        "          :seed=VALUE (:s=VALUE) uint32 random seed (0-4294967295).\n"
        "          :restarts=COUNT restart count (1-32, default 1).\n"
        "          :iter=COUNT Lloyd iteration cap (1-100). takes precedence over\n"
        "            :iter_max when both are specified.\n"
        "          :iter_max=COUNT Lloyd iteration cap (1-100, default 20).\n"
        "          :miniter=COUNT minimum iteration floor (0 or 1-100).\n"
        "          :polish_iter=COUNT extra post-iteration refinement count (0 or 1-16).\n"
        "          :feedback_slots=COUNT relocate this many weak clusters per feedback step (1-16,\n"
        "          default 1).\n"
        "          :feedback_interval=COUNT run feedback every N iterations (1-64, default 1).\n"
        "          :animation_mode=0|1 default 0\n"
        "          :scene_cut_threshold=VALUE 0.0-1.0 default 0.20\n"
        "          :merge=MODE (:g=MODE) auto, none, ward\n"
        "          :merge_oversplit=FACTOR (:o=FACTOR) 1.0-3.0\n"
        "          :merge_lloyd=COUNT (:l=COUNT) 0-30\n"
        "      medoids -> k-medoids clustering. sub-option:\n"
        "          :algo=NAME (:a=NAME) choose k-medoids solver:\n"
        "              auto      -> adaptive default (small PAM, mid CLARA, large BanditPAM)\n"
        "              pam       -> exhaustive swap search\n"
        "              sample    -> CLARA:\n"
        "                           PAM on subsamples\n"
        "              random    -> CLARANS:\n"
        "                           randomized neighbor search\n"
        "              bandit    -> BanditPAM:\n"
        "                           bandit swap pruning\n"
        "          :seed=VALUE (:s=VALUE) uint32 random seed (0-4294967295, default 1).\n"
        "          :iter=COUNT PAM iteration cap (1-64).\n"
        "          :sample=COUNT candidate cap hint (0 or 64-1048576).\n"
        "          :point_budget=COUNT candidate cap (64-16384). with :sample uses min(sample,\n"
        "          point_budget).\n"
        "          :histbits=BITS histogram bits/channel (3-6).\n"
        "          :rare_keep=COUNT keep low-frequency bins (0-1024).\n"
        "          :prune_mass=RATIO retain cumulative mass (0.900-1.000).\n"
        "          :auction=0|1 enable capacity-aware auction\n"
        "          reassignment (default 0).\n"
        "          :auction_shortlist=COUNT auction shortlist width\n"
        "          (2-8, default 4).\n"
        "          :clara_trials=COUNT CLARA trial count (1-32).\n"
        "          :clara_sample=COUNT CLARA sample size (0 or 64-1048576).\n"
        "          :clarans_local=COUNT CLARANS local searches (1-32).\n"
        "          :clarans_neighbors=COUNT CLARANS neighbor budget (0 or 1-5000000).\n"
        "          :bandit_iter=COUNT BanditPAM iteration cap (1-64).\n"
        "          :bandit_candidates=COUNT Bandit candidate budget (8-4096).\n"
        "          :bandit_batch=COUNT Bandit mini-batch size (8-4096).\n"
        "          :animation_mode=0|1 default 0\n"
        "          :scene_cut_threshold=VALUE 0.0-1.0 default 0.20\n"
        "          :merge=MODE (:g=MODE) auto, none, ward\n"
        "          :merge_oversplit=FACTOR (:o=FACTOR) 1.0-3.0\n"
        "          :merge_lloyd=COUNT (:l=COUNT) 0-30\n"
        "      center  -> discrete k-center clustering. sub-option:\n"
        "          :algo=NAME (:a=NAME) choose center solver:\n"
        "              auto   -> choose fft/hybrid from quality and sample budget\n"
        "              fft    -> Gonzalez farthest-first traversal\n"
        "              swap   -> local swap refinement from random seeds\n"
        "              hybrid -> fft initialization + swap refinement\n"
        "          :seed=VALUE (:s=VALUE) uint32 random seed (0-4294967295, default 1).\n"
        "          :restarts=COUNT restart count (1-32, default 1).\n"
        "          :iter=COUNT swap iteration cap (1-64, default 16).\n"
        "          :histbits=BITS histogram bits/channel (3-6, default 5).\n"
        "          :point_budget=COUNT candidate cap (0 or 64-16384, default 0=auto).\n"
        "          :prune_mass=RATIO retain cumulative mass (0.900-1.000,\n"
        "          default 0.995).\n"
        "          :animation_mode=0|1 default 0\n"
        "          :scene_cut_threshold=VALUE 0.0-1.0 default 0.20\n"
        "          :merge=MODE (:g=MODE) auto, none, ward\n"
        "          :merge_oversplit=FACTOR (:o=FACTOR) 1.0-3.0\n"
        "          :merge_lloyd=COUNT (:l=COUNT) 0-30\n"
    },
    {
        'm',
        "mapfile",
        "-m FILE, --mapfile=FILE\n"
        "    transform image colors to match this set of colors. Accepts image files and palette\n"
        "    files in ACT, PAL, and GPL formats. Use TYPE:PATH (act:, pal:, pal-jasc:, pal-riff:,\n"
        "    gpl:) to force a format. Without TYPE the extension or file contents decide. TYPE:-\n"
        "    reads palette bytes from stdin (for example, gpl:-).\n"
    },
    {
        'M',
        "mapfile-output",
        "-M FILE, --mapfile-output=FILE\n"
        "    export the computed palette. TYPE:PATH prefixes or file extensions (.act, .pal, .gpl)\n"
        "    choose the format. .pal defaults to JASC text; use pal-riff: for RIFF output. Writing\n"
        "    to '-' needs TYPE:PATH; TYPE:- sends the palette to stdout.\n"
    },
    {
        'e',
        "monochrome",
        "-e, --monochrome\n"
        "    output monochrome sixel image this option assumes the terminal background color is\n"
        "    black\n"
    },
    {
        'k',
        "insecure",
        "-k, --insecure\n"
        "    allow to connect to SSL sites without certs(enabled only when configured with a\n"
        "    supported network backend)\n"
    },
    {
        'i',
        "invert",
        "-i, --invert\n"
        "    assume the terminal background color is white, make sense only when -e option is\n"
        "    given\n"
    },
    {
        'I',
        "high-color",
        "-I, --high-color\n"
        "    output 15bpp sixel image\n"
    },
    {
        'u',
        "use-macro",
        "-u, --use-macro\n"
        "    use DECDMAC and DECINVM sequences to optimize GIF animation rendering\n"
    },
    {
        'n',
        "macro-number",
        "-n MACRONO, --macro-number=MACRONO\n"
        "    specify an number argument for DECDMAC and make terminal memorize SIXEL image. No\n"
        "    image is shown if this option is specified\n"
    },
    {
        'C',
        "complexion-score",
        "-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE\n"
        "    [[deprecated]] specify an number argument for the score of complexion correction.\n"
        "    COMPLEXIONSCORE must be 1 or more.\n"
    },
    {
        'g',
        "ignore-delay",
        "-g, --ignore-delay\n"
        "    render GIF animation without delay\n"
    },
    {
        'S',
        "static",
        "-S, --static\n"
        "    render animated GIF as a static image\n"
    },
    {
        'd',
        "diffusion",
        "-d DIFFUSION, --diffusion=DIFFUSION\n"
        "    choose diffusion method used with -p option (color reduction).\n"
        "    DIFFUSION is one of them:\n"
        "      auto       -> choose diffusion type automatically (default)\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      none       -> do not diffuse\n"
        "      fs         -> Floyd-Steinberg method\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      atkinson   -> Bill Atkinson's method\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      jajuni     -> Jarvis, Judice & Ninke\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      stucki     -> Stucki's method\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      burkes     -> Burkes' method\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      sierra     -> Sierra diffusion family\n"
        "        sub-option:\n"
        "          variant=LEVEL  -> 1, 2, or 3 (default 1)\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      a_dither   -> positionally stable arithmetic dither\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      x_dither   -> positionally stable arithmetic xor based dither\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      bluenoise  -> tileable blue-noise ordered dither\n"
        "        sub-option:\n"
        "          strength=VALUE      -> floating point amplitude\n"
        "                                 (default 0.055)\n"
        "          gradient_factor=VALUE\n"
        "                               -> gamma for gradient attenuation\n"
        "                                  (<=0 disables; default 0)\n"
        "          g=VALUE             -> short alias of gradient_factor\n"
        "          phase=X,Y           -> phase offsets for blue-noise tile\n"
        "          seed=SEED           -> phase seed when phase is omitted\n"
        "          channel=CHANNEL     -> mono or rgb (default mono)\n"
        "          size=SIZE           -> 64 (embedded tile size)\n"
        "          scan=SCANTYPE       -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      lso2       -> libsixel variable error diffusion tables\n"
        "        sub-option:\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default serpentine)\n"
        "      interframe -> interframe error diffusion (palette path only)\n"
        "        sub-option:\n"
        "          diffusion=KERNEL  -> auto, none, fs, atkinson, jajuni,\n"
        "                               stucki, burkes, sierra1, sierra2,\n"
        "                               or sierra3\n"
        "          scan=SCANTYPE      -> auto, raster, or serpentine\n"
        "                                 (default raster)\n"
        "      stbn       -> interframe STBN/PMJ source selection\n"
        "        sub-option:\n"
        "          source=SOURCE         -> hash, mask, or pmj (default hash)\n"
        "          diffusion=KERNEL      -> auto, none, fs, atkinson, jajuni,\n"
        "                                   stucki, burkes, sierra1, sierra2,\n"
        "                                   or sierra3 (default none)\n"
        "          strength=VALUE        -> float in 0.0-2.0 (default 0.055)\n"
        "          motion_adapt=0|1      -> enable motion adaptive strength\n"
        "                                   (default 0)\n"
        "          scene_cut_reset=0|1   -> reset carry at frame boundaries\n"
        "                                   (default 0)\n"
        "          scene_detect=0|1      -> reset on scene-change detection\n"
        "                                   (default 0)\n"
        "          alpha_guard=0|1       -> suppress stbn near alpha edges\n"
        "                                   (default 0)\n"
        "          perceptual_weight=0|1 -> apply perceptual channel weights\n"
        "                                   (default 0)\n"
        "          fastpath=0|1          -> enable pmj fast path (bit-exact)\n"
        "                                   (default 0)\n"
        "          scan=SCANTYPE         -> auto, raster, or serpentine\n"
        "                                   (default raster)\n"
    },
    {
        'f',
        "find-largest",
        "-f FINDTYPE, --find-largest=FINDTYPE\n"
        "    choose method for finding the largest dimension of median cut boxes for splitting,\n"
        "    make sense only when -p option (color reduction) is specified FINDTYPE is one of\n"
        "    them:\n"
        "      auto -> choose finding method automatically (default)\n"
        "      norm -> simply comparing the range in RGB space\n"
        "      lum  -> transforming into luminosities before the comparison\n"
        "      pca  -> split along the first principal component and cut at weighted median\n"
    },
    {
        's',
        "select-color",
        "-s SELECTTYPE, --select-color=SELECTTYPE\n"
        "    choose the method for selecting representative color from each median-cut box, make\n"
        "    sense only when -p option (color reduction) is specified SELECTTYPE is one of them:\n"
        "      auto      -> choose selecting method automatically (default)\n"
        "      center    -> choose the center of the box\n"
        "      average    -> calculate the color average into the box\n"
        "      histogram -> similar with average but considers color histogram\n"
    },
    {
        'c',
        "crop",
        "-c REGION, --crop=REGION\n"
        "    crop source image to fit the specified geometry. REGION should be formatted as\n"
        "    '%dx%d+%d+%d'\n"
    },
    {
        'w',
        "width",
        "-w WIDTH, --width=WIDTH\n"
        "    resize image to specified width WIDTH is represented by the following syntax\n"
        "                                 auto       -> preserving aspect ratio (default)\n"
        "                                 <number>%  -> scale width with given percentage\n"
        "                                 <number>   -> scale width with pixel counts\n"
        "                                 <number>c  -> scale width with terminal cell count\n"
        "                                 <number>px -> scale width with pixel counts\n"
    },
    {
        'h',
        "height",
        "-h HEIGHT, --height=HEIGHT\n"
        "    resize image to specified height HEIGHT is represented by the following syntax\n"
        "                                 auto       -> preserving aspect ratio (default)\n"
        "                                 <number>%  -> scale height with given percentage\n"
        "                                 <number>   -> scale height with pixel counts\n"
        "                                 <number>c  -> scale height with terminal cell count\n"
        "                                 <number>px -> scale height with pixel counts\n"
    },
    {
        'r',
        "resampling",
        "-r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE\n"
        "    choose resampling filter used with -w or -h option (scaling) RESAMPLINGTYPE is one of\n"
        "    them:\n"
        "      nearest  -> Nearest-Neighbor method\n"
        "      gaussian -> Gaussian filter\n"
        "      hanning  -> Hanning filter\n"
        "      hamming  -> Hamming filter\n"
        "      bilinear -> Bilinear filter (default)\n"
        "      welsh    -> Welsh filter\n"
        "      bicubic  -> Bicubic filter\n"
        "      lanczos2 -> Lanczos-2 filter\n"
        "      lanczos3 -> Lanczos-3 filter\n"
        "      lanczos4 -> Lanczos-4 filter\n"
    },
    {
        'q',
        "quality",
        "-q QUALITYMODE, --quality=QUALITYMODE\n"
        "    select quality of color quanlization.\n"
        "      auto -> decide quality mode automatically (default)\n"
        "      low  -> low quality and high speed mode\n"
        "      high -> high quality and low speed mode\n"
        "      full -> full quality and careful speed mode\n"
    },
    {
        '~',
        "lookup-policy",
        "-~ LOOKUPPOLICY, --lookup-policy=LOOKUPPOLICY\n"
        "    choose histogram lookup width LOOKUPPOLICY is one of them:\n"
        "      auto      -> follow pixel depth\n"
        "      5bit      -> force classic 5-bit buckets\n"
        "      6bit      -> favor 6-bit RGB buckets\n"
        "      none      -> disable LUT caching and scan directly\n"
        "      certlut   -> certified hierarchical lookup tree with zero error\n"
        "      eytzinger -> implicit binary tree lookup with local neighbour scan (default)\n"
        "      fhedt      -> Voronoi grid built via 3D EDT with optional boundary refinement\n"
        "      vptree    -> VP-tree lookup built from palette entries\n"
        "      rbc       -> Random Ball Cover cluster pruning\n"
        "      mahalanobis -> RBC clusters with Mahalanobis lower bounds\n"
    },
    {
        'l',
        "loop-control",
        "-l LOOPMODE, --loop-control=LOOPMODE\n"
        "    select loop control mode for GIF animation.\n"
        "      auto    -> honor the setting of GIF header (default)\n"
        "      force   -> always enable loop\n"
        "      disable -> always disable loop\n"
    },
    {
        'T',
        "start-frame",
        "-T FRAME_NO, --start-frame=FRAME_NO\n"
        "    set the animation start frame index for supported loaders. Positive values and zero\n"
        "    select from the beginning, negative values count from the end (-1 is the last frame).\n"
    },
    {
        't',
        "palette-type",
        "-t PALETTETYPE, --palette-type=PALETTETYPE\n"
        "    select palette color space type\n"
        "      auto -> choose palette type automatically (default)\n"
        "      hls  -> use HLS color space\n"
        "      rgb  -> use RGB color space\n"
    },
    {
        'b',
        "builtin-palette",
        "-b BUILTINPALETTE, --builtin-palette=BUILTINPALETTE\n"
        "    select built-in palette type\n"
        "      xterm16    -> X default 16 color map\n"
        "      xterm256   -> X default 256 color map\n"
        "      vt340mono  -> VT340 monochrome map\n"
        "      vt340color -> VT340 color map\n"
        "      gray1      -> 1bit grayscale map\n"
        "      gray2      -> 2bit grayscale map\n"
        "      gray4      -> 4bit grayscale map\n"
        "      gray8      -> 8bit grayscale map\n"
    },
    {
        'E',
        "encode-policy",
        "-E ENCODEPOLICY, --encode-policy=ENCODEPOLICY\n"
        "    select encoding policy\n"
        "      auto -> choose encoding policy automatically (default)\n"
        "      fast -> encode as fast as possible\n"
        "      size -> encode to as small sixel sequence as possible\n"
    },
    {
        'B',
        "bgcolor",
        "-B BGCOLOR, --bgcolor=BGCOLOR\n"
        "    specify background color BGCOLOR is represented by the following syntax #rgb #rrggbb\n"
        "    #rrrgggbbb #rrrrggggbbbb rgb:r/g/b rgb:rr/gg/bb rgb:rrr/ggg/bbb rgb:rrrr/gggg/bbbb\n"
    },
    {
        'P',
        "penetrate",
        "-P, --penetrate\n"
        "    [[deprecated]] penetrate GNU Screen using DCS pass-through sequence\n"
    },
    {
        'D',
        "pipe-mode",
        "-D, --pipe-mode\n"
        "    [[deprecated]] read source images from stdin continuously\n"
    },
    {
        'v',
        "verbose",
        "-v, --verbose\n"
        "    show debugging info and the planner DAG (DAG = Directed Acyclic Graph).\n"
    },
    {
        'L',
        "loaders",
        "-L LIST, --loaders=LIST\n"
        "    choose loader priority order LIST is a comma separated list of loader names (prefixes\n"
        "    accepted). libpng/libjpeg/libwebp/coregraphics support\n"
        "                               :orientation=on|off (or :o=..., default on).\n"
        "                               libpng/libjpeg/libwebp/libtiff/builtin support\n"
        "                               :cms_engine=none|auto|builtin|lcms2|colorsync (or :e=...,\n"
        "                               default none). WIC supports :ico_minsize=SIZE to choose\n"
        "                               the smallest ICO frame with edge >= SIZE. Append \"!\" to\n"
        "                               disable fallbacks. Use -H to list available loaders.\n"
    },
    {
        '#',
        "cms-engine",
        "-# ENGINE, --cms-engine=ENGINE\n"
        "    set default loader CMS backend for this process (SIXEL_LOADER_CMS_ENGINE).\n"
        "      none      -> disable loader CMS.\n"
        "      auto      -> prefer lcms2, then ColorSync (macOS), then builtin.\n"
        "      builtin   -> force builtin backend.\n"
        "      lcms2     -> force lcms2 backend.\n"
        "      colorsync -> force ColorSync backend.\n"
    },
    {
        '@',
        "drcs",
        "-@ MMV:CHARSET:PATH, --drcs=MMV:CHARSET:PATH\n"
        "    emit DRCS tiles instead of SIXEL output. MMV selects the mapping revision (0..2,\n"
        "    default 2). CHARSET chooses the slot (1-126 when MMV=0, 1-63 when MMV=1, 1-158 when\n"
        "    MMV=2; default 1). PATH routes tile data (\"-\" keeps stdout; blank disables the\n"
        "    external sink).\n"
    },
    {
        'O',
        "ormode",
        "-O, --ormode\n"
        "    enables sixel output in \"ormode\"\n"
    },
    {
        'X',
        "clustering-colorspace",
        "-X COLORSPACE, --clustering-colorspace=COLORSPACE\n"
        "    choose palette clustering color space\n"
        "      gamma  -> sRGB gamma(default)\n"
        "      linear -> linear RGB color space\n"
        "      oklab  -> OKLab color space\n"
        "      cielab -> CIELAB color space\n"
        "      din99d -> DIN99d color space\n"
    },
    {
        'W',
        "working-colorspace",
        "-W COLORSPACE, --working-colorspace=COLORSPACE\n"
        "    choose internal working color space\n"
        "      gamma  -> sRGB gamma(default)\n"
        "      linear -> linear RGB color space\n"
        "      oklab  -> OKLab color space\n"
        "      cielab -> CIELAB color space\n"
        "      din99d -> DIN99d color space\n"
    },
    {
        'U',
        "output-colorspace",
        "-U COLORSPACE, --output-colorspace=COLORSPACE\n"
        "    choose output color space\n"
        "      gamma   -> sRGB gamma(default)\n"
        "      linear  -> linear RGB color space\n"
        "      smpte-c -> SMPTE-C gamma color space\n"
    },
    {
        '1',
        "show-completion",
        "-1 TARGET, --show-completion=TARGET\n"
        "    print shell completion script TARGET is one of [bash, zsh, all]\n"
    },
    {
        '2',
        "install-completion",
        "-2 TARGET, --install-completion=TARGET\n"
        "    install shell completion script TARGET is one of [bash, zsh, all]\n"
    },
    {
        '3',
        "uninstall-completion",
        "-3 TARGET, --uninstall-completion=TARGET\n"
        "    uninstall shell completion script TARGET is one of [bash, zsh, all]\n"
    },
    {
        '%',
        "env",
        "-% KEY=VALUE, --env=KEY=VALUE\n"
        "    set process environment variable before conversion. Repeatable.\n"
    },
    {
        'V',
        "version",
        "-V, --version\n"
        "    show version and license info\n"
    },
    {
        'H',
        "help",
        "-H, --help\n"
        "    show this help and available loaders\n"
    }
};

#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__) && !defined(__PCC__)
# pragma GCC diagnostic pop
#endif

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
        "SIXEL_TRACE_TOPIC",
        "enable topic-scoped debug traces using comma-separated tokens.\n"
        "Example: 'path,suggestion' narrows diagnostics to those topics."
    },
    {
        "SIXEL_STATUS_FORCE_COLORS",
        "force ANSI colorized diagnostics from status markup output.\n"
        "Set to '1' to emit color sequences without TTY detection."
    },
    {
        "SIXEL_CLIPBOARD_BACKEND",
        "select clipboard backend. Set to 'system' (default) to use the\n"
        "desktop clipboard or 'file' to use a file-backed fake clipboard."
    },
    {
        "SIXEL_CLIPBOARD_FILE_DIR",
        "directory used by the fake clipboard backend when\n"
        "SIXEL_CLIPBOARD_BACKEND=file. The backend stores image/text payloads\n"
        "as files under this path."
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
        "SIXEL_LOADER_OSC11_BG_QUERY",
        "enable OSC11 background color probing in loaders.\n"
        "Only the exact value '1' enables probing. Defaults to off, but\n"
        "img2sixel sets it to '1' when the variable is unset.\n"
        "When OSC11 succeeds, background colorspace is forced to gamma."
    },
    {
        "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS",
        "max wait time in milliseconds for OSC11 background color probing.\n"
        "Defaults to 50. Invalid values fall back to 50."
    },
    {
        "SIXEL_ANIMATION_HIDE_CURSOR",
        "hide terminal cursor with DECTCEM during animated TTY output.\n"
        "Only the exact value '1' enables this behavior. Defaults to off,\n"
        "but img2sixel sets it to '1' when the variable is unset."
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
        "SIXEL_DITHER_STBN_SOURCE",
        "internal STBN source override used by -d stbn.\n"
        "Accepts stbn-hash (default), stbn-mask, or pmj."
    },
    {
        "SIXEL_DITHER_INTERFRAME_DIFFUSION",
        "set interframe spatial diffusion kernel for -d interframe.\n"
        "Accepts auto, none, fs, atkinson, jajuni, stucki, burkes,\n"
        "sierra1, sierra2, or sierra3.\n"
        "When unset, -d interframe defaults to fs."
    },
    {
        "SIXEL_DITHER_STBN_DIFFUSION",
        "set STBN spatial diffusion kernel for -d stbn.\n"
        "Accepts auto, none, fs, atkinson, jajuni, stucki, burkes,\n"
        "sierra1, sierra2, or sierra3.\n"
        "When unset, -d stbn defaults to none.\n"
        "Overridden by -d stbn:diffusion=KERNEL."
    },
    {
        "SIXEL_DITHER_STBN_STRENGTH",
        "set interframe STBN/PMJ noise strength for -d stbn.\n"
        "Accepts 0.0-2.0; defaults to 0.055.\n"
        "Overridden by -d stbn:strength=VALUE."
    },
    {
        "SIXEL_DITHER_STBN_MOTION_ADAPT",
        "toggle stbn motion adaptation for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:motion_adapt=0|1."
    },
    {
        "SIXEL_DITHER_STBN_SCENE_CUT_RESET",
        "toggle stbn scene-cut reset for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:scene_cut_reset=0|1."
    },
    {
        "SIXEL_DITHER_STBN_SCENE_DETECT",
        "toggle stbn scene detection for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:scene_detect=0|1."
    },
    {
        "SIXEL_DITHER_STBN_ALPHA_GUARD",
        "toggle stbn alpha-edge guard for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:alpha_guard=0|1."
    },
    {
        "SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT",
        "toggle stbn perceptual channel weighting for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:perceptual_weight=0|1."
    },
    {
        "SIXEL_DITHER_STBN_FASTPATH",
        "toggle stbn pmj fast path for -d stbn (0 or 1).\n"
        "Overridden by -d stbn:fastpath=0|1."
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
        "SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR",
        "set gradient attenuation gamma when -d bluenoise is set.\n"
        "Values <= 0 disable gradient modulation."
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
        "SIXEL_LOADER_BACKGROUND_COLORSPACE",
        "set how loader background colors are interpreted during alpha\n"
        "composition. Accepts gamma (default) or linear.\n"
        "Ignored when background color comes from OSC11 query."
    },
    {
        "SIXEL_BACKGROUND_POLICY",
        "choose background priority for builtin PNG/APNG/GIF composition.\n"
        "file_first (default) prefers file background over -B/SIXEL_BGCOLOR.\n"
        "explicit_first prefers -B/SIXEL_BGCOLOR over file background.\n"
        "Invalid or empty values fall back to file_first."
    },
    {
        "SIXEL_TRANSPARENT_POLICY",
        "control builtin non-PAL alpha normalization.\n"
        "composite (default) and transparent both composite semi-alpha when\n"
        "a background is available; transparent keeps alpha==0 source RGB.\n"
        "Invalid or empty values fall back to composite."
    },
    {
        "SIXEL_LOADER_ORIENTATION",
        "default EXIF orientation handling for libjpeg/libpng/libwebp.\n"
        "Accepts on/off (preferred) and 1/0 aliases. Defaults to on."
    },
    {
        "SIXEL_LOADER_LIBJPEG_ORIENTATION",
        "override EXIF orientation handling for libjpeg loader only.\n"
        "Accepts on/off and 1/0. Overrides SIXEL_LOADER_ORIENTATION."
    },
    {
        "SIXEL_LOADER_LIBPNG_ORIENTATION",
        "override EXIF orientation handling for libpng loader only.\n"
        "Accepts on/off and 1/0. Overrides SIXEL_LOADER_ORIENTATION."
    },
    {
        "SIXEL_LOADER_LIBWEBP_ORIENTATION",
        "override EXIF orientation handling for libwebp loader only.\n"
        "Accepts on/off and 1/0. Overrides SIXEL_LOADER_ORIENTATION."
    },
    {
        "SIXEL_LOADER_COREGRAPHICS_ORIENTATION",
        "override orientation handling for coregraphics loader only.\n"
        "Accepts on/off and 1/0. Overrides SIXEL_LOADER_ORIENTATION."
    },
    {
        "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES",
        "set total memory cap in bytes for CoreGraphics frame replay cache.\n"
        "Unset defaults to 67108864 (64 MiB). Set 0 to disable cache.\n"
        "Invalid values are rejected."
    },
    {
        "SIXEL_LOADER_ANIMATION_START_FRAME_NO",
        "set the animation start frame index used by supported loaders.\n"
        "0 or positive values are absolute indexes. Negative values count\n"
        "from the end (-1 is the last frame). Out-of-range indexes fail.\n"
        "Applied only on the first loop; later loops decode from frame 0."
    },
    {
        "SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES",
        "cap the total number of frames emitted by libwebp animation decode.\n"
        "Accepts positive integers; invalid or unset values use the built-in\n"
        "safety limit."
    },
    {
        "SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE",
        "force lossy WebP inputs through the legacy RGB decode path instead\n"
        "of the default YUV float32 pipeline. Intended for tests and\n"
        "regression debugging; values beginning with 1/y/t enable it."
    },
    {
        "SIXEL_LOADER_CMS_RENDERING_INTENT",
        "override the lcms2 rendering-intent fallback order used by PNG\n"
        "loader CMS conversion. Accepts comma-separated values from\n"
        "`perceptual`, `relative` (`relative_colorimetric`),\n"
        "`saturation`, and `absolute` (`absolute_colorimetric`).\n"
        "Append `!` to disable default fallback intents."
    },
    {
        "SIXEL_LOADER_CMS_ENGINE",
        "select default loader CMS backend. Accepts none, auto, builtin,\n"
        "lcms2, or colorsync. auto prefers lcms2, then ColorSync (macOS),\n"
        "then builtin. Overridden by -#/--cms-engine."
    },
    {
        "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        "override CMS backend for builtin loader conversion.\n"
        "Overrides SIXEL_LOADER_CMS_ENGINE."
    },
    {
        "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
        "override builtin BMP info40 ambiguity mode. Accepts auto,\n"
        "windows, or os2. auto keeps Windows behavior unless the tuple\n"
        "matches an OS/2-only compression pattern."
    },
    {
        "SIXEL_LOADER_LIBPNG_CMS_ENGINE",
        "override CMS backend for libpng loader conversion.\n"
        "Overrides SIXEL_LOADER_CMS_ENGINE."
    },
    {
        "SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR",
        "control PNG/APNG tRNS+alpha keycolor transparency path for builtin/libpng.\n"
        "Accepts only 0/1. Unset and 1 enable (default); 0 disables."
    },
    {
        "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES",
        "allow librsvg file-path decode so local relative external assets\n"
        "can resolve. Unset/0/off/false/no keep byte-only mode."
    },
    {
        "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ",
        "allow librsvg to decode gzip-compressed SVG from stdin/pipe by\n"
        "spooling input to a temporary .svgz file first."
    },
    {
        "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_OPEN",
        "test-only failpoint for SVGZ stdin spool path: force temporary\n"
        "file open failure before librsvg decode starts."
    },
    {
        "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_WRITE",
        "test-only failpoint for SVGZ stdin spool path: force temporary\n"
        "file write failure while buffering compressed SVG input."
    },
    {
        "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_CLOSE",
        "test-only failpoint for SVGZ stdin spool path: force temporary\n"
        "file close failure after buffered write completion."
    },
    {
        "SIXEL_LOADER_LIBJPEG_CMS_ENGINE",
        "override CMS backend for libjpeg loader conversion.\n"
        "Overrides SIXEL_LOADER_CMS_ENGINE."
    },
    {
        "SIXEL_LOADER_LIBWEBP_CMS_ENGINE",
        "override CMS backend for libwebp loader conversion.\n"
        "Overrides SIXEL_LOADER_CMS_ENGINE."
    },
    {
        "SIXEL_LOADER_LIBTIFF_CMS_ENGINE",
        "override CMS backend for libtiff loader conversion.\n"
        "Overrides SIXEL_LOADER_CMS_ENGINE."
    },
    {
        "SIXEL_LOADER_CMS_TARGET_COLORSPACE",
        "set loader CMS output colorspace after loader-side CMS conversion.\n"
        "Accepts gamma, linear, cielab, oklab, or din99d.\n"
        "Default is linear."
    },
    {
        "SIXEL_LOADER_HDR_FALLBACK_PROFILE",
        "set builtin HDR source-profile fallback when GAMMA/PRIMARIES are\n"
        "missing or unusable. Accepts linear-srgb (default) or srgb.\n"
        "Header-derived metadata takes precedence."
    },
    {
        "SIXEL_LOADER_HDR_EXPOSURE_EV",
        "apply HDR exposure in EV stops on builtin HDR decode path before\n"
        "quantization. Positive brightens, negative darkens. Default 0."
    },
    {
        "SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE",
        "control whether builtin HDR applies Radiance EXPOSURE metadata.\n"
        "Accepts 1/on/true/yes (default) or 0/off/false/no."
    },
    {
        "SIXEL_LOADER_HDR_TONEMAP",
        "apply HDR tone mapping on builtin HDR decode path. Accepts none\n"
        "(default) or reinhard."
    },
    {
        "SIXEL_LOADER_PREFER_8BIT",
        "when set to 1, keep loader CMS output in RGB888 instead of\n"
        "float32 target colorspaces."
    },
    {
        "SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII",
        "compatibility toggle for builtin PNM ASCII raster parsing.\n"
        "Set to 1 to accept truncated ASCII rasters and fill missing\n"
        "samples with legacy defaults. Unset keeps strict EOF errors."
    },
    {
        "SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
        "compatibility toggle for duplicate PAM required keys in P7.\n"
        "Set to 1 to allow duplicate WIDTH/HEIGHT/DEPTH/MAXVAL and use\n"
        "the last value. Unset rejects duplicates."
    },
    {
        "SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA",
        "compatibility toggle for trailing data after decoded PNM raster.\n"
        "Set to 1 to allow trailing bytes/comments after pixel data.\n"
        "Unset rejects trailing raster data in strict mode."
    },
    {
        "SIXEL_LOADER_PAM_ALLOW_ENDHDR_TRAILING_TOKENS",
        "compatibility toggle for trailing tokens on PAM ENDHDR line.\n"
        "Set to 1 to allow extra tokens after ENDHDR in P7 headers.\n"
        "Unset rejects unexpected tokens after ENDHDR."
    },
    {
        "SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER",
        "compatibility toggle for oversized PAM headers in P7.\n"
        "Set to 1 to allow headers larger than 64KiB or 1024 lines.\n"
        "Unset rejects oversized PAM headers in strict mode."
    },
    {
        "SIXEL_CMS_RENDERING_INTENT",
        "legacy alias for SIXEL_LOADER_CMS_RENDERING_INTENT. Used only when\n"
        "SIXEL_LOADER_CMS_RENDERING_INTENT is unset."
    },
    {
        "SIXEL_LOADER_WIC_ICO_MINSIZE",
        "default minimum edge size used by wic:ico_minsize when no\n"
        "suboption is provided. Accepts positive integers."
    },
    {
        "SIXEL_LODER_WIC_ICO_MINSIZE",
        "legacy alias for SIXEL_LOADER_WIC_ICO_MINSIZE."
    },
    {
        "SIXEL_PALETTE_SAMPLE_TARGET",
        "request a specific sample count for palette estimation. Positive\n"
        "integers override automatic sizing."
    },
    {
        "SIXEL_PALETTE_ANIMATION_MODE",
        "enable frame-to-frame palette stabilization for animated inputs.\n"
        "Accepts 0 or 1 (default 0)."
    },
    {
        "SIXEL_PALETTE_SCENE_CUT_THRESHOLD",
        "scene-cut sensitivity for animation_mode.\n"
        "Accepts 0.0-1.0 (default 0.20)."
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
        "Cap Lloyd passes in the primary k-means solver. Accepts 1-100,\n"
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
        "SIXEL_PALETTE_KMEANS_BINNING",
        "k-means histogram pre-binning mode: auto, none, hard, or soft.\n"
        "Default auto picks soft when sample density is high."
    },
    {
        "SIXEL_PALETTE_KMEANS_BINBITS",
        "k-means histogram bits per channel (4-8, default 6)."
    },
    {
        "SIXEL_PALETTE_KMEANS_MAPPING",
        "k-means histogram mapping space: uniform or srgb (default\n"
        "uniform)."
    },
    {
        "SIXEL_PALETTE_KMEANS_SOFTDIST",
        "k-means soft-binning kernel. Currently supports trilinear."
    },
    {
        "SIXEL_PALETTE_KMEANS_AUTORATIO",
        "k-means auto-binning density ratio threshold (1-1048576,\n"
        "default 32)."
    },
    {
        "SIXEL_PALETTE_KMEANS_FEEDBACK",
        "k-means residual histogram feedback switch: off or on\n"
        "(default off)."
    },
    {
        "SIXEL_PALETTE_KMEANS_PRUNE",
        "k-means pruning policy: auto, none, hamerly, elkan,\n"
        "or yinyang.\n"
        "Default auto maps to hamerly."
    },
    {
        "SIXEL_PALETTE_KMEANS_SEED",
        "default uint32 random seed for k-means (0-4294967295).\n"
        "when set, runs become reproducible."
    },
    {
        "SIXEL_PALETTE_KMEANS_RESTARTS",
        "k-means restart count (1-32, default 1)."
    },
    {
        "SIXEL_PALETTE_KMEANS_ITER",
        "force k-means Lloyd iteration cap (1-100).\n"
        "takes precedence over SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX."
    },
    {
        "SIXEL_PALETTE_KMEANS_MINITER",
        "set k-means minimum Lloyd iterations (0 or 1-100,\n"
        "default 0)."
    },
    {
        "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        "add post-pass k-means Lloyd refinements (0 or 1-16,\n"
        "default 0)."
    },
    {
        "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        "number of weak clusters moved per feedback step (1-16,\n"
        "default 1)."
    },
    {
        "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL",
        "feedback cadence in Lloyd iterations (1-64, default 1)."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_ALGO",
        "default k-medoids solver when -Q medoids omits :algo.\n"
        "Accepts auto, pam, sample, random, or bandit."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_SEED",
        "default uint32 random seed for k-medoids stochastic paths.\n"
        "Accepts 0-4294967295."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_ITER",
        "override k-medoids PAM iteration cap. Accepts 1-64."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_SAMPLE",
        "override k-medoids sample candidate hint.\n"
        "Accepts 0 or 64-1048576."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        "override CLARA trial count. Accepts 1-32."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        "override CLARA sample size. Accepts 0 or 64-1048576."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        "override CLARANS local-search count. Accepts 1-32."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS",
        "override CLARANS neighbor budget. Accepts 0 or 1-5000000."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        "override BanditPAM iteration cap. Accepts 1-64."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES",
        "override BanditPAM candidate budget. Accepts 8-4096."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        "override BanditPAM mini-batch size. Accepts 8-4096."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_HISTBITS",
        "histogram bits per channel for medoids preprocessing.\n"
        "Accepts 3-6, default 5."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        "override medoids candidate point cap. Accepts 64-16384.\n"
        "When SIXEL_PALETTE_KMEDOIDS_SAMPLE is also set, the solver uses\n"
        "min(sample, point_budget)."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP",
        "reserve low-frequency histogram bins before mass pruning.\n"
        "Accepts 0-1024, default 64."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS",
        "retain cumulative histogram mass before medoids solve.\n"
        "Accepts 0.900-1.000, default 0.995."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_AUCTION",
        "enable capacity-aware auction reassignment in medoids.\n"
        "Accepts 0 or 1, default 0."
    },
    {
        "SIXEL_PALETTE_KMEDOIDS_AUCTION_SHORTLIST",
        "set auction shortlist width per point in medoids.\n"
        "Accepts 2-8, default 4."
    },
    {
        "SIXEL_PALETTE_KCENTER_ALGO",
        "default k-center strategy when -Q center omits :algo.\n"
        "Accepts auto, fft, swap, or hybrid."
    },
    {
        "SIXEL_PALETTE_KCENTER_SEED",
        "default uint32 random seed for k-center stochastic paths.\n"
        "Accepts 0-4294967295."
    },
    {
        "SIXEL_PALETTE_KCENTER_RESTARTS",
        "k-center restart count (1-32, default 1)."
    },
    {
        "SIXEL_PALETTE_KCENTER_ITER",
        "k-center swap iteration cap (1-64, default 16)."
    },
    {
        "SIXEL_PALETTE_KCENTER_HISTBITS",
        "histogram bits per channel for k-center preprocessing.\n"
        "Accepts 3-6, default 5."
    },
    {
        "SIXEL_PALETTE_KCENTER_POINT_BUDGET",
        "override k-center candidate point cap.\n"
        "Accepts 0 or 64-16384. 0 keeps the internal auto budget."
    },
    {
        "SIXEL_PALETTE_KCENTER_PRUNE_MASS",
        "retain cumulative histogram mass before k-center solve.\n"
        "Accepts 0.900-1.000, default 0.995."
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
        "eytzinger, fhedt, vptree, rbc, or mahalanobis; default is certlut)."
    },
    {
        "SIXEL_LOOKUP_PACKING",
        "choose dense LUT packing for 5bit/6bit policies\n"
        "(`linear`, `morton`, or `hilbert`; default `linear`)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_RESOLUTION",
        "choose FHEDT grid resolution (64, 128, or 256; default 64)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_REFINE",
        "enable corner refinement on FHEDT boundary cells (0 or 1; default 1)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_USE_DIST2",
        "reuse EDT distances to skip corner refinement (0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_USE_CACHE",
        "enable a small FHEDT lookup cache keyed by voxel coordinates\n"
        "(0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_SHARED",
        "share the FHEDT grid between workers after building it once\n"
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
        "SIXEL_LOOKUP_FHEDT_TILE_XY",
        "set FHEDT tile width/height; adaptive defaults shrink tiles for\n"
        "diverse palettes and grow them for skewed palettes."
    },
    {
        "SIXEL_LOOKUP_FHEDT_TILE_DEPTH",
        "set FHEDT tile depth; follows the same adaptive policy as\n"
        "SIXEL_LOOKUP_FHEDT_TILE_XY."
    },
    {
        "SIXEL_LOOKUP_FHEDT_FIRST_TOUCH",
        "zero FHEDT tiles before the EDT so NUMA systems can place pages on\n"
        "the worker that will consume them (0 or 1; default 0)."
    },
    {
        "SIXEL_LOOKUP_FHEDT_PIN_THREADS",
        "pin FHEDT worker threads at startup to reduce migration\n"
        "(0 or 1; default 0)."
    },
    {
        "SIXEL_FHEDT_TILE_XY",
        "compatibility alias for SIXEL_LOOKUP_FHEDT_TILE_XY. Shares the same\n"
        "semantics and adaptive defaults."
    },
    {
        "SIXEL_FHEDT_TILE_DEPTH",
        "compatibility alias for SIXEL_LOOKUP_FHEDT_TILE_DEPTH. Shares the\n"
        "same semantics and adaptive defaults."
    },
    {
        "SIXEL_FHEDT_FIRST_TOUCH",
        "compatibility alias for SIXEL_LOOKUP_FHEDT_FIRST_TOUCH. Same\n"
        "behaviour and defaults."
    },
    {
        "SIXEL_FHEDT_PIN_THREADS",
        "compatibility alias for SIXEL_LOOKUP_FHEDT_PIN_THREADS. Same\n"
        "behaviour and defaults."
    },
    {
        "SIXEL_LOG_PATH",
        "write a JSON timeline for FHEDT or LUT builds when set."
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
    "L:#:786Rp:m:M:eb:Id:f:s:c:w:h:r:q:Q:~:kil:T:t:ugvSn:PE:U:B:C:D@:"
    "OVX:W:H%:1:2:3:";

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

static char *
img2sixel_ascii_lower_copy(char const *source)
{
    char *result;
    size_t index;
    size_t length;

    result = NULL;
    index = 0u;
    length = 0u;

    if (source == NULL) {
        return NULL;
    }

    length = strlen(source);
    result = (char *)malloc(length + 1u);
    if (result == NULL) {
        return NULL;
    }

    while (index < length) {
        result[index] = (char)tolower((unsigned char)source[index]);
        ++index;
    }
    result[length] = '\0';

    return result;
}

/*
 * Copy a diagnostic string into a fixed buffer while always keeping it
 * NUL-terminated. This avoids format-truncation warnings with -Werror.
 */
static void
img2sixel_copy_truncated(char *destination,
                         size_t destination_size,
                         char const *source)
{
    size_t copy_length;

    if (destination == NULL || destination_size == 0u) {
        return;
    }

    copy_length = 0u;
    if (source != NULL) {
        copy_length = strlen(source);
        if (copy_length >= destination_size) {
            copy_length = destination_size - 1u;
        }
        memcpy(destination, source, copy_length);
    }
    destination[copy_length] = '\0';
}

static int
img2sixel_safe_size_add(size_t *total, size_t addend)
{
    if (total == NULL) {
        return -1;
    }
    if (addend > SIZE_MAX - *total) {
        return -1;
    }
    *total += addend;
    return 0;
}

static void
img2sixel_format_invalid_argument_message(char *buffer,
                                          size_t buffer_size,
                                          int short_opt,
                                          char const *argument_text,
                                          char const *long_opt,
                                          char const *detail,
                                          char const *help_text)
{
    size_t offset;
    int written;

    offset = 0u;
    written = 0;
    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    if (argument_text == NULL) {
        argument_text = "(missing)";
    }
    if (long_opt == NULL) {
        long_opt = "?";
    }
    if (help_text == NULL) {
        help_text = "";
    }

    buffer[0] = '\0';
    written = snprintf(buffer,
                       buffer_size,
                       "\\fW'%s'\\fP is invalid argument for "
                       "\\fB-%c\\fP,\\fB--%s\\fP option:\n\n",
                       argument_text,
                       (char)short_opt,
                       long_opt);
    if (written < 0) {
        return;
    }
    if ((size_t)written >= buffer_size) {
        return;
    }
    offset = (size_t)written;

    if (detail != NULL && detail[0] != '\0') {
        written = snprintf(buffer + offset,
                           buffer_size - offset,
                           "%s\n\n",
                           detail);
        if (written < 0) {
            return;
        }
        if ((size_t)written >= buffer_size - offset) {
            return;
        }
        offset += (size_t)written;
    }

    if (offset >= buffer_size) {
        return;
    }
    (void)snprintf(buffer + offset,
                   buffer_size - offset,
                   "%s",
                   help_text);
}

enum {
    IMG2SIXEL_CMS_ENGINE_CHOICE_NONE = 0,
    IMG2SIXEL_CMS_ENGINE_CHOICE_AUTO = 1,
    IMG2SIXEL_CMS_ENGINE_CHOICE_BUILTIN = 2,
    IMG2SIXEL_CMS_ENGINE_CHOICE_LCMS2 = 3,
    IMG2SIXEL_CMS_ENGINE_CHOICE_COLORSYNC = 4
};

static sixel_option_choice_t const g_img2sixel_cms_engine_choices[] = {
    { "none", IMG2SIXEL_CMS_ENGINE_CHOICE_NONE },
    { "off", IMG2SIXEL_CMS_ENGINE_CHOICE_NONE },
    { "disabled", IMG2SIXEL_CMS_ENGINE_CHOICE_NONE },
    { "auto", IMG2SIXEL_CMS_ENGINE_CHOICE_AUTO },
    { "builtin", IMG2SIXEL_CMS_ENGINE_CHOICE_BUILTIN },
    { "lcms2", IMG2SIXEL_CMS_ENGINE_CHOICE_LCMS2 },
    { "lcms", IMG2SIXEL_CMS_ENGINE_CHOICE_LCMS2 },
    { "colorsync", IMG2SIXEL_CMS_ENGINE_CHOICE_COLORSYNC },
    { "color-sync", IMG2SIXEL_CMS_ENGINE_CHOICE_COLORSYNC }
};

static char const *
img2sixel_cms_engine_canonical_name(int choice_value)
{
    switch (choice_value) {
    case IMG2SIXEL_CMS_ENGINE_CHOICE_NONE:
        return "none";
    case IMG2SIXEL_CMS_ENGINE_CHOICE_AUTO:
        return "auto";
    case IMG2SIXEL_CMS_ENGINE_CHOICE_BUILTIN:
        return "builtin";
    case IMG2SIXEL_CMS_ENGINE_CHOICE_LCMS2:
        return "lcms2";
    case IMG2SIXEL_CMS_ENGINE_CHOICE_COLORSYNC:
        return "colorsync";
    default:
        break;
    }

    return NULL;
}

static int
img2sixel_apply_loader_cms_engine(char const *value,
                                  char *error_buffer,
                                  size_t error_buffer_size)
{
    char const *canonical_name;
    char const *detail_source;
    sixel_option_choice_result_t match_result;
    char match_detail[512];
    char *lower_value;
    int matched_choice;
    int status;

    canonical_name = NULL;
    detail_source = NULL;
    match_result = SIXEL_OPTION_CHOICE_NONE;
    match_detail[0] = '\0';
    lower_value = NULL;
    matched_choice = 0;
    status = 0;

    if (error_buffer != NULL && error_buffer_size > 0u) {
        error_buffer[0] = '\0';
    }

    lower_value = img2sixel_ascii_lower_copy(
        value != NULL ? value : "");
    if (lower_value == NULL) {
        if (error_buffer != NULL && error_buffer_size > 0u) {
            (void)snprintf(error_buffer,
                           error_buffer_size,
                           "failed to allocate temporary memory while "
                           "parsing cms-engine.");
        }
        return -1;
    }

    match_result = sixel_option_match_choice(
        lower_value,
        g_img2sixel_cms_engine_choices,
        sizeof(g_img2sixel_cms_engine_choices) /
            sizeof(g_img2sixel_cms_engine_choices[0]),
        &matched_choice,
        match_detail,
        sizeof(match_detail));
    free(lower_value);
    lower_value = NULL;

    if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
        sixel_option_report_ambiguous_prefix(
            value != NULL ? value : "",
            match_detail,
            error_buffer,
            error_buffer_size);
        if (error_buffer != NULL && error_buffer_size > 0u
            && error_buffer[0] == '\0') {
            detail_source = sixel_helper_get_additional_message();
            if (detail_source != NULL && detail_source[0] != '\0') {
                img2sixel_copy_truncated(error_buffer,
                                         error_buffer_size,
                                         detail_source);
            }
        }
        return -1;
    }
    if (match_result != SIXEL_OPTION_CHOICE_MATCH) {
        sixel_option_report_invalid_choice(
            "cms-engine accepts none, auto, builtin, lcms2, or colorsync.",
            match_detail,
            error_buffer,
            error_buffer_size);
        if (error_buffer != NULL && error_buffer_size > 0u
            && error_buffer[0] == '\0') {
            detail_source = sixel_helper_get_additional_message();
            if (detail_source != NULL && detail_source[0] != '\0') {
                img2sixel_copy_truncated(error_buffer,
                                         error_buffer_size,
                                         detail_source);
            }
        }
        return -1;
    }

    canonical_name = img2sixel_cms_engine_canonical_name(matched_choice);
    if (canonical_name == NULL) {
        if (error_buffer != NULL && error_buffer_size > 0u) {
            (void)snprintf(error_buffer,
                           error_buffer_size,
                           "internal error: unknown cms-engine choice.");
        }
        return -1;
    }

    status = img2sixel_compat_setenv("SIXEL_LOADER_CMS_ENGINE",
                                     canonical_name);
    if (status != 0) {
        if (error_buffer != NULL && error_buffer_size > 0u) {
            (void)snprintf(error_buffer,
                           error_buffer_size,
                           "failed to set environment variable "
                           "'SIXEL_LOADER_CMS_ENGINE'");
        }
        return -1;
    }

    return 0;
}

static void
img2sixel_report_invalid_argument(int short_opt,
                                  char const *value,
                                  char const *detail)
{
    char *buffer;
    char fallback_buffer[2048];
    char argument_copy[1024];
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t detail_length;
    size_t help_length;
    size_t header_size;
    size_t required_size;
    size_t argument_length;
    size_t argument_copy_length;

    buffer = NULL;
    memset(fallback_buffer, 0, sizeof(fallback_buffer));
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
    detail_length = 0u;
    help_length = strlen(help_text);
    header_size = 0u;
    required_size = 1u;
    argument_length = strlen(argument);
    argument_copy_length = argument_length;
    if (argument_copy_length >= sizeof(argument_copy)) {
        argument_copy_length = sizeof(argument_copy) - 1u;
        argument_copy_length = img2sixel_utf8_trim_length(argument,
                                                          argument_copy_length);
    }
    memcpy(argument_copy, argument, argument_copy_length);
    argument_copy[argument_copy_length] = '\0';

    if (detail != NULL) {
        detail_length = strlen(detail);
    }
    if (img2sixel_safe_size_add(&header_size,
                                sizeof("\\fW'") - 1u) != 0
        || img2sixel_safe_size_add(&header_size,
                                   argument_copy_length) != 0
        || img2sixel_safe_size_add(
               &header_size,
               sizeof("'\\fP is invalid argument for \\fB-") - 1u) != 0
        || img2sixel_safe_size_add(&header_size, 1u) != 0
        || img2sixel_safe_size_add(&header_size,
                                   sizeof("\\fP,\\fB--") - 1u) != 0
        || img2sixel_safe_size_add(&header_size,
                                   strlen(long_opt)) != 0
        || img2sixel_safe_size_add(&header_size,
                                   sizeof("\\fP option:\n\n") - 1u) != 0) {
        required_size = 0u;
    } else if (img2sixel_safe_size_add(&required_size,
                                       header_size) != 0
               || img2sixel_safe_size_add(&required_size,
                                          help_length) != 0) {
        required_size = 0u;
    } else if (detail_length > 0u
               && (img2sixel_safe_size_add(&required_size,
                                           detail_length) != 0
                   || img2sixel_safe_size_add(&required_size, 2u) != 0)) {
        required_size = 0u;
    }

    if (required_size > 0u) {
        buffer = (char *)malloc(required_size);
    }
    if (buffer != NULL) {
        img2sixel_format_invalid_argument_message(buffer,
                                                  required_size,
                                                  short_opt,
                                                  argument_copy,
                                                  long_opt,
                                                  detail,
                                                  help_text);
        sixel_helper_set_additional_message(buffer);
        free(buffer);
        return;
    }

    img2sixel_format_invalid_argument_message(fallback_buffer,
                                              sizeof(fallback_buffer),
                                              short_opt,
                                              argument_copy,
                                              long_opt,
                                              detail,
                                              help_text);
    sixel_helper_set_additional_message(fallback_buffer);
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
           "  libfetch: "
#ifdef HAVE_LIBFETCH
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

/*
 * Use sig_atomic_t storage for signal handlers and worker-thread polling.
 * The callback reads this flag from worker threads to stop looped output.
 */
static volatile sig_atomic_t signaled = 0;

static void
signal_handler(int sig)
{
    signaled = sig;
}

static int
signal_cancel_callback(void *context)
{
    (void)context;

    return signaled != 0;
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

typedef struct img2sixel_parsed_option {
    int code;
    int optopt_value;
    char const *argument;
    char const *token;
} img2sixel_parsed_option_t;

int
img2sixel_main(int argc, char *argv[])
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
        {"mapfile",               required_argument,  &long_opt, 'm'},
        {"mapfile-output",        required_argument,  &long_opt, 'M'},
        {"monochrome",            no_argument,        &long_opt, 'e'},
        {"high-color",            no_argument,        &long_opt, 'I'},
        {"builtin-palette",       required_argument,  &long_opt, 'b'},
        {"diffusion",             required_argument,  &long_opt, 'd'},
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
        {"start-frame",          required_argument,  &long_opt, 'T'},
        {"use-macro",             no_argument,        &long_opt, 'u'},
        {"ignore-delay",          no_argument,        &long_opt, 'g'},
        {"verbose",               no_argument,        &long_opt, 'v'},
        {"loaders",               required_argument,  &long_opt, 'L'},
        {"cms-engine",            required_argument,  &long_opt, '#'},
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
        {"env",                   required_argument,  &long_opt, '%'},
        {"show-completion",       required_argument,  &long_opt, '1'},
        {"install-completion",    required_argument,  &long_opt, '2'},
        {"uninstall-completion",  required_argument,  &long_opt, '3'},
        {"version",               no_argument,        &long_opt, 'V'},
        {"help",                  no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */
    char detail_buffer[2048];
    char const *detail_source = NULL;
    int detail_limit;
    img2sixel_parsed_option_t *parsed_options;
    img2sixel_parsed_option_t *grown_options;
    size_t parsed_count;
    size_t parsed_capacity;
    size_t parsed_index;
    int parse_unknown_option;
    int parse_terminal_optind;
    img2sixel_parsed_option_t current_option;

    n = 0;
    completion_cli_result = 0;
    completion_exit_status = 0;
#if HAVE_GETOPT_LONG
    long_opt = 0;
    option_index = 0;
#endif  /* HAVE_GETOPT_LONG */
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

    optstring = g_img2sixel_optstring;

    img2sixel_trace_topic_message("lifecycle",
                                 "main start: argc=%d",
                                 argc);

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
            grown_options = (img2sixel_parsed_option_t *)realloc(
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
        case '#':
            if (img2sixel_apply_loader_cms_engine(
                    parsed_options[parsed_index].argument,
                    detail_buffer,
                    sizeof(detail_buffer)) != 0) {
                img2sixel_report_invalid_argument(
                    '#',
                    parsed_options[parsed_index].argument,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : NULL);
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
        img2sixel_handle_getopt_error(
            parsed_options[parsed_count - 1u].optopt_value,
            parsed_options[parsed_count - 1u].token);
        status = SIXEL_BAD_ARGUMENT;
        goto unknown_option_error;
    }

    /*
     * img2sixel enables loader-side OSC11 probing by default only when the
     * variable is unset. User-provided values (including empty strings) win.
     */
    if (img2sixel_compat_getenv("SIXEL_LOADER_OSC11_BG_QUERY") == NULL) {
        if (img2sixel_compat_setenv("SIXEL_LOADER_OSC11_BG_QUERY",
                                    "1") != 0) {
            sixel_helper_set_additional_message(
                "failed to set environment variable "
                "'SIXEL_LOADER_OSC11_BG_QUERY'.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
    }

    /*
     * Enable DECTCEM cursor hide for animated tty output only when the user
     * did not configure the variable.
     */
    if (img2sixel_compat_getenv("SIXEL_ANIMATION_HIDE_CURSOR") == NULL) {
        if (img2sixel_compat_setenv("SIXEL_ANIMATION_HIDE_CURSOR",
                                    "1") != 0) {
            sixel_helper_set_additional_message(
                "failed to set environment variable "
                "'SIXEL_ANIMATION_HIDE_CURSOR'.");
            status = SIXEL_RUNTIME_ERROR;
            goto error;
        }
    }

    status = sixel_encoder_new(&encoder, NULL);
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
        case '#':
            break;
        case '?':
            img2sixel_handle_getopt_error(
                parsed_options[parsed_index].optopt_value,
                parsed_options[parsed_index].token);
            status = SIXEL_BAD_ARGUMENT;
            goto unknown_option_error;
        case '1':
        case '2':
        case '3':
            completion_exit_status = 0;
            completion_cli_result =
                img2sixel_handle_completion_option(n,
                                                   parsed_options[
                                                       parsed_index].argument,
                                                   &completion_exit_status);
            if (completion_cli_result <= 0) {
                status = SIXEL_FALSE;
                goto end;
            }
            status = SIXEL_OK;
            goto end;

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
    optind = parse_terminal_optind;

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
    status = sixel_encoder_set_cancel_callback(encoder,
                                               signal_cancel_callback,
                                               NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
#endif
    img2sixel_trace_topic_message("lifecycle",
                                 "encoding dispatch: optind=%d argc=%d",
                                 optind,
                                 argc);
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
    img2sixel_trace_topic_message("lifecycle",
                                 "encode completed successfully");
    goto end;

error:
    img2sixel_trace_topic_message("lifecycle",
                                 "enter error path: status=%d",
                                 status);
    fprintf(stderr, "\n%s\n%s\n\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    if (status == SIXEL_BAD_CLIPBOARD) {
        img2sixel_print_clipboard_hint();
        fprintf(stderr, "\n");
    }
    goto end;

unknown_option_error:
    img2sixel_trace_topic_message("lifecycle",
                                 "enter unknown-option path");
    fprintf(stderr,
            "\n"
            "usage: img2sixel [-78eIkiugvSPDOVH] [-= threads] [-. precision] [-p colors] [-m file]\n"
            "                 [-d diffusiontype] [-Q model]\n"
            "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
            "                 [-h height] [-r resamplingtype] [-q quality]\n"
            "                 [-~ lookuppolicy] [-l loopmode]\n"
            "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
            "                 [-E encodepolicy] [-L loaderlist] [-# cmsengine]\n"
            "                 [-@ mmv:charset:path] [-1 shell] [-2 shell]\n"
            "                 [-3 shell] [-X clusteringcolorspace]\n"
            "                 [-W workingcolorspace] [-U outputcolorspace]\n"
            "                 [-B bgcolor] [-o outfile] [filename ...]\n\n"
            "for more details, type: 'img2sixel -H'.\n\n");
    goto end;

end:
    img2sixel_trace_topic_message("lifecycle",
                                 "begin cleanup: status=%d",
                                 status);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (parsed_options != NULL) {
        free(parsed_options);
        parsed_options = NULL;
    }
    exit_code = img2sixel_exit_code(status);
    img2sixel_trace_topic_message("lifecycle",
                                 "main return: exit_code=%d",
                                 exit_code);
    return exit_code;
}

#if !defined(LIBSIXEL_IMG2SIXEL_EMBEDDED)
int
main(int argc, char *argv[])
{
    return img2sixel_main(argc, argv);
}
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
