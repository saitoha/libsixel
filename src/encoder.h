/*
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_ENCODER_H
#define LIBSIXEL_ENCODER_H

#include <stddef.h>
#include <limits.h>

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

struct sixel_frame;
struct sixel_assessment;

/* palette type */
#define SIXEL_COLOR_OPTION_DEFAULT          0   /* use default settings */
#define SIXEL_COLOR_OPTION_MONOCHROME       1   /* use monochrome palette */
#define SIXEL_COLOR_OPTION_BUILTIN          2   /* use builtin palette */
#define SIXEL_COLOR_OPTION_MAPFILE          3   /* use mapfile option */
#define SIXEL_COLOR_OPTION_HIGHCOLOR        4   /* use highcolor option */

/* encoder object */
struct sixel_encoder {

    unsigned int ref;               /* reference counter */
    sixel_allocator_t *allocator;   /* allocator object */
    int reqcolors;
    int force_palette;              /* force palette size when non-zero */
    int color_option;
    char *mapfile;
    char *palette_output;
    char *loader_order;
    int builtin_palette;
    int method_for_diffuse;
    int method_for_scan;
    int method_for_carry;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int quantize_model;
    int final_merge_mode;
    int lut_policy;
    int sixel_reversible;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fdrcs;
    int fignore_delay;
    int complexion;
    int fstatic;
    int cell_width;
    int cell_height;
    int pixelwidth;
    int pixelheight;
    int percentwidth;
    int percentheight;
    int clipx;
    int clipy;
    int clipwidth;
    int clipheight;
    int clipfirst;
    int macro_number;
    int penetrate_multiplexer;
    int encode_policy;
    int working_colorspace;
    int output_colorspace;
    int ormode;
    int pipe_mode;
    int verbose;
    int has_gri_arg_limit;
    unsigned char *bgcolor;
    int outfd;
    int tile_outfd;
    int finsecure;
    int *cancel_flag;
    void *dither_cache;
    unsigned short drcs_charset_no;
    int drcs_mmv;
    int capture_quantized;
    int capture_source;
    unsigned char *capture_pixels;
    size_t capture_pixels_size;
    unsigned char *capture_palette;
    size_t capture_palette_size;
    size_t capture_pixel_bytes;
    int capture_width;
    int capture_height;
    int capture_pixelformat;
    int capture_colorspace;
    int capture_ncolors;
    int capture_valid;
    struct sixel_frame *capture_source_frame;
    unsigned int assessment_sections;
    const char *assessment_json_path;
    struct sixel_assessment *assessment_observer;
    char last_loader_name[64];
    char last_source_path[PATH_MAX];
    size_t last_input_bytes;
    int output_is_png;
    int output_png_to_stdout;
    char *png_output_path;
    char *sixel_output_path;
    int clipboard_output_active;
    char clipboard_output_format[32];
    char *clipboard_output_path;
};

#if HAVE_TESTS
SIXELAPI int
sixel_encoder_tests_main(void);
#endif

#endif /* LIBSIXEL_ENCODER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
