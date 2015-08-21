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

#ifndef LIBSIXEL_ENCODER_H
#define LIBSIXEL_ENCODER_H

/* encoder object */
struct sixel_encoder {
    unsigned int ref;               /* reference counter */
    sixel_allocator_t *allocator;   /* allocator object */
    int reqcolors;
    char *mapfile;
    int monochrome;
    int highcolor;
    int builtin_palette;
    int method_for_diffuse;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fignore_delay;
    int complexion;
    int fstatic;
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
    int pipe_mode;
    int verbose;
    unsigned char *bgcolor;
    int outfd;
    int finsecure;
    int *cancel_flag;
    void *dither_cache;
};

#if HAVE_TESTS
int
sixel_encoder_tests_main(void);
#endif

#endif /* LIBSIXEL_ENCODER_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
