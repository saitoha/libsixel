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

#if defined(HAVE_MKSTEMP)
int mkstemp(char *);
#endif

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

static char *
create_png_temp_template(void)
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

    /*
     * MinGW promotes snprintf's size argument to int, so allowing the
     * directory to grow beyond INT_MAX would reintroduce the truncation
     * warning seen in GCC.  Bail out when the directory is so long that the
     * resulting template would exceed that upper bound.
     */
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
 * The converter forms a round-trip pipeline when PNG output is requested.
 *
 *     +-------------+     +--------------------+     +-------------+
 *     | source load | --> | sixel encoder temp | --> | PNG decoder |
 *     +-------------+     +--------------------+     +-------------+
 *
 * Keeping the decoded stage inside libsixel guarantees that the PNG image
 * reproduces the post-quantized SIXEL output exactly.
 */
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
            "Options:\n"
            "-o, --outfile              specify output file name.\n"
            "                           (default:stdout)\n"
            "-T PATH, --tiles=PATH      specify output path for DRCS-SIXEL\n"
            "                           tile characters.\n"
            "                           use '-' to write to stdout.\n"
            "-7, --7bit-mode            generate a sixel image for 7bit\n"
            "                           terminals or printers (default)\n"
            "-8, --8bit-mode            generate a sixel image for 8bit\n"
            "                           terminals or printers\n"
            "-R, --gri-limit            limit arguments of DECGRI('!') to 255\n"
            "-p COLORS, --colors=COLORS specify number of colors to reduce\n"
            "                           the image to (default=256)\n"
            "-m FILE, --mapfile=FILE    transform image colors to match this\n"
            "                           set of colorsspecify map\n"
            "-e, --monochrome           output monochrome sixel image\n"
            "                           this option assumes the terminal\n"
            "                           background color is black\n"
            "-k, --insecure             allow to connect to SSL sites without\n"
            "                           certs(enabled only when configured\n"
            "                           with --with-libcurl)\n"
            "-i, --invert               assume the terminal background color\n"
            "                           is white, make sense only when -e\n"
            "                           option is given\n"
            "-I, --high-color           output 15bpp sixel image\n"
            "-u, --use-macro            use DECDMAC and DECINVM sequences to\n"
            "                           optimize GIF animation rendering\n"
            "-n MACRONO, --macro-number=MACRONO\n"
            "                           specify an number argument for\n"
            "                           DECDMAC and make terminal memorize\n"
            "                           SIXEL image. No image is shown if\n"
            "                           this option is specified\n"
            "-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE\n"
            "                           [[deprecated]] specify an number\n"
            "                           argument for the score of\n"
            "                           complexion correction.\n"
            "                           COMPLEXIONSCORE must be 1 or more.\n"
            "-g, --ignore-delay         render GIF animation without delay\n"
            "-S, --static               render animated GIF as a static image\n"
            );
    fprintf(stdout,
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
            "-y SCANTYPE, --diffusion-scan=SCANTYPE\n"
            "                           choose scan order for diffusion\n"
            "                           SCANTYPE is one of them:\n"
            "                             auto -> choose scan order\n"
            "                                     automatically\n"
            "                             raster -> left-to-right scan\n"
            "                             serpentine -> alternate direction\n"
            "                                           on each line\n"
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
            );
    fprintf(stdout,
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
            "-c REGION, --crop=REGION   crop source image to fit the\n"
            "                           specified geometry. REGION should\n"
            "                           be formatted as '%%dx%%d+%%d+%%d'\n"
            "-w WIDTH, --width=WIDTH    resize image to specified width\n"
            "                           WIDTH is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale width with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale width with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale width with\n"
            "                                           pixel counts\n"
            );
    fprintf(stdout,
            "-h HEIGHT, --height=HEIGHT resize image to specified height\n"
            "                           HEIGHT is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale height with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale height with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale height with\n"
            "                                           pixel counts\n"
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
            "-l LOOPMODE, --loop-control=LOOPMODE\n"
            "                           select loop control mode for GIF\n"
            "                           animation.\n"
            "                             auto    -> honor the setting of\n"
            "                                        GIF header (default)\n"
            "                             force   -> always enable loop\n"
            "                             disable -> always disable loop\n"
            );
    fprintf(stdout,
            "-t PALETTETYPE, --palette-type=PALETTETYPE\n"
            "                           select palette color space type\n"
            "                             auto -> choose palette type\n"
            "                                     automatically (default)\n"
            "                             hls  -> use HLS color space\n"
            "                             rgb  -> use RGB color space\n"
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
            "-E ENCODEPOLICY, --encode-policy=ENCODEPOLICY\n"
            "                           select encoding policy\n"
            "                             auto -> choose encoding policy\n"
            "                                     automatically (default)\n"
            "                             fast -> encode as fast as possible\n"
            "                             size -> encode to as small sixel\n"
            "                                     sequence as possible\n"
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
            "-P, --penetrate            [[deprecated]] penetrate GNU Screen\n"
            "                           using DCS pass-through sequence\n"
            "-D, --pipe-mode            [[deprecated]] read source images from\n"
            "                           stdin continuously\n"
            "-v, --verbose              show debugging info\n"
            "-J LIST, --loaders=LIST    choose loader priority order\n"
            "                           LIST is a comma separated set of\n"
            "                           loader names like 'gd,builtin'\n"
            "-@ DSCS, --drcs DSCS       output extended DRCS tiles instead of regular\n"
            "                           SIXEL image (experimental)\n"
            "-M VERSION, --mapping-version=VERSION\n"
            "                           specify DRCS-SIXEL Unicode mapping version\n"
            "-O, --ormode               enables sixel output in \"ormode\"\n"
            "-W WORKING_COLORSPACE, --working-colorspace=WORKING_COLORSPACE\n"
            "                           choose internal working color space\n"
            "                             gamma  -> sRGB gamma(default)\n"
            "                             linear -> linear RGB color space\n"
            "                             oklab  -> OKLab color space\n"
            "-U OUTPUT_COLORSPACE, --output-colorspace=OUTPUT_COLORSPACE\n"
            "                           choose output color space\n"
            "                             gamma   -> sRGB gamma(default)\n"
            "                             linear  -> linear RGB color space\n"
            "                             smpte-c -> SMPTE-C gamma color space\n"
            "-V, --version              show version and license info\n"
            "-H, --help                 show this help\n"
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
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    char const *optstring = "o:T:78Rp:m:eb:Id:f:s:c:w:h:r:q:L:kil:t:ugvSn:PE:U:B:C:D@:M:OJ:VW:HY:y:";
#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"outfile",            required_argument,  &long_opt, 'o'},
        {"tiles",              required_argument,  &long_opt, 'T'},
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
        {"loaders",            required_argument,  &long_opt, 'J'},
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
        {"mapping-version",    required_argument,  &long_opt, 'M'},
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

    output_is_png = 0;
    output_png_to_stdout = 0;
    png_output_path = NULL;
    png_temp_path = NULL;
    png_temp_fd = (-1);
    png_final_path = NULL;

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
        case 'o':
            if (is_png_target(optarg)) {
                output_is_png = 1;
                output_png_to_stdout = (strcmp(optarg, "png:-") == 0);
                free(png_output_path);
                png_output_path = NULL;
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
                status = sixel_encoder_setopt(encoder, n, optarg);
                if (SIXEL_FAILED(status)) {
                    goto argerr;
                }
            }
            break;
        default:
            status = sixel_encoder_setopt(encoder, n, optarg);
            if (SIXEL_FAILED(status)) {
                goto argerr;
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
#else
    (void) signal_handler;
#endif

    if (output_is_png) {
        png_temp_path = create_png_temp_template();
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

    /* mark as success */
    status = SIXEL_OK;
    goto end;

argerr:
    fprintf(stderr,
            "usage: img2sixel [-78eIkiugvSPDOVH] [-p colors] [-m file] [-d diffusiontype]\n"
            "                 [-y scantype]\n"
            "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
            "                 [-h height] [-r resamplingtype] [-q quality] [-l loopmode]\n"
            "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
            "                 [-E encodepolicy] [-J loaderlist] [-@ dscs]\n"
            "                 [-M mapping-version]\n"
            "                 [-W workingcolorspace] [-U outputcolorspace] [-B bgcolor]\n"
            "                 [-T path] [-o outfile] [filename ...]\n"
            "for more details, type: 'img2sixel -H'.\n");

error:
    fprintf(stderr, "%s\n%s\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    status = (-1);
end:
    if (png_temp_path != NULL) {
        (void) unlink(png_temp_path);
    }
    free(png_temp_path);
    free(png_output_path);
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
