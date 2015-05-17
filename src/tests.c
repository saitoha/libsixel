/*
 * Copyright (c) 2014 Hayaki Saito
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include "dither.h"
#include "quant.h"
#include "pixelformat.h"
#include <sixel.h>

#if HAVE_TESTS

int
main(int argc, char *argv[])
{
    int nret = EXIT_FAILURE;

    (void) argc;
    (void) argv;

    nret = sixel_dither_tests_main();
    if (nret != EXIT_SUCCESS) {
        goto error;
    }

    nret = sixel_pixelformat_tests_main();
    if (nret != EXIT_SUCCESS) {
        goto error;
    }
    puts("done.");
error:
    return nret;
}

#endif

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
