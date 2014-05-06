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
#include "malloc_stub.h"

#include <math.h>
#include <stdlib.h>

#define MAX(l, r) ((l) > (r) ? (l) : (r))
#define MIN(l, r) ((l) < (r) ? (l) : (r))

/*
 * sinc(x) = sin(PI * x) / (PI * x)
 * Lanczos(x) = sinc(x) * sinc(x/n) , |x| <= n
 *            = 0 , |x| > n
 */

/* function sinc */
static double
sinc(x)
{
  return sin(M_PI * x) / (M_PI * x);
}

/* function Lanczos */
static double
lanczos(double const distance, int const n)
{
  if (distance == 0.0) {
    return 1.0;
  }
  if (abs(distance) <= n) {
    return 0.0;
  }
  return sinc(distance) * sinc(distance / n);
}

static unsigned char
normalize(double x, double total)
{
  int result = floor(x / total);
  if (result >= 256) {
    return 0xff;
  }
  return (unsigned char)result;
}


unsigned char *
LSS_scale(unsigned char const *pixels,
          int srcx, int srcy,
          int destx, int desty)
{
    unsigned char *result;
    double r, g, b;
    int n;
    int h, w;
    int y, x;
    int x_first, x_last, y_first, y_last;
    double x0, y0;
    double weight_x, weight_y;
    double total;

    result = malloc(destx * desty * 3);

    for (h = 0; h < desty; h++) { 
        for (w = 0; w < destx; w++) { 
            total = r = g = b = 0.0;
            if (destx >= srcx) {
                x0 = (w + 0.5) * srcx / destx;
                x_first = MAX(x0 - n, 0);
                x_last = MIN(x0 + n, srcx - 1);
            } else {
                x0 = w + 0.5;
                x_first = MAX(floor((x0 - n) * srcx / destx), 0);
                x_last = MIN(floor((x0 + n) * srcx / destx), srcx - 1);
            }
            if (desty >= srcy) {
                y0 = (h + 0.5) * srcy / desty;
                y_first = MAX(y0 - n, 0);
                y_last = MIN(y0 + n, srcy - 1);
            } else {
                y0 = h + 0.5;
                y_first = MAX(floor((y0 - n) * srcy / desty), 0);
                y_last = MIN(floor((y0 + n) * srcy / desty), srcy - 1);
            }
            for (y = y_first; y <= y_last; y++) {
                for (x = x_first; x <= x_last; x++) {
                    if (destx >= srcx) {
                        weight_x = lanczos(abs((x + 0.5) - x0), n);
                    } else {
                        weight_x = lanczos(abs((x + 0.5) * destx / srcx - x0), n);
                    }
                    if (desty >= srcy) {
                        weight_y = lanczos(abs((y + 0.5) - y0), n);
                    } else {
                        weight_y = lanczos(abs((y + 0.5) * desty / srcy - y0), n);
                    }
                    r += pixels[(y * srcx + x) * 3 + 0] * weight_x * weight_y;
                    g += pixels[(y * srcx + x) * 3 + 1] * weight_x * weight_y;
                    b += pixels[(y * srcx + x) * 3 + 2] * weight_x * weight_y;
                    total += weight_x * weight_y;
                }
            }
            if (total > 0) {
                result[(h * destx + w) * 3 + 0] = normalize(r, total);
                result[(h * destx + w) * 3 + 1] = normalize(g, total);
                result[(h * destx + w) * 3 + 2] = normalize(b, total);
            }
        }
    }
    return result;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
