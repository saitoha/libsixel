/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>
#include <stdio.h>

#if HAVE_STDRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include <sixel.h>
#include "dither.h"
#include "palette.h"
#include "frame.h"
#include "pixelformat.h"
#include "writer.h"
#include "encoder.h"
#include "decoder.h"
#include "status.h"
#include "loader.h"
#include "fromgif.h"
#include "chunk.h"
#include "allocator.h"
#include "scale.h"

#if HAVE_TESTS

int sixel_tosixel_tests_main(void);

#endif

#if HAVE_TESTS

int
main(int argc, char *argv[])
{
    int nret = EXIT_FAILURE;
    int dirty = 0;

    (void) argc;
    (void) argv;

    nret = sixel_fromgif_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("fromgif ng.");
        dirty = 1;
    } else {
        puts("fromgif ok.");
    }

    nret = sixel_loader_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("loader ng.");
        dirty = 1;
    } else {
        puts("loader ok.");
    }

    nret = sixel_dither_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("dither ng.");
        dirty = 1;
    } else {
        puts("dither ok.");
    }

    nret = sixel_pixelformat_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("pixelformat ng.");
        dirty = 1;
    } else {
        puts("pixelformat ok.");
    }

    nret = sixel_frame_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("frame ng.");
        dirty = 1;
    } else {
        puts("frame ok.");
    }

    nret = sixel_writer_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("writer ng.");
        dirty = 1;
    } else {
        puts("writer ok.");
    }

    nret = sixel_palette_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("quant ng.");
        dirty = 1;
    } else {
        puts("quant ok.");
    }

    nret = sixel_tosixel_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("tosixel ng.");
        dirty = 1;
    } else {
        puts("tosixel ok.");
    }

    nret = sixel_encoder_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("encoder ng.");
        dirty = 1;
    } else {
        puts("encoder ok.");
    }

    nret = sixel_decoder_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("decoder ng.");
        dirty = 1;
    } else {
        puts("decoder ok.");
    }

    nret = sixel_status_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("status ng.");
        dirty = 1;
    } else {
        puts("status ok.");
    }

    nret = sixel_chunk_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("chunk ng.");
        dirty = 1;
    } else {
        puts("chunk ok.");
    }

    nret = sixel_allocator_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("allocator ng.");
        dirty = 1;
    } else {
        puts("allocator ok.");
    }

    nret = sixel_scale_tests_main();
    if (nret != EXIT_SUCCESS) {
        puts("scale ng.");
        dirty = 1;
    } else {
        puts("scale ok.");
    }

    fflush(stdout);

    if (dirty) {
        return (127);
    }
    return (0);
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
