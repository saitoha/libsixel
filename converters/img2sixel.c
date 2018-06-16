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
            "-D, --pipe-mode            read source images from stdin\n"
            "                           continuously (deprecated)\n"
            "-v, --verbose              show debugging info\n"
            "-V, --version              show version and license info\n"
            "-H, --help                 show this help\n"
            "-G LANG, --generate-code=LANG\n"
            "                           generate code equivalent to given\n"
            "                           command-line options in specified\n"
            "                           language.\n"
            "                             c -> generate C code\n"
            "                             python2 -> generate code for python2\n"
            "                             python3 -> generate code for python3\n"
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


static const char ** get_optflag_map()
{
    static const char *optflag_map[256];

    optflag_map['o'] = "SIXEL_OPTFLAG_OUTPUT";
    optflag_map['7'] = "SIXEL_OPTFLAG_7BIT_MODE";
    optflag_map['8'] = "SIXEL_OPTFLAG_8BIT_MODE";
    optflag_map['R'] = "SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT";
    optflag_map['p'] = "SIXEL_OPTFLAG_COLORS";
    optflag_map['m'] = "SIXEL_OPTFLAG_MAPFILE";
    optflag_map['e'] = "SIXEL_OPTFLAG_MONOCHROME";
    optflag_map['k'] = "SIXEL_OPTFLAG_INSECURE";
    optflag_map['i'] = "SIXEL_OPTFLAG_INVERT";
    optflag_map['I'] = "SIXEL_OPTFLAG_HIGH_COLOR";
    optflag_map['u'] = "SIXEL_OPTFLAG_USE_MACRO";
    optflag_map['n'] = "SIXEL_OPTFLAG_MACRO_NUMBER";
    optflag_map['C'] = "SIXEL_OPTFLAG_COMPLEXION_SCORE";
    optflag_map['g'] = "SIXEL_OPTFLAG_IGNORE_DELAY";
    optflag_map['S'] = "SIXEL_OPTFLAG_STATIC";
    optflag_map['d'] = "SIXEL_OPTFLAG_DIFFUSION";
    optflag_map['f'] = "SIXEL_OPTFLAG_FIND_LARGEST";
    optflag_map['s'] = "SIXEL_OPTFLAG_SELECT_COLOR";
    optflag_map['c'] = "SIXEL_OPTFLAG_CROP";
    optflag_map['w'] = "SIXEL_OPTFLAG_WIDTH";
    optflag_map['h'] = "SIXEL_OPTFLAG_HEIGHT";
    optflag_map['r'] = "SIXEL_OPTFLAG_RESAMPLING";
    optflag_map['q'] = "SIXEL_OPTFLAG_QUALITY";
    optflag_map['l'] = "SIXEL_OPTFLAG_LOOPMODE";
    optflag_map['t'] = "SIXEL_OPTFLAG_PALETTE_TYPE";
    optflag_map['b'] = "SIXEL_OPTFLAG_BUILTIN_PALETTE";
    optflag_map['E'] = "SIXEL_OPTFLAG_ENCODE_POLICY";
    optflag_map['B'] = "SIXEL_OPTFLAG_BGCOLOR";
    optflag_map['P'] = "SIXEL_OPTFLAG_PENETRATE";
    optflag_map['D'] = "SIXEL_OPTFLAG_PIPE_MODE";
    optflag_map['v'] = "SIXEL_OPTFLAG_VERBOSE";

    return optflag_map;
}


static char *
str_escape(char const *s, char const c, int use_multibyte)
{
    char *result;
    char *p;

    if (s == NULL) {
        return NULL;
    }

    p = result = malloc(strlen(s) * 4);

    while (*s) {
      if (*s == c) {
          *p++ = '\\';
          *p++ = *s++;
      } else if (use_multibyte && *s < 0) {
          *p++ = *s++;
      } else if (*s < 0x20 || *s >= 0x7f) {
          *p++ = '\\';
          *p++ = 'x';
          *p = ((*s < 0 ? 0x100: 0) + *s) >> 4;
          *p += (*p < 10) ? 0x30: 0x57;
          ++p;
          *p = ((*s < 0 ? 0x100: 0) + *s) & 0xf;
          *p += (*p < 10) ? 0x30: 0x57;
          ++p;
          ++s;
      } else {
          *p++ = *s++;
      }
    }
    *p = '\0';
    return result;
}


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
    char const *optstring = "o:78Rp:m:eb:Id:f:s:c:w:h:r:q:kil:t:ugvSn:PE:B:C:DVHG:";
#if HAVE_GETOPT_LONG
    char const **optflag_map = NULL;
    char *arg = NULL;
    enum LANGTYPE {
        LANG_NONE,
        LANG_C,
        LANG_PYTHON2,
        LANG_PYTHON3
    };
    enum LANGTYPE generation_lang = LANG_NONE;
    char escape_char = '"';
    int use_multibyte = 0;
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
        {"generate-code",    required_argument,  &long_opt, 'G'},
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
        case 'G':
            if (strcmp(optarg, "c") == 0) {
                generation_lang = LANG_C;
                escape_char = '"';
                use_multibyte = 0;
            } else if (strcmp(optarg, "python2") == 0) {
                generation_lang = LANG_PYTHON2;
                escape_char = '\'';
                use_multibyte = 0;
            } else if (strcmp(optarg, "python3") == 0) {
                generation_lang = LANG_PYTHON3;
                escape_char = '\'';
                use_multibyte = 1;
            } else {
                sixel_helper_set_additional_message(
                        "cannot parse the argument of generation option(-G).");
                status = SIXEL_BAD_ARGUMENT;
                goto argerr;
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

    if (generation_lang != LANG_NONE) {
        optind = 0;  /* reinitialize getopt() */

        optflag_map = get_optflag_map();

        switch (generation_lang) {
        case LANG_C:
            printf("/* generated by img2sixel */\n");
            printf("#include <sixel.h>\n");
            printf("#include <stdio.h>\n");
            printf("\n");
            printf("int main(int argc, char *argv[])\n");
            printf("{\n");
            printf("    SIXELSTATUS status = SIXEL_FALSE;\n");
            printf("    sixel_encoder_t *encoder = NULL;\n");
            printf("\n");
            printf("    status = sixel_encoder_new(&encoder, NULL);\n");
            printf("    if (SIXEL_FAILED(status)) {\n");
            printf("        goto error;\n");
            printf("    }\n");
            break;

        case LANG_PYTHON2:
            printf("#!/usr/bin/env python\n");
            printf("# generated by img2sixel\n");
            printf("\n");
            printf("from libsixel.encoder import *\n");
            printf("import locale\n");
            printf("lang, encoding = locale.getdefaultlocale()\n");
            printf("\n");
            printf("encoder = Encoder()\n");
            break;

        case LANG_PYTHON3:
            printf("#!/usr/bin/env python\n");
            printf("# generated by img2sixel\n");
            printf("\n");
            printf("from libsixel.encoder import *\n");
            printf("\n");
            printf("encoder = Encoder()\n");
            break;

        default:
            break;
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
            case SIXEL_OPTFLAG_GENERATE_CODE:
                /* ignore */
                break;
            default:
                arg = str_escape(optarg, escape_char, use_multibyte);
                switch (generation_lang) {
                case LANG_C:
                    printf("    status = %s(encoder, %s, \"%s\");\n",
                           "sixel_encoder_setopt", optflag_map[n], arg);
                    printf("    if (SIXEL_FAILED(status)) {\n");
                    printf("        goto error;\n");
                    printf("    }\n");
                    break;
                case LANG_PYTHON2:
                case LANG_PYTHON3:
                    printf("encoder.setopt(%s, '%s');\n", optflag_map[n], arg);
                    break;
                default:
                    break;
                }
                free(arg);
            }
        }
        if (optind == argc) {
            switch (generation_lang) {
            case LANG_C:
                printf("    status = sixel_encoder_encode(encoder, %s);\n", "NULL");
                printf("    if (SIXEL_FAILED(status)) {\n");
                printf("        goto error;\n");
                printf("    }\n");
                break;
            case LANG_PYTHON2:
            case LANG_PYTHON3:
                printf("encoder.encode()\n");
                break;
            default:
                break;
            }
        } else {
            for (n = optind; n < argc; n++) {
                arg = str_escape(argv[n], escape_char, use_multibyte);
                switch (generation_lang) {
                case LANG_C:
                    printf("    status = sixel_encoder_encode(encoder, \"%s\");\n", arg);
                    printf("    if (SIXEL_FAILED(status)) {\n");
                    printf("        goto error;\n");
                    printf("    }\n");
                    break;
                case LANG_PYTHON2:
                    printf("encoder.encode('%s'.decode(encoding))\n", arg);
                    break;
                case LANG_PYTHON3:
                    printf("encoder.encode('%s')\n", arg);
                    break;
                default:
                    break;
                }
                free(arg);
            }
        }
        switch (generation_lang) {
        case LANG_C:
            printf("    goto end;\n");
            printf("\n");
            printf("error:\n");
            printf("    fprintf(stderr, \"%s\\n\" \"%s\\n\",\n", "%s", "%s");
            printf("            sixel_helper_format_error(status),\n");
            printf("            sixel_helper_get_additional_message());\n");
            printf("\n");
            printf("end:\n");
            printf("    sixel_encoder_unref(encoder);\n");
            printf("}\n");
            break;
        case LANG_PYTHON2:
        case LANG_PYTHON3:
            break;
        default:
            break;
        }
    } else if (optind == argc) {  /* from stdin */
        status = sixel_encoder_encode(encoder, NULL);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else {  /* from files */
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
            "                 [-E encodepolicy] [-B bgcolor] [-o outfile] [-G lang]\n"
            "                 [filename ...]\n"
            "for more details, type: 'img2sixel -H'.\n");

error:
    fprintf(stderr, "%s\n" "%s\n",
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
