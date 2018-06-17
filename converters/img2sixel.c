/*
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
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
#endif
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#include <sixel.h>

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
           "\n"
           "Copyright (C) 2014-2018 Hayaki Saito <saitoha@me.com>.\n"
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
            "-u, --use-macro            use DECDMAC and DEVINVM sequences to\n"
            "                           optimize GIF animation rendering\n"
            "-n MACRONO, --macro-number=MACRONO\n"
            "                           specify an number argument for\n"
            "                           DECDMAC and make terminal memorize\n"
            "                           SIXEL image. No image is shown if\n"
            "                           this option is specified\n"
            "-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE\n"
            "                           specify an number argument for the\n"
            "                           score of complexion correction.\n"
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
            "                             a_dither -> positionally stable\n"
            "                                         arithmetic dither\n"
            "                             x_dither -> positionally stable\n"
            "                                         arithmetic xor based dither\n"
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
            "-P, --penetrate            penetrate GNU Screen using DCS\n"
            "                           pass-through sequence\n"
            "-D, --pipe-mode            [[deprecated]] read source images from\n"
            "                           stdin continuously\n"
            "-v, --verbose              show debugging info\n"
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
    char const *optstring = "o:78Rp:m:eb:Id:f:s:c:w:h:r:q:kil:t:ugvSn:PE:B:C:DVH";
#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"outfile",          no_argument,        &long_opt, 'o'},
        {"7bit-mode",        no_argument,        &long_opt, '7'},
        {"8bit-mode",        no_argument,        &long_opt, '8'},
        {"gri-limit",        no_argument,        &long_opt, 'R'},
        {"colors",           required_argument,  &long_opt, 'p'},
        {"mapfile",          required_argument,  &long_opt, 'm'},
        {"monochrome",       no_argument,        &long_opt, 'e'},
        {"high-color",       no_argument,        &long_opt, 'I'},
        {"builtin-palette",  required_argument,  &long_opt, 'b'},
        {"diffusion",        required_argument,  &long_opt, 'd'},
        {"find-largest",     required_argument,  &long_opt, 'f'},
        {"select-color",     required_argument,  &long_opt, 's'},
        {"crop",             required_argument,  &long_opt, 'c'},
        {"width",            required_argument,  &long_opt, 'w'},
        {"height",           required_argument,  &long_opt, 'h'},
        {"resampling",       required_argument,  &long_opt, 'r'},
        {"quality",          required_argument,  &long_opt, 'q'},
        {"palette-type",     required_argument,  &long_opt, 't'},
        {"insecure",         no_argument,        &long_opt, 'k'},
        {"invert",           no_argument,        &long_opt, 'i'},
        {"loop-control",     required_argument,  &long_opt, 'l'},
        {"use-macro",        no_argument,        &long_opt, 'u'},
        {"ignore-delay",     no_argument,        &long_opt, 'g'},
        {"verbose",          no_argument,        &long_opt, 'v'},
        {"static",           no_argument,        &long_opt, 'S'},
        {"macro-number",     required_argument,  &long_opt, 'n'},
        {"penetrate",        no_argument,        &long_opt, 'P'},
        {"encode-policy",    required_argument,  &long_opt, 'E'},
        {"bgcolor",          required_argument,  &long_opt, 'B'},
        {"complexion-score", required_argument,  &long_opt, 'C'},
        {"pipe-mode",        no_argument,        &long_opt, 'D'}, /* deprecated */
        {"version",          no_argument,        &long_opt, 'V'},
        {"help",             no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */

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

    /* mark as success */
    status = SIXEL_OK;
    goto end;

argerr:
    fprintf(stderr,
            "usage: img2sixel [-78eIkiugvSPDVH] [-p colors] [-m file] [-d diffusiontype]\n"
            "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
            "                 [-h height] [-r resamplingtype] [-q quality] [-l loopmode]\n"
            "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
            "                 [-E encodepolicy] [-B bgcolor] [-o outfile] [filename ...]\n"
            "for more details, type: 'img2sixel -H'.\n");

error:
    fprintf(stderr, "%s\n%s\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
    status = (-1);
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
