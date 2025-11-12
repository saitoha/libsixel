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

#include <stdlib.h>
#include <stdio.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include "dither.h"
#include "palette.h"
#include "lut.h"
#include "assessment.h"
#include <sixel.h>


static const unsigned char pal_mono_dark[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};


static const unsigned char pal_mono_light[] = {
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00
};

static const unsigned char pal_gray_1bit[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_2bit[] = {
    0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_4bit[] = {
    0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33,
    0x44, 0x44, 0x44, 0x55, 0x55, 0x55, 0x66, 0x66, 0x66, 0x77, 0x77, 0x77,
    0x88, 0x88, 0x88, 0x99, 0x99, 0x99, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xbb,
    0xcc, 0xcc, 0xcc, 0xdd, 0xdd, 0xdd, 0xee, 0xee, 0xee, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_8bit[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b,
    0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f,
    0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b,
    0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1e, 0x1e, 0x1e, 0x1f, 0x1f, 0x1f,
    0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23,
    0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x26, 0x26, 0x26, 0x27, 0x27, 0x27,
    0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b,
    0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f,
    0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33,
    0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37,
    0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x3a, 0x3a, 0x3a, 0x3b, 0x3b, 0x3b,
    0x3c, 0x3c, 0x3c, 0x3d, 0x3d, 0x3d, 0x3e, 0x3e, 0x3e, 0x3f, 0x3f, 0x3f,
    0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43,
    0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x47, 0x47, 0x47,
    0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x4a, 0x4a, 0x4a, 0x4b, 0x4b, 0x4b,
    0x4c, 0x4c, 0x4c, 0x4d, 0x4d, 0x4d, 0x4e, 0x4e, 0x4e, 0x4f, 0x4f, 0x4f,
    0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52, 0x53, 0x53, 0x53,
    0x54, 0x54, 0x54, 0x55, 0x55, 0x55, 0x56, 0x56, 0x56, 0x57, 0x57, 0x57,
    0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x5a, 0x5a, 0x5a, 0x5b, 0x5b, 0x5b,
    0x5c, 0x5c, 0x5c, 0x5d, 0x5d, 0x5d, 0x5e, 0x5e, 0x5e, 0x5f, 0x5f, 0x5f,
    0x60, 0x60, 0x60, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x63, 0x63, 0x63,
    0x64, 0x64, 0x64, 0x65, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x67,
    0x68, 0x68, 0x68, 0x69, 0x69, 0x69, 0x6a, 0x6a, 0x6a, 0x6b, 0x6b, 0x6b,
    0x6c, 0x6c, 0x6c, 0x6d, 0x6d, 0x6d, 0x6e, 0x6e, 0x6e, 0x6f, 0x6f, 0x6f,
    0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73,
    0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77,
    0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b,
    0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f,
    0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83,
    0x84, 0x84, 0x84, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87,
    0x88, 0x88, 0x88, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b, 0x8b,
    0x8c, 0x8c, 0x8c, 0x8d, 0x8d, 0x8d, 0x8e, 0x8e, 0x8e, 0x8f, 0x8f, 0x8f,
    0x90, 0x90, 0x90, 0x91, 0x91, 0x91, 0x92, 0x92, 0x92, 0x93, 0x93, 0x93,
    0x94, 0x94, 0x94, 0x95, 0x95, 0x95, 0x96, 0x96, 0x96, 0x97, 0x97, 0x97,
    0x98, 0x98, 0x98, 0x99, 0x99, 0x99, 0x9a, 0x9a, 0x9a, 0x9b, 0x9b, 0x9b,
    0x9c, 0x9c, 0x9c, 0x9d, 0x9d, 0x9d, 0x9e, 0x9e, 0x9e, 0x9f, 0x9f, 0x9f,
    0xa0, 0xa0, 0xa0, 0xa1, 0xa1, 0xa1, 0xa2, 0xa2, 0xa2, 0xa3, 0xa3, 0xa3,
    0xa4, 0xa4, 0xa4, 0xa5, 0xa5, 0xa5, 0xa6, 0xa6, 0xa6, 0xa7, 0xa7, 0xa7,
    0xa8, 0xa8, 0xa8, 0xa9, 0xa9, 0xa9, 0xaa, 0xaa, 0xaa, 0xab, 0xab, 0xab,
    0xac, 0xac, 0xac, 0xad, 0xad, 0xad, 0xae, 0xae, 0xae, 0xaf, 0xaf, 0xaf,
    0xb0, 0xb0, 0xb0, 0xb1, 0xb1, 0xb1, 0xb2, 0xb2, 0xb2, 0xb3, 0xb3, 0xb3,
    0xb4, 0xb4, 0xb4, 0xb5, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7, 0xb7,
    0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xba, 0xba, 0xba, 0xbb, 0xbb, 0xbb,
    0xbc, 0xbc, 0xbc, 0xbd, 0xbd, 0xbd, 0xbe, 0xbe, 0xbe, 0xbf, 0xbf, 0xbf,
    0xc0, 0xc0, 0xc0, 0xc1, 0xc1, 0xc1, 0xc2, 0xc2, 0xc2, 0xc3, 0xc3, 0xc3,
    0xc4, 0xc4, 0xc4, 0xc5, 0xc5, 0xc5, 0xc6, 0xc6, 0xc6, 0xc7, 0xc7, 0xc7,
    0xc8, 0xc8, 0xc8, 0xc9, 0xc9, 0xc9, 0xca, 0xca, 0xca, 0xcb, 0xcb, 0xcb,
    0xcc, 0xcc, 0xcc, 0xcd, 0xcd, 0xcd, 0xce, 0xce, 0xce, 0xcf, 0xcf, 0xcf,
    0xd0, 0xd0, 0xd0, 0xd1, 0xd1, 0xd1, 0xd2, 0xd2, 0xd2, 0xd3, 0xd3, 0xd3,
    0xd4, 0xd4, 0xd4, 0xd5, 0xd5, 0xd5, 0xd6, 0xd6, 0xd6, 0xd7, 0xd7, 0xd7,
    0xd8, 0xd8, 0xd8, 0xd9, 0xd9, 0xd9, 0xda, 0xda, 0xda, 0xdb, 0xdb, 0xdb,
    0xdc, 0xdc, 0xdc, 0xdd, 0xdd, 0xdd, 0xde, 0xde, 0xde, 0xdf, 0xdf, 0xdf,
    0xe0, 0xe0, 0xe0, 0xe1, 0xe1, 0xe1, 0xe2, 0xe2, 0xe2, 0xe3, 0xe3, 0xe3,
    0xe4, 0xe4, 0xe4, 0xe5, 0xe5, 0xe5, 0xe6, 0xe6, 0xe6, 0xe7, 0xe7, 0xe7,
    0xe8, 0xe8, 0xe8, 0xe9, 0xe9, 0xe9, 0xea, 0xea, 0xea, 0xeb, 0xeb, 0xeb,
    0xec, 0xec, 0xec, 0xed, 0xed, 0xed, 0xee, 0xee, 0xee, 0xef, 0xef, 0xef,
    0xf0, 0xf0, 0xf0, 0xf1, 0xf1, 0xf1, 0xf2, 0xf2, 0xf2, 0xf3, 0xf3, 0xf3,
    0xf4, 0xf4, 0xf4, 0xf5, 0xf5, 0xf5, 0xf6, 0xf6, 0xf6, 0xf7, 0xf7, 0xf7,
    0xf8, 0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xfa, 0xfa, 0xfa, 0xfb, 0xfb, 0xfb,
    0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff
};


static const unsigned char pal_xterm256[] = {
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x80, 0x80, 0xc0, 0xc0, 0xc0,
    0x80, 0x80, 0x80, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00,
    0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x00, 0x00, 0x87, 0x00, 0x00, 0xaf,
    0x00, 0x00, 0xd7, 0x00, 0x00, 0xff, 0x00, 0x5f, 0x00, 0x00, 0x5f, 0x5f,
    0x00, 0x5f, 0x87, 0x00, 0x5f, 0xaf, 0x00, 0x5f, 0xd7, 0x00, 0x5f, 0xff,
    0x00, 0x87, 0x00, 0x00, 0x87, 0x5f, 0x00, 0x87, 0x87, 0x00, 0x87, 0xaf,
    0x00, 0x87, 0xd7, 0x00, 0x87, 0xff, 0x00, 0xaf, 0x00, 0x00, 0xaf, 0x5f,
    0x00, 0xaf, 0x87, 0x00, 0xaf, 0xaf, 0x00, 0xaf, 0xd7, 0x00, 0xaf, 0xff,
    0x00, 0xd7, 0x00, 0x00, 0xd7, 0x5f, 0x00, 0xd7, 0x87, 0x00, 0xd7, 0xaf,
    0x00, 0xd7, 0xd7, 0x00, 0xd7, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x5f,
    0x00, 0xff, 0x87, 0x00, 0xff, 0xaf, 0x00, 0xff, 0xd7, 0x00, 0xff, 0xff,
    0x5f, 0x00, 0x00, 0x5f, 0x00, 0x5f, 0x5f, 0x00, 0x87, 0x5f, 0x00, 0xaf,
    0x5f, 0x00, 0xd7, 0x5f, 0x00, 0xff, 0x5f, 0x5f, 0x00, 0x5f, 0x5f, 0x5f,
    0x5f, 0x5f, 0x87, 0x5f, 0x5f, 0xaf, 0x5f, 0x5f, 0xd7, 0x5f, 0x5f, 0xff,
    0x5f, 0x87, 0x00, 0x5f, 0x87, 0x5f, 0x5f, 0x87, 0x87, 0x5f, 0x87, 0xaf,
    0x5f, 0x87, 0xd7, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0x00, 0x5f, 0xaf, 0x5f,
    0x5f, 0xaf, 0x87, 0x5f, 0xaf, 0xaf, 0x5f, 0xaf, 0xd7, 0x5f, 0xaf, 0xff,
    0x5f, 0xd7, 0x00, 0x5f, 0xd7, 0x5f, 0x5f, 0xd7, 0x87, 0x5f, 0xd7, 0xaf,
    0x5f, 0xd7, 0xd7, 0x5f, 0xd7, 0xff, 0x5f, 0xff, 0x00, 0x5f, 0xff, 0x5f,
    0x5f, 0xff, 0x87, 0x5f, 0xff, 0xaf, 0x5f, 0xff, 0xd7, 0x5f, 0xff, 0xff,
    0x87, 0x00, 0x00, 0x87, 0x00, 0x5f, 0x87, 0x00, 0x87, 0x87, 0x00, 0xaf,
    0x87, 0x00, 0xd7, 0x87, 0x00, 0xff, 0x87, 0x5f, 0x00, 0x87, 0x5f, 0x5f,
    0x87, 0x5f, 0x87, 0x87, 0x5f, 0xaf, 0x87, 0x5f, 0xd7, 0x87, 0x5f, 0xff,
    0x87, 0x87, 0x00, 0x87, 0x87, 0x5f, 0x87, 0x87, 0x87, 0x87, 0x87, 0xaf,
    0x87, 0x87, 0xd7, 0x87, 0x87, 0xff, 0x87, 0xaf, 0x00, 0x87, 0xaf, 0x5f,
    0x87, 0xaf, 0x87, 0x87, 0xaf, 0xaf, 0x87, 0xaf, 0xd7, 0x87, 0xaf, 0xff,
    0x87, 0xd7, 0x00, 0x87, 0xd7, 0x5f, 0x87, 0xd7, 0x87, 0x87, 0xd7, 0xaf,
    0x87, 0xd7, 0xd7, 0x87, 0xd7, 0xff, 0x87, 0xff, 0x00, 0x87, 0xff, 0x5f,
    0x87, 0xff, 0x87, 0x87, 0xff, 0xaf, 0x87, 0xff, 0xd7, 0x87, 0xff, 0xff,
    0xaf, 0x00, 0x00, 0xaf, 0x00, 0x5f, 0xaf, 0x00, 0x87, 0xaf, 0x00, 0xaf,
    0xaf, 0x00, 0xd7, 0xaf, 0x00, 0xff, 0xaf, 0x5f, 0x00, 0xaf, 0x5f, 0x5f,
    0xaf, 0x5f, 0x87, 0xaf, 0x5f, 0xaf, 0xaf, 0x5f, 0xd7, 0xaf, 0x5f, 0xff,
    0xaf, 0x87, 0x00, 0xaf, 0x87, 0x5f, 0xaf, 0x87, 0x87, 0xaf, 0x87, 0xaf,
    0xaf, 0x87, 0xd7, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0x00, 0xaf, 0xaf, 0x5f,
    0xaf, 0xaf, 0x87, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xd7, 0xaf, 0xaf, 0xff,
    0xaf, 0xd7, 0x00, 0xaf, 0xd7, 0x5f, 0xaf, 0xd7, 0x87, 0xaf, 0xd7, 0xaf,
    0xaf, 0xd7, 0xd7, 0xaf, 0xd7, 0xff, 0xaf, 0xff, 0x00, 0xaf, 0xff, 0x5f,
    0xaf, 0xff, 0x87, 0xaf, 0xff, 0xaf, 0xaf, 0xff, 0xd7, 0xaf, 0xff, 0xff,
    0xd7, 0x00, 0x00, 0xd7, 0x00, 0x5f, 0xd7, 0x00, 0x87, 0xd7, 0x00, 0xaf,
    0xd7, 0x00, 0xd7, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0x00, 0xd7, 0x5f, 0x5f,
    0xd7, 0x5f, 0x87, 0xd7, 0x5f, 0xaf, 0xd7, 0x5f, 0xd7, 0xd7, 0x5f, 0xff,
    0xd7, 0x87, 0x00, 0xd7, 0x87, 0x5f, 0xd7, 0x87, 0x87, 0xd7, 0x87, 0xaf,
    0xd7, 0x87, 0xd7, 0xd7, 0x87, 0xff, 0xd7, 0xaf, 0x00, 0xd7, 0xaf, 0x5f,
    0xd7, 0xaf, 0x87, 0xd7, 0xaf, 0xaf, 0xd7, 0xaf, 0xd7, 0xd7, 0xaf, 0xff,
    0xd7, 0xd7, 0x00, 0xd7, 0xd7, 0x5f, 0xd7, 0xd7, 0x87, 0xd7, 0xd7, 0xaf,
    0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0x00, 0xd7, 0xff, 0x5f,
    0xd7, 0xff, 0x87, 0xd7, 0xff, 0xaf, 0xd7, 0xff, 0xd7, 0xd7, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff, 0x00, 0x5f, 0xff, 0x00, 0x87, 0xff, 0x00, 0xaf,
    0xff, 0x00, 0xd7, 0xff, 0x00, 0xff, 0xff, 0x5f, 0x00, 0xff, 0x5f, 0x5f,
    0xff, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0xff, 0x5f, 0xd7, 0xff, 0x5f, 0xff,
    0xff, 0x87, 0x00, 0xff, 0x87, 0x5f, 0xff, 0x87, 0x87, 0xff, 0x87, 0xaf,
    0xff, 0x87, 0xd7, 0xff, 0x87, 0xff, 0xff, 0xaf, 0x00, 0xff, 0xaf, 0x5f,
    0xff, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0xff, 0xaf, 0xd7, 0xff, 0xaf, 0xff,
    0xff, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0xff, 0xd7, 0x87, 0xff, 0xd7, 0xaf,
    0xff, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x5f,
    0xff, 0xff, 0x87, 0xff, 0xff, 0xaf, 0xff, 0xff, 0xd7, 0xff, 0xff, 0xff,
    0x08, 0x08, 0x08, 0x12, 0x12, 0x12, 0x1c, 0x1c, 0x1c, 0x26, 0x26, 0x26,
    0x30, 0x30, 0x30, 0x3a, 0x3a, 0x3a, 0x44, 0x44, 0x44, 0x4e, 0x4e, 0x4e,
    0x58, 0x58, 0x58, 0x62, 0x62, 0x62, 0x6c, 0x6c, 0x6c, 0x76, 0x76, 0x76,
    0x80, 0x80, 0x80, 0x8a, 0x8a, 0x8a, 0x94, 0x94, 0x94, 0x9e, 0x9e, 0x9e,
    0xa8, 0xa8, 0xa8, 0xb2, 0xb2, 0xb2, 0xbc, 0xbc, 0xbc, 0xc6, 0xc6, 0xc6,
    0xd0, 0xd0, 0xd0, 0xda, 0xda, 0xda, 0xe4, 0xe4, 0xe4, 0xee, 0xee, 0xee,
};


static float mask_a(int x, int y, int c);
static float mask_x(int x, int y, int c);
static void diffuse_none(unsigned char *data, int width, int height,
                         int x, int y, int depth, int error, int direction);
static void diffuse_fs(unsigned char *data, int width, int height,
                       int x, int y, int depth, int error, int direction);
static void diffuse_atkinson(unsigned char *data, int width, int height,
                             int x, int y, int depth, int error,
                             int direction);
static void diffuse_jajuni(unsigned char *data, int width, int height,
                           int x, int y, int depth, int error,
                           int direction);
static void diffuse_stucki(unsigned char *data, int width, int height,
                           int x, int y, int depth, int error,
                           int direction);
static void diffuse_burkes(unsigned char *data, int width, int height,
                           int x, int y, int depth, int error,
                           int direction);
static void diffuse_sierra1(unsigned char *data, int width, int height,
                            int x, int y, int depth, int error,
                            int direction);
static void diffuse_sierra2(unsigned char *data, int width, int height,
                            int x, int y, int depth, int error,
                            int direction);
static void diffuse_sierra3(unsigned char *data, int width, int height,
                            int x, int y, int depth, int error,
                            int direction);
static void diffuse_none_carry(int32_t *carry_curr, int32_t *carry_next,
                               int32_t *carry_far, int width, int height,
                               int depth, int x, int y, int32_t error,
                               int direction, int channel);
static void diffuse_fs_carry(int32_t *carry_curr, int32_t *carry_next,
                             int32_t *carry_far, int width, int height,
                             int depth, int x, int y, int32_t error,
                             int direction, int channel);
static void diffuse_atkinson_carry(int32_t *carry_curr, int32_t *carry_next,
                                   int32_t *carry_far, int width,
                                   int height, int depth, int x, int y,
                                   int32_t error, int direction,
                                   int channel);
static void diffuse_jajuni_carry(int32_t *carry_curr, int32_t *carry_next,
                                 int32_t *carry_far, int width, int height,
                                 int depth, int x, int y, int32_t error,
                                 int direction, int channel);
static void diffuse_stucki_carry(int32_t *carry_curr, int32_t *carry_next,
                                 int32_t *carry_far, int width, int height,
                                 int depth, int x, int y, int32_t error,
                                 int direction, int channel);
static void diffuse_burkes_carry(int32_t *carry_curr, int32_t *carry_next,
                                 int32_t *carry_far, int width, int height,
                                 int depth, int x, int y, int32_t error,
                                 int direction, int channel);
static void diffuse_sierra1_carry(int32_t *carry_curr, int32_t *carry_next,
                                  int32_t *carry_far, int width, int height,
                                  int depth, int x, int y, int32_t error,
                                  int direction, int channel);
static void diffuse_sierra2_carry(int32_t *carry_curr, int32_t *carry_next,
                                  int32_t *carry_far, int width, int height,
                                  int depth, int x, int y, int32_t error,
                                  int direction, int channel);
static void diffuse_sierra3_carry(int32_t *carry_curr, int32_t *carry_next,
                                  int32_t *carry_far, int width, int height,
                                  int depth, int x, int y, int32_t error,
                                  int direction, int channel);

static sixel_lut_t *dither_lut_context = NULL;

static const int (*
lso2_table(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

/*****************************************************************************
 *
 * dithering primitives
 *
 *****************************************************************************/


/* diffuse error energy to surround pixels (normal strategy) */
static void
error_diffuse_normal(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + (error * numerator * 2 / denominator + 1) / 2;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

/* error diffusion with fast strategy */
static void
error_diffuse_fast(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + error * numerator / denominator;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}


/* error diffusion with precise strategy */
static void
error_diffuse_precise(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = (int)(*data + error * numerator / (double)denominator + 0.5);
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}


typedef void (*diffuse_fixed_carry_mode)(int32_t *carry_curr,
                                         int32_t *carry_next,
                                         int32_t *carry_far,
                                         int width,
                                         int height,
                                         int depth,
                                         int x,
                                         int y,
                                         int32_t error,
                                         int direction,
                                         int channel);


typedef void (*diffuse_varerr_mode)(unsigned char *data,
                                    int width,
                                    int height,
                                    int x,
                                    int y,
                                    int depth,
                                    int32_t error,
                                    int index,
                                    int direction);

typedef void (*diffuse_varerr_carry_mode)(int32_t *carry_curr,
                                          int32_t *carry_next,
                                          int32_t *carry_far,
                                          int width,
                                          int height,
                                          int depth,
                                          int x,
                                          int y,
                                          int32_t error,
                                          int index,
                                          int direction,
                                          int channel);


static int32_t
diffuse_varerr_term(int32_t error, int weight, int denom)
{
    int64_t delta;

    delta = (int64_t)error * (int64_t)weight;
    if (delta >= 0) {
        delta = (delta + denom / 2) / denom;
    } else {
        delta = (delta - denom / 2) / denom;
    }

    return (int32_t)delta;
}


static int32_t
diffuse_fixed_term(int32_t error, int numerator, int denominator)
{
    int64_t delta;

    delta = (int64_t)error * (int64_t)numerator;
    if (delta >= 0) {
        delta = (delta + denominator / 2) / denominator;
    } else {
        delta = (delta - denominator / 2) / denominator;
    }

    return (int32_t)delta;
}


static void
diffuse_varerr_apply_direct(unsigned char *target, int depth, size_t offset,
                            int32_t delta)
{
    int64_t value;
    int result;

    value = (int64_t)target[offset * depth] << VARERR_SCALE_SHIFT;
    value += delta;
    if (value < 0) {
        value = 0;
    } else {
        int64_t max_value;

        max_value = VARERR_MAX_VALUE;
        if (value > max_value) {
            value = max_value;
        }
    }

    result = (int)((value + VARERR_ROUND) >> VARERR_SCALE_SHIFT);
    if (result < 0) {
        result = 0;
    }
    if (result > 255) {
        result = 255;
    }
    target[offset * depth] = (unsigned char)result;
}


static void
diffuse_lso2(unsigned char *data, int width, int height,
             int x, int y, int depth, int32_t error,
             int index, int direction)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    int32_t term_r = 0;
    int32_t term_r2 = 0;
    int32_t term_dl = 0;
    int32_t term_d = 0;
    int32_t term_dr = 0;
    int32_t term_d2 = 0;
    size_t offset;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table();
    entry = table[index];
    denom = entry[6];
    if (denom == 0) {
        return;
    }

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = diffuse_varerr_term(error, entry[5], denom);


    if (direction >= 0) {
        if (x + 1 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x + 2 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    } else {
        if (x - 1 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x - 2 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    }
}


static void
diffuse_lso2_carry(int32_t *carry_curr, int32_t *carry_next, int32_t *carry_far,
                   int width, int height, int depth,
                   int x, int y, int32_t error,
                   int index, int direction, int channel)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    int32_t term_r = 0;
    int32_t term_r2 = 0;
    int32_t term_dl = 0;
    int32_t term_d = 0;
    int32_t term_dr = 0;
    int32_t term_d2 = 0;
    size_t base;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table();
    entry = table[index];
    denom = entry[6];
    if (denom == 0) {
        return;
    }

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = error - term_r - term_r2 - term_dl - term_d - term_dr;

    if (direction >= 0) {
        if (x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x + 2 < width) {
            base = ((size_t)(x + 2) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    } else {
        if (x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x - 2 >= 0) {
            base = ((size_t)(x - 2) * (size_t)depth) + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    }
}


static void
scanline_params(int serpentine, int index, int limit,
                int *start, int *end, int *step, int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}


static SIXELSTATUS
sixel_dither_apply_positional(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int methodForDiffuse,
    int methodForScan,
    int foptimize_palette,
    int (*f_lookup)(const unsigned char *pixel,
                    int depth,
                    const unsigned char *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion),
    unsigned short *indextable,
    int complexion,
    unsigned char copy[],
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors)
{
    int serpentine;
    int y;
    float (*f_mask)(int x, int y, int c);

    switch (methodForDiffuse) {
    case SIXEL_DIFFUSE_A_DITHER:
        f_mask = mask_a;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
    default:
        f_mask = mask_x;
        break;
    }

    serpentine = (methodForScan == SIXEL_SCAN_SERPENTINE);

    if (foptimize_palette) {
        int x;

        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
        for (y = 0; y < height; ++y) {
            int start;
            int end;
            int step;
            int direction;

            scanline_params(serpentine, y, width,
                            &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;
                int color_index;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + f_mask(x, y, d) * 32;
                    copy[d] = val < 0 ? 0
                               : val > 255 ? 255 : val;
                }
                color_index = f_lookup(copy, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                if (migration_map[color_index] == 0) {
                    result[pos] = *ncolors;
                    for (d = 0; d < depth; ++d) {
                        new_palette[*ncolors * depth + d]
                            = palette[color_index * depth + d];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    result[pos] = migration_map[color_index] - 1;
                }
            }
        }
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
    } else {
        int x;

        for (y = 0; y < height; ++y) {
            int start;
            int end;
            int step;
            int direction;

            scanline_params(serpentine, y, width,
                            &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + f_mask(x, y, d) * 32;
                    copy[d] = val < 0 ? 0
                               : val > 255 ? 255 : val;
                }
                result[pos] = f_lookup(copy, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
            }
        }
        *ncolors = reqcolor;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_dither_apply_variable(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int methodForScan,
    int foptimize_palette,
    int (*f_lookup)(const unsigned char *pixel,
                    int depth,
                    const unsigned char *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion),
    unsigned short *indextable,
    int complexion,
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors,
    int methodForDiffuse,
    int methodForCarry)
{
    SIXELSTATUS status = SIXEL_FALSE;
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    int serpentine;
    int y;
    diffuse_varerr_mode varerr_diffuse;
    diffuse_varerr_carry_mode varerr_diffuse_carry;
    int use_carry;
    size_t carry_len;
    int32_t *carry_curr = NULL;
    int32_t *carry_next = NULL;
    int32_t *carry_far = NULL;
    unsigned char corrected[max_channels];
    int32_t sample_scaled[max_channels];
    int32_t accum_scaled[max_channels];
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    size_t carry_base;
    const unsigned char *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int diff;
    int table_index;
    int64_t accum;
    int64_t clamped;
    int32_t target_scaled;
    int32_t error_scaled;
    int32_t *tmp;

    if (depth > max_channels) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    use_carry = (methodForCarry == SIXEL_CARRY_ENABLE);
    carry_len = 0;

    switch (methodForDiffuse) {
    case SIXEL_DIFFUSE_LSO2:
        varerr_diffuse = diffuse_lso2;
        varerr_diffuse_carry = diffuse_lso2_carry;
        break;
    default:
        varerr_diffuse = diffuse_lso2;
        varerr_diffuse_carry = diffuse_lso2_carry;
        break;
    }

    if (use_carry) {
        carry_len = (size_t)width * (size_t)depth;
        carry_curr = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_curr == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        carry_next = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_next == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        carry_far = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_far == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    serpentine = (methodForScan == SIXEL_SCAN_SERPENTINE);

    if (foptimize_palette) {
        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    }

    for (y = 0; y < height; ++y) {
        scanline_params(serpentine, y, width,
                        &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            carry_base = (size_t)x * (size_t)depth;
            if (use_carry) {
                for (n = 0; n < depth; ++n) {
                    accum = ((int64_t)data[base + n]
                             << VARERR_SCALE_SHIFT)
                          + carry_curr[carry_base + (size_t)n];
                    if (accum < INT32_MIN) {
                        accum = INT32_MIN;
                    } else if (accum > INT32_MAX) {
                        accum = INT32_MAX;
                    }
                    carry_curr[carry_base + (size_t)n] = 0;
                    clamped = accum;
                    if (clamped < 0) {
                        clamped = 0;
                    } else if (clamped > VARERR_MAX_VALUE) {
                        clamped = VARERR_MAX_VALUE;
                    }
                    accum_scaled[n] = (int32_t)clamped;
                    corrected[n]
                        = (unsigned char)((clamped + VARERR_ROUND)
                                          >> VARERR_SCALE_SHIFT);
                }
                source_pixel = corrected;
            } else {
                for (n = 0; n < depth; ++n) {
                    sample_scaled[n]
                        = (int32_t)data[base + n]
                        << VARERR_SCALE_SHIFT;
                    corrected[n] = data[base + n];
                }
                source_pixel = data + base;
            }

            color_index = f_lookup(source_pixel, depth, palette,
                                   reqcolor, indextable,
                                   complexion);

            if (foptimize_palette) {
                if (migration_map[color_index] == 0) {
                    output_index = *ncolors;
                    for (n = 0; n < depth; ++n) {
                        new_palette[output_index * depth + n]
                            = palette[color_index * depth + n];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    output_index = migration_map[color_index] - 1;
                }
                result[pos] = output_index;
            } else {
                output_index = color_index;
                result[pos] = output_index;
            }

            for (n = 0; n < depth; ++n) {
                if (foptimize_palette) {
                    palette_value = new_palette[output_index * depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                }
                diff = (int)source_pixel[n] - palette_value;
                if (diff < 0) {
                    diff = -diff;
                }
                if (diff > 255) {
                    diff = 255;
                }
                table_index = diff;
                if (use_carry) {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = accum_scaled[n] - target_scaled;
                    varerr_diffuse_carry(carry_curr, carry_next, carry_far,
                                         width, height, depth,
                                         x, y, error_scaled,
                                         table_index,
                                         direction, n);
                } else {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = sample_scaled[n] - target_scaled;
                    varerr_diffuse(data + n, width, height,
                                   x, y, depth, error_scaled,
                                   table_index,
                                   direction);
                }
            }
        }
        if (use_carry) {
            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(int32_t));
            }
        }
    }

    if (foptimize_palette) {
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
    } else {
        *ncolors = reqcolor;
    }

    status = SIXEL_OK;

end:
    free(carry_next);
    free(carry_curr);
    free(carry_far);
    return status;
}


static SIXELSTATUS
sixel_dither_apply_fixed(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int methodForScan,
    int foptimize_palette,
    int (*f_lookup)(const unsigned char *pixel,
                    int depth,
                    const unsigned char *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion),
    unsigned short *indextable,
    int complexion,
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors,
    int methodForDiffuse,
    int methodForCarry)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    SIXELSTATUS status = SIXEL_FALSE;
    int serpentine;
    int y;
    void (*f_diffuse)(unsigned char *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      int offset,
                      int direction);
    diffuse_fixed_carry_mode f_diffuse_carry;
    int use_carry;
    size_t carry_len;
    int32_t *carry_curr = NULL;
    int32_t *carry_next = NULL;
    int32_t *carry_far = NULL;
    unsigned char corrected[max_channels];
    int32_t accum_scaled[max_channels];
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    size_t carry_base;
    const unsigned char *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int64_t accum;
    int64_t clamped;
    int32_t target_scaled;
    int32_t error_scaled;
    int offset;
    int32_t *tmp;

    if (depth > max_channels) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    use_carry = (methodForCarry == SIXEL_CARRY_ENABLE);
    carry_len = 0;

    if (depth != 3) {
        f_diffuse = diffuse_none;
        f_diffuse_carry = diffuse_none_carry;
        use_carry = 0;
    } else {
        switch (methodForDiffuse) {
        case SIXEL_DIFFUSE_NONE:
            f_diffuse = diffuse_none;
            f_diffuse_carry = diffuse_none_carry;
            break;
        case SIXEL_DIFFUSE_ATKINSON:
            f_diffuse = diffuse_atkinson;
            f_diffuse_carry = diffuse_atkinson_carry;
            break;
        case SIXEL_DIFFUSE_FS:
            f_diffuse = diffuse_fs;
            f_diffuse_carry = diffuse_fs_carry;
            break;
        case SIXEL_DIFFUSE_JAJUNI:
            f_diffuse = diffuse_jajuni;
            f_diffuse_carry = diffuse_jajuni_carry;
            break;
        case SIXEL_DIFFUSE_STUCKI:
            f_diffuse = diffuse_stucki;
            f_diffuse_carry = diffuse_stucki_carry;
            break;
        case SIXEL_DIFFUSE_BURKES:
            f_diffuse = diffuse_burkes;
            f_diffuse_carry = diffuse_burkes_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA1:
            f_diffuse = diffuse_sierra1;
            f_diffuse_carry = diffuse_sierra1_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA2:
            f_diffuse = diffuse_sierra2;
            f_diffuse_carry = diffuse_sierra2_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA3:
            f_diffuse = diffuse_sierra3;
            f_diffuse_carry = diffuse_sierra3_carry;
            break;
        default:
            f_diffuse = diffuse_none;
            f_diffuse_carry = diffuse_none_carry;
            break;
        }
    }

    if (use_carry) {
        carry_len = (size_t)width * (size_t)depth;
        if (carry_len > 0) {
            carry_curr = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_curr == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            carry_next = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_next == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            carry_far = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_far == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        } else {
            use_carry = 0;
        }
    }

    serpentine = (methodForScan == SIXEL_SCAN_SERPENTINE);

    if (foptimize_palette) {
        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    } else {
        *ncolors = reqcolor;
    }

    for (y = 0; y < height; ++y) {
        scanline_params(serpentine, y, width,
                        &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            carry_base = (size_t)x * (size_t)depth;
            if (use_carry) {
                for (n = 0; n < depth; ++n) {
                    accum = ((int64_t)data[base + n]
                             << VARERR_SCALE_SHIFT)
                           + carry_curr[carry_base + (size_t)n];
                    if (accum < INT32_MIN) {
                        accum = INT32_MIN;
                    } else if (accum > INT32_MAX) {
                        accum = INT32_MAX;
                    }
                    clamped = accum;
                    if (clamped < 0) {
                        clamped = 0;
                    } else if (clamped > VARERR_MAX_VALUE) {
                        clamped = VARERR_MAX_VALUE;
                    }
                    accum_scaled[n] = (int32_t)clamped;
                    corrected[n]
                        = (unsigned char)((clamped + VARERR_ROUND)
                                          >> VARERR_SCALE_SHIFT);
                    data[base + n] = corrected[n];
                    carry_curr[carry_base + (size_t)n] = 0;
                }
                source_pixel = corrected;
            } else {
                source_pixel = data + base;
            }

            color_index = f_lookup(source_pixel, depth, palette,
                                   reqcolor, indextable,
                                   complexion);

            if (foptimize_palette) {
                if (migration_map[color_index] == 0) {
                    output_index = *ncolors;
                    for (n = 0; n < depth; ++n) {
                        new_palette[output_index * depth + n]
                            = palette[color_index * depth + n];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    output_index = migration_map[color_index] - 1;
                }
                result[pos] = output_index;
            } else {
                output_index = color_index;
                result[pos] = output_index;
            }

            for (n = 0; n < depth; ++n) {
                if (foptimize_palette) {
                    palette_value = new_palette[output_index * depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                }
                if (use_carry) {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = accum_scaled[n] - target_scaled;
                    f_diffuse_carry(carry_curr, carry_next, carry_far,
                                    width, height, depth,
                                    x, y, error_scaled, direction, n);
                } else {
                    offset = (int)source_pixel[n] - palette_value;
                    f_diffuse(data + n, width, height, x, y,
                              depth, offset, direction);
                }
            }
        }
        if (use_carry) {
            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(int32_t));
            }
        }
    }

    if (foptimize_palette) {
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
    }

    status = SIXEL_OK;

end:
    free(carry_far);
    free(carry_next);
    free(carry_curr);
    return status;
}


static void
diffuse_none(unsigned char *data, int width, int height,
             int x, int y, int depth, int error, int direction)
{
    /* unused */ (void) data;
    /* unused */ (void) width;
    /* unused */ (void) height;
    /* unused */ (void) x;
    /* unused */ (void) y;
    /* unused */ (void) depth;
    /* unused */ (void) error;
    /* unused */ (void) direction;
}


static void
diffuse_none_carry(int32_t *carry_curr, int32_t *carry_next,
                   int32_t *carry_far, int width, int height,
                   int depth, int x, int y, int32_t error,
                   int direction, int channel)
{
    /* unused */ (void) carry_curr;
    /* unused */ (void) carry_next;
    /* unused */ (void) carry_far;
    /* unused */ (void) width;
    /* unused */ (void) height;
    /* unused */ (void) depth;
    /* unused */ (void) x;
    /* unused */ (void) y;
    /* unused */ (void) error;
    /* unused */ (void) direction;
    /* unused */ (void) channel;
}


static void
diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int error, int direction)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/16    1/16
     */
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_normal(data, pos + 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 1, 16);
            }
        }
    } else {
        if (x > 0) {
            error_diffuse_normal(data, pos - 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 1, 16);
            }
        }
    }
}


static void
diffuse_fs_carry(int32_t *carry_curr, int32_t *carry_next,
                 int32_t *carry_far, int width, int height,
                 int depth, int x, int y, int32_t error,
                 int direction, int channel)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    int forward;

    /* unused */ (void) carry_far;
    if (error == 0) {
        return;
    }

    forward = direction >= 0;
    if (forward) {
        if (x + 1 < width) {
            size_t base;
            int32_t term;

            base = ((size_t)(x + 1) * (size_t)depth)
                 + (size_t)channel;
            term = diffuse_fixed_term(error, 7, 16);
            carry_curr[base] += term;
        }
        if (y + 1 < height) {
            if (x > 0) {
                size_t base;
                int32_t term;

                base = ((size_t)(x - 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 3, 16);
                carry_next[base] += term;
            }
            {
                size_t base;
                int32_t term;

                base = ((size_t)x * (size_t)depth) + (size_t)channel;
                term = diffuse_fixed_term(error, 5, 16);
                carry_next[base] += term;
            }
            if (x + 1 < width) {
                size_t base;
                int32_t term;

                base = ((size_t)(x + 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 1, 16);
                carry_next[base] += term;
            }
        }
    } else {
        if (x - 1 >= 0) {
            size_t base;
            int32_t term;

            base = ((size_t)(x - 1) * (size_t)depth)
                 + (size_t)channel;
            term = diffuse_fixed_term(error, 7, 16);
            carry_curr[base] += term;
        }
        if (y + 1 < height) {
            if (x + 1 < width) {
                size_t base;
                int32_t term;

                base = ((size_t)(x + 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 3, 16);
                carry_next[base] += term;
            }
            {
                size_t base;
                int32_t term;

                base = ((size_t)x * (size_t)depth) + (size_t)channel;
                term = diffuse_fixed_term(error, 5, 16);
                carry_next[base] += term;
            }
            if (x - 1 >= 0) {
                size_t base;
                int32_t term;

                base = ((size_t)(x - 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 1, 16);
                carry_next[base] += term;
            }
        }
    }
}


static void
diffuse_atkinson(unsigned char *data, int width, int height,
                 int x, int y, int depth, int error, int direction)
{
    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    int pos;
    int sign;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    if (x + sign >= 0 && x + sign < width) {
        error_diffuse_fast(data, pos + sign, depth, error, 1, 8);
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        error_diffuse_fast(data, pos + sign * 2, depth, error, 1, 8);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_fast(data,
                               row + (-sign),
                               depth, error, 1, 8);
        }
        error_diffuse_fast(data, row, depth, error, 1, 8);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_fast(data,
                               row + sign,
                               depth, error, 1, 8);
        }
    }
    if (y < height - 2) {
        error_diffuse_fast(data, pos + width * 2, depth, error, 1, 8);
    }
}


static void
diffuse_atkinson_carry(int32_t *carry_curr, int32_t *carry_next,
                       int32_t *carry_far, int width, int height,
                       int depth, int x, int y, int32_t error,
                       int direction, int channel)
{
    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    int sign;
    int32_t term;

    if (error == 0) {
        return;
    }

    term = diffuse_fixed_term(error, 1, 8);
    sign = direction >= 0 ? 1 : -1;
    if (x + sign >= 0 && x + sign < width) {
        size_t base;

        base = ((size_t)(x + sign) * (size_t)depth)
             + (size_t)channel;
        carry_curr[base] += term;
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        size_t base;

        base = ((size_t)(x + sign * 2) * (size_t)depth)
             + (size_t)channel;
        carry_curr[base] += term;
    }
    if (y + 1 < height) {
        if (x - sign >= 0 && x - sign < width) {
            size_t base;

            base = ((size_t)(x - sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term;
        }
        {
            size_t base;

            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term;
        }
        if (x + sign >= 0 && x + sign < width) {
            size_t base;

            base = ((size_t)(x + sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term;
        }
    }
    if (y + 2 < height) {
        size_t base;

        base = ((size_t)x * (size_t)depth) + (size_t)channel;
        carry_far[base] += term;
    }
}


static void
diffuse_jajuni(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_weights[] = { 7, 5 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_weights[] = { 3, 5, 7, 5, 3 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_weights[] = { 1, 3, 5, 3, 1 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_weights[i], 48);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_weights[i], 48);
        }
    }
    if (y < height - 2) {
        int row;

        row = pos + width * 2;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_weights[i], 48);
        }
    }
}


static void
diffuse_jajuni_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_weights[] = { 7, 5 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_weights[] = { 3, 5, 7, 5, 3 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_weights[] = { 1, 3, 5, 3, 1 };
    int sign;
    int i;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_weights[i], 48);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_weights[i], 48);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_weights[i], 48);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static void
diffuse_stucki(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        int row;

        row = pos + width * 2;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
diffuse_stucki_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int sign;
    int i;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static void
diffuse_burkes(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
                             pos + (neighbor - x),
                             depth, error,
                             row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}

static void
diffuse_burkes_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int sign;
    int i;

    /* unused */ (void) carry_far;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
}

static void
diffuse_sierra1(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 1; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
                             pos + (neighbor - x),
                             depth, error,
                             row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 2; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}


static void
diffuse_sierra1_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    /* unused */ (void) carry_far;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 1; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 2; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
}


static void
diffuse_sierra2(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
diffuse_sierra2_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static void
diffuse_sierra3(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
diffuse_sierra3_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static float
mask_a (int x, int y, int c)
{
    return ((((x + c * 67) + y * 236) * 119) & 255 ) / 128.0 - 1.0;
}

static float
mask_x (int x, int y, int c)
{
    return ((((x + c * 29) ^ y* 149) * 1234) & 511 ) / 256.0 - 1.0;
}

/* lookup closest color from palette with "normal" strategy */
static int
lookup_normal(unsigned char const * const pixel,
              int const depth,
              unsigned char const * const palette,
              int const reqcolor,
              unsigned short * const cachetable,
              int const complexion)
{
    int result;
    int diff;
    int r;
    int i;
    int n;
    int distant;

    result = (-1);
    diff = INT_MAX;

    /* don't use cachetable in 'normal' strategy */
    (void) cachetable;

    for (i = 0; i < reqcolor; i++) {
        distant = 0;
        r = pixel[0] - palette[i * depth + 0];
        distant += r * r * complexion;
        for (n = 1; n < depth; ++n) {
            r = pixel[n] - palette[i * depth + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    return result;
}


/*
 * Shared fast lookup flow handled by the lut module.  The palette lookup now
 * delegates to sixel_lut_map_pixel() so policy-specific caches and the
 * certification tree stay encapsulated inside src/lut.c.
 */

static int
lookup_fast_lut(unsigned char const * const pixel,
                int const depth,
                unsigned char const * const palette,
                int const reqcolor,
                unsigned short * const cachetable,
                int const complexion)
{
    (void)depth;
    (void)palette;
    (void)reqcolor;
    (void)cachetable;
    (void)complexion;

    if (dither_lut_context == NULL) {
        return 0;
    }

    return sixel_lut_map_pixel(dither_lut_context, pixel);
}


static int
lookup_mono_darkbg(unsigned char const * const pixel,
                   int const depth,
                   unsigned char const * const palette,
                   int const reqcolor,
                   unsigned short * const cachetable,
                   int const complexion)
{
    int n;
    int distant;

    /* unused */ (void) palette;
    /* unused */ (void) cachetable;
    /* unused */ (void) complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant >= 128 * reqcolor ? 1: 0;
}


static int
lookup_mono_lightbg(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion)
{
    int n;
    int distant;

    /* unused */ (void) palette;
    /* unused */ (void) cachetable;
    /* unused */ (void) complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant < 128 * reqcolor ? 1: 0;
}



/*
 * Apply the palette into the supplied pixel buffer while coordinating the
 * dithering strategy.  The routine performs the following steps:
 *   - Select an index lookup helper, enabling the fast LUT path when the
 *     caller requested palette optimization and the input pixels are RGB.
 *   - Ensure the LUT object is prepared for the active policy, including
 *     weight selection for CERTLUT so complexion aware lookups remain
 *     accurate.
 *   - Dispatch to the positional, variable, or fixed error diffusion
 *     routines, each of which expects the LUT context to be initialized
 *     beforehand.
 *   - Release any temporary LUT references that were acquired for the fast
 *     lookup path.
 */
static SIXELSTATUS
sixel_dither_map_pixels(
    sixel_index_t     /* out */ *result,
    unsigned char     /* in */  *data,
    int               /* in */  width,
    int               /* in */  height,
    int               /* in */  depth,
    unsigned char     /* in */  *palette,
    int               /* in */  reqcolor,
    int               /* in */  methodForDiffuse,
    int               /* in */  methodForScan,
    int               /* in */  methodForCarry,
    int               /* in */  foptimize,
    int               /* in */  foptimize_palette,
    int               /* in */  complexion,
    int               /* in */  lut_policy,
    int               /* in */  method_for_largest,
    sixel_lut_t       /* in */  *lut,
    int               /* in */  *ncolors,
    sixel_allocator_t /* in */  *allocator)
{
#if _MSC_VER
    enum { max_depth = 4 };
#else
    const size_t max_depth = 4;
#endif
    unsigned char copy[max_depth];
    SIXELSTATUS status = SIXEL_FALSE;
    int sum1;
    int sum2;
    int n;
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    unsigned short migration_map[SIXEL_PALETTE_MAX];
    int (*f_lookup)(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion) = lookup_normal;
    int use_varerr;
    int use_positional;
    int carry_mode;
    sixel_lut_t *active_lut;
    int manage_lut;
    int policy;
    int wR;
    int wG;
    int wB;

    active_lut = NULL;
    manage_lut = 0;

    if (reqcolor < 1) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: "
            "a bad argument is detected, reqcolor < 0.");
        goto end;
    }

    use_varerr = (depth == 3
                  && methodForDiffuse == SIXEL_DIFFUSE_LSO2);
    use_positional = (methodForDiffuse == SIXEL_DIFFUSE_A_DITHER
                      || methodForDiffuse == SIXEL_DIFFUSE_X_DITHER);
    carry_mode = (methodForCarry == SIXEL_CARRY_ENABLE)
               ? SIXEL_CARRY_ENABLE
               : SIXEL_CARRY_DISABLE;

    if (reqcolor == 2) {
        sum1 = 0;
        sum2 = 0;
        for (n = 0; n < depth; ++n) {
            sum1 += palette[n];
        }
        for (n = depth; n < depth + depth; ++n) {
            sum2 += palette[n];
        }
        if (sum1 == 0 && sum2 == 255 * 3) {
            f_lookup = lookup_mono_darkbg;
        } else if (sum1 == 255 * 3 && sum2 == 0) {
            f_lookup = lookup_mono_lightbg;
        }
    }
    if (foptimize && depth == 3 && f_lookup == lookup_normal) {
        f_lookup = lookup_fast_lut;
    }

    if (f_lookup == lookup_fast_lut) {
        if (depth != 3) {
            status = SIXEL_BAD_ARGUMENT;
            sixel_helper_set_additional_message(
                "sixel_dither_map_pixels: fast lookup requires RGB pixels.");
            goto end;
        }
        policy = lut_policy;
        if (policy != SIXEL_LUT_POLICY_CERTLUT
            && policy != SIXEL_LUT_POLICY_5BIT
            && policy != SIXEL_LUT_POLICY_6BIT) {
            policy = SIXEL_LUT_POLICY_6BIT;
        }
        if (lut == NULL) {
            status = sixel_lut_new(&active_lut, policy, allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            manage_lut = 1;
        } else {
            active_lut = lut;
            manage_lut = 0;
        }
        if (policy == SIXEL_LUT_POLICY_CERTLUT) {
            if (method_for_largest == SIXEL_LARGE_LUM) {
                wR = complexion * 299;
                wG = 587;
                wB = 114;
            } else {
                wR = complexion;
                wG = 1;
                wB = 1;
            }
        } else {
            wR = complexion;
            wG = 1;
            wB = 1;
        }
        status = sixel_lut_configure(active_lut,
                                     palette,
                                     depth,
                                     reqcolor,
                                     complexion,
                                     wR,
                                     wG,
                                     wB,
                                     policy);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        dither_lut_context = active_lut;
    }

    if (use_positional) {
        status = sixel_dither_apply_positional(result, data, width, height,
                                          depth, palette, reqcolor,
                                          methodForDiffuse, methodForScan,
                                          foptimize_palette, f_lookup,
                                          NULL, complexion, copy,
                                          new_palette, migration_map,
                                          ncolors);
    } else if (use_varerr) {
        status = sixel_dither_apply_variable(result, data, width, height,
                                        depth, palette, reqcolor,
                                        methodForScan, foptimize_palette,
                                        f_lookup, NULL, complexion,
                                        new_palette, migration_map,
                                        ncolors,
                                        methodForDiffuse,
                                        carry_mode);
    } else {
        status = sixel_dither_apply_fixed(result, data, width, height,
                                     depth, palette, reqcolor,
                                     methodForScan, foptimize_palette,
                                     f_lookup, NULL, complexion,
                                     new_palette, migration_map,
                                     ncolors, methodForDiffuse,
                                     carry_mode);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (dither_lut_context != NULL && f_lookup == lookup_fast_lut) {
        dither_lut_context = NULL;
    }
    if (manage_lut && active_lut != NULL) {
        sixel_lut_unref(active_lut);
    }
    return status;
}


/*
 * Helper that detects whether the palette currently matches either of the
 * builtin monochrome definitions.  These tables skip cache initialization
 * during fast-path dithering because they already match the terminal
 * defaults.
 */
static int
sixel_palette_is_builtin_mono(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0;
    }
    if (palette->entries == NULL) {
        return 0;
    }
    if (palette->entry_count < 2U) {
        return 0;
    }
    if (palette->depth != 3) {
        return 0;
    }
    if (memcmp(palette->entries, pal_mono_dark,
               sizeof(pal_mono_dark)) == 0) {
        return 1;
    }
    if (memcmp(palette->entries, pal_mono_light,
               sizeof(pal_mono_light)) == 0) {
        return 1;
    }
    return 0;
}

/*
 * Route palette application through the local dithering helper.  The
 * function keeps all state in the palette object so we can share cache
 * buffers between invocations and later stages.  The flow is:
 *   1. Synchronize the quantizer configuration with the dither object so the
 *      LUT builder honors the requested policy.
 *   2. Invoke sixel_dither_map_pixels() to populate the index buffer and
 *      record the resulting palette size.
 *   3. Return the status to the caller so palette application errors can be
 *      reported at a single site.
 */
static SIXELSTATUS
sixel_dither_resolve_indexes(sixel_index_t *result,
                             unsigned char *data,
                             int width,
                             int height,
                             int depth,
                             sixel_palette_t *palette,
                             int reqcolor,
                             int method_for_diffuse,
                             int method_for_scan,
                             int method_for_carry,
                             int foptimize,
                             int foptimize_palette,
                             int complexion,
                             int lut_policy,
                             int method_for_largest,
                             int *ncolors,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (palette == NULL || palette->entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_palette_set_lut_policy(lut_policy);
    sixel_palette_set_method_for_largest(method_for_largest);

    status = sixel_dither_map_pixels(result,
                                       data,
                                       width,
                                       height,
                                       depth,
                                       palette->entries,
                                       reqcolor,
                                       method_for_diffuse,
                                       method_for_scan,
                                       method_for_carry,
                                       foptimize,
                                       foptimize_palette,
                                       complexion,
                                       lut_policy,
                                       method_for_largest,
                                       palette->lut,
                                       ncolors,
                                       allocator);

    return status;
}


/*
 * VT340 undocumented behavior regarding the color palette reported
 * by Vertis Sidus(@vrtsds):
 *     it loads the first fifteen colors as 1 through 15, and loads the
 *     sixteenth color as 0.
 */
static const unsigned char pal_vt340_mono[] = {
    /* 1   Gray-2   */  13 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 2   Gray-4   */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 3   Gray-6   */  40 * 255 / 100, 40 * 255 / 100, 40 * 255 / 100,
    /* 4   Gray-1   */   6 * 255 / 100,  6 * 255 / 100,  6 * 255 / 100,
    /* 5   Gray-3   */  20 * 255 / 100, 20 * 255 / 100, 20 * 255 / 100,
    /* 6   Gray-5   */  33 * 255 / 100, 33 * 255 / 100, 33 * 255 / 100,
    /* 7   White 7  */  46 * 255 / 100, 46 * 255 / 100, 46 * 255 / 100,
    /* 8   Black 0  */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
    /* 9   Gray-2   */  13 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 10  Gray-4   */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 11  Gray-6   */  40 * 255 / 100, 40 * 255 / 100, 40 * 255 / 100,
    /* 12  Gray-1   */   6 * 255 / 100,  6 * 255 / 100,  6 * 255 / 100,
    /* 13  Gray-3   */  20 * 255 / 100, 20 * 255 / 100, 20 * 255 / 100,
    /* 14  Gray-5   */  33 * 255 / 100, 33 * 255 / 100, 33 * 255 / 100,
    /* 15  White 7  */  46 * 255 / 100, 46 * 255 / 100, 46 * 255 / 100,
    /* 0   Black    */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
};


static const unsigned char pal_vt340_color[] = {
    /* 1   Blue     */  20 * 255 / 100, 20 * 255 / 100, 80 * 255 / 100,
    /* 2   Red      */  80 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 3   Green    */  20 * 255 / 100, 80 * 255 / 100, 20 * 255 / 100,
    /* 4   Magenta  */  80 * 255 / 100, 20 * 255 / 100, 80 * 255 / 100,
    /* 5   Cyan     */  20 * 255 / 100, 80 * 255 / 100, 80 * 255 / 100,
    /* 6   Yellow   */  80 * 255 / 100, 80 * 255 / 100, 20 * 255 / 100,
    /* 7   Gray 50% */  53 * 255 / 100, 53 * 255 / 100, 53 * 255 / 100,
    /* 8   Gray 25% */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 9   Blue*    */  33 * 255 / 100, 33 * 255 / 100, 60 * 255 / 100,
    /* 10  Red*     */  60 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 11  Green*   */  33 * 255 / 100, 60 * 255 / 100, 33 * 255 / 100,
    /* 12  Magenta* */  60 * 255 / 100, 33 * 255 / 100, 60 * 255 / 100,
    /* 13  Cyan*    */  33 * 255 / 100, 60 * 255 / 100, 60 * 255 / 100,
    /* 14  Yellow*  */  60 * 255 / 100, 60 * 255 / 100, 33 * 255 / 100,
    /* 15  Gray 75% */  80 * 255 / 100, 80 * 255 / 100, 80 * 255 / 100,
    /* 0   Black    */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
};


/* create dither context object */
SIXELAPI SIXELSTATUS
sixel_dither_new(
    sixel_dither_t    /* out */ **ppdither, /* dither object to be created */
    int               /* in */  ncolors,    /* required colors */
    sixel_allocator_t /* in */  *allocator) /* allocator, null if you use
                                               default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t headsize;
    int quality_mode;
    sixel_palette_t *palette;

    /* ensure given pointer is not null */
    if (ppdither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_new: ppdither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    *ppdither = NULL;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *ppdither = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    if (ncolors < 0) {
        ncolors = SIXEL_PALETTE_MAX;
        quality_mode = SIXEL_QUALITY_HIGHCOLOR;
    } else {
        if (ncolors > SIXEL_PALETTE_MAX) {
            status = SIXEL_BAD_INPUT;
            goto end;
        } else if (ncolors < 1) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "sixel_dither_new: palette colors must be more than 0");
            goto end;
        }
        quality_mode = SIXEL_QUALITY_LOW;
    }
    headsize = sizeof(sixel_dither_t);

    *ppdither = (sixel_dither_t *)sixel_allocator_malloc(allocator, headsize);
    if (*ppdither == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_dither_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppdither)->ref = 1U;
    (*ppdither)->palette = NULL;
    (*ppdither)->reqcolors = ncolors;
    (*ppdither)->force_palette = 0;
    (*ppdither)->ncolors = ncolors;
    (*ppdither)->origcolors = (-1);
    (*ppdither)->keycolor = (-1);
    (*ppdither)->optimized = 0;
    (*ppdither)->optimize_palette = 0;
    (*ppdither)->complexion = 1;
    (*ppdither)->bodyonly = 0;
    (*ppdither)->method_for_largest = SIXEL_LARGE_NORM;
    (*ppdither)->method_for_rep = SIXEL_REP_CENTER_BOX;
    (*ppdither)->method_for_diffuse = SIXEL_DIFFUSE_FS;
    (*ppdither)->method_for_scan = SIXEL_SCAN_AUTO;
    (*ppdither)->method_for_carry = SIXEL_CARRY_AUTO;
    (*ppdither)->quality_mode = quality_mode;
    (*ppdither)->requested_quality_mode = quality_mode;
    (*ppdither)->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    (*ppdither)->allocator = allocator;
    (*ppdither)->lut_policy = SIXEL_LUT_POLICY_AUTO;
    (*ppdither)->sixel_reversible = 0;
    (*ppdither)->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    (*ppdither)->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;

    status = sixel_palette_new(&(*ppdither)->palette, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *ppdither);
        *ppdither = NULL;
        goto end;
    }

    palette = (*ppdither)->palette;
    palette->requested_colors = (unsigned int)ncolors;
    palette->quality_mode = quality_mode;
    palette->force_palette = 0;
    palette->lut_policy = SIXEL_LUT_POLICY_AUTO;

    status = sixel_palette_resize(palette,
                                  (unsigned int)ncolors,
                                  3,
                                  allocator);
    if (SIXEL_FAILED(status)) {
        sixel_palette_unref(palette);
        (*ppdither)->palette = NULL;
        sixel_allocator_free(allocator, *ppdither);
        *ppdither = NULL;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
    }
    return status;
}


/* create dither context object (deprecated) */
SIXELAPI sixel_dither_t *
sixel_dither_create(
    int     /* in */ ncolors)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_dither_t *dither = NULL;

    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return dither;
}


SIXELAPI void
sixel_dither_destroy(
    sixel_dither_t  /* in */ *dither)
{
    sixel_allocator_t *allocator;

    if (dither) {
        allocator = dither->allocator;
        if (dither->palette != NULL) {
            sixel_palette_unref(dither->palette);
            dither->palette = NULL;
        }
        sixel_allocator_free(allocator, dither);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_dither_ref(
    sixel_dither_t  /* in */ *dither)
{
    /* TODO: be thread safe */
    ++dither->ref;
}


SIXELAPI void
sixel_dither_unref(
    sixel_dither_t  /* in */ *dither)
{
    /* TODO: be thread safe */
    if (dither != NULL && --dither->ref == 0) {
        sixel_dither_destroy(dither);
    }
}


SIXELAPI sixel_dither_t *
sixel_dither_get(
    int     /* in */ builtin_dither)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *palette;
    int ncolors;
    int keycolor;
    sixel_dither_t *dither = NULL;

    switch (builtin_dither) {
    case SIXEL_BUILTIN_MONO_DARK:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_dark;
        keycolor = 0;
        break;
    case SIXEL_BUILTIN_MONO_LIGHT:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_light;
        keycolor = 0;
        break;
    case SIXEL_BUILTIN_XTERM16:
        ncolors = 16;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_XTERM256:
        ncolors = 256;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_VT340_MONO:
        ncolors = 16;
        palette = (unsigned char *)pal_vt340_mono;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_VT340_COLOR:
        ncolors = 16;
        palette = (unsigned char *)pal_vt340_color;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G1:
        ncolors = 2;
        palette = (unsigned char *)pal_gray_1bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G2:
        ncolors = 4;
        palette = (unsigned char *)pal_gray_2bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G4:
        ncolors = 16;
        palette = (unsigned char *)pal_gray_4bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G8:
        ncolors = 256;
        palette = (unsigned char *)pal_gray_8bit;
        keycolor = (-1);
        break;
    default:
        goto end;
    }

    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        dither = NULL;
        goto end;
    }

    status = sixel_palette_set_entries(dither->palette,
                                       palette,
                                       (unsigned int)ncolors,
                                       3,
                                       dither->allocator);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(dither);
        dither = NULL;
        goto end;
    }
    dither->palette->requested_colors = (unsigned int)ncolors;
    dither->palette->entry_count = (unsigned int)ncolors;
    dither->palette->depth = 3;
    dither->keycolor = keycolor;
    dither->optimized = 1;
    dither->optimize_palette = 0;

end:
    return dither;
}


static void
sixel_dither_set_method_for_largest(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_largest)
{
    if (method_for_largest == SIXEL_LARGE_AUTO) {
        method_for_largest = SIXEL_LARGE_NORM;
    }
    dither->method_for_largest = method_for_largest;
}


static void
sixel_dither_set_method_for_rep(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_rep)
{
    if (method_for_rep == SIXEL_REP_AUTO) {
        method_for_rep = SIXEL_REP_CENTER_BOX;
    }
    dither->method_for_rep = method_for_rep;
}


static void
sixel_dither_set_quality_mode(
    sixel_dither_t  /* in */  *dither,
    int             /* in */  quality_mode)
{
    dither->requested_quality_mode = quality_mode;

    if (quality_mode == SIXEL_QUALITY_AUTO) {
        if (dither->ncolors <= 8) {
            quality_mode = SIXEL_QUALITY_HIGH;
        } else {
            quality_mode = SIXEL_QUALITY_LOW;
        }
    }
    dither->quality_mode = quality_mode;
}


SIXELAPI SIXELSTATUS
sixel_dither_initialize(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *data,
    int             /* in */ width,
    int             /* in */ height,
    int             /* in */ pixelformat,
    int             /* in */ method_for_largest,
    int             /* in */ method_for_rep,
    int             /* in */ quality_mode)
{
    unsigned char *buf = NULL;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;
    SIXELSTATUS status = SIXEL_FALSE;

    /* ensure dither object is not null */
    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_new: dither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* increment ref count */
    sixel_dither_ref(dither);

    sixel_dither_set_pixelformat(dither, pixelformat);

    /* keep quantizer policy in sync with the dither object */
    sixel_palette_set_lut_policy(dither->lut_policy);

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        input_pixels = data;
        break;
    default:
        /* normalize pixelformat */
        normalized_pixels
            = (unsigned char *)sixel_allocator_malloc(
                dither->allocator, (size_t)(width * height * 3));
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_initialize: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        status = sixel_helper_normalize_pixelformat(
            normalized_pixels,
            &pixelformat,
            data,
            pixelformat,
            width,
            height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        input_pixels = normalized_pixels;
        break;
    }

    sixel_dither_set_method_for_largest(dither, method_for_largest);
    sixel_dither_set_method_for_rep(dither, method_for_rep);
    sixel_dither_set_quality_mode(dither, quality_mode);

    status = sixel_palette_make_palette(&buf,
                                        input_pixels,
                                        (unsigned int)(width * height * 3),
                                        SIXEL_PIXELFORMAT_RGB888,
                                        (unsigned int)dither->reqcolors,
                                        (unsigned int *)&dither->ncolors,
                                        (unsigned int *)&dither->origcolors,
                                        dither->method_for_largest,
                                        dither->method_for_rep,
                                        dither->quality_mode,
                                        dither->force_palette,
                                        dither->sixel_reversible,
                                        dither->quantize_model,
                                        dither->final_merge_mode,
                                        dither->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_palette_set_entries(dither->palette,
                                       buf,
                                       (unsigned int)dither->ncolors,
                                       3,
                                       dither->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    dither->palette->entry_count = (unsigned int)dither->ncolors;
    dither->palette->requested_colors = (unsigned int)dither->reqcolors;
    dither->palette->original_colors = (unsigned int)dither->origcolors;
    dither->palette->depth = 3;

    dither->optimized = 1;
    if (dither->origcolors <= dither->ncolors) {
        dither->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }

    sixel_palette_free_palette(buf, dither->allocator);
    status = SIXEL_OK;

end:
    if (normalized_pixels != NULL) {
        sixel_allocator_free(dither->allocator, normalized_pixels);
    }

    /* decrement ref count */
    sixel_dither_unref(dither);

    return status;
}


/* set lookup table policy */
SIXELAPI void
sixel_dither_set_lut_policy(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ lut_policy)
{
    int normalized;
    int previous_policy;

    if (dither == NULL) {
        return;
    }

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT
        || lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        normalized = lut_policy;
    }
    previous_policy = dither->lut_policy;
    if (previous_policy == normalized) {
        return;
    }

    /*
     * Policy transitions for the shared LUT mirror the previous cache flow:
     *
     *   [lut] --policy change--> (drop) --rebuild--> [lut]
     */
    dither->lut_policy = normalized;
    if (dither->palette != NULL) {
        dither->palette->lut_policy = normalized;
    }
    if (dither->palette != NULL && dither->palette->lut != NULL) {
        sixel_lut_unref(dither->palette->lut);
        dither->palette->lut = NULL;
    }
}


/* get lookup table policy */
SIXELAPI int
sixel_dither_get_lut_policy(
    sixel_dither_t  /* in */ *dither)
{
    int policy;

    policy = SIXEL_LUT_POLICY_AUTO;
    if (dither != NULL) {
        policy = dither->lut_policy;
    }

    return policy;
}


/* set diffusion type, choose from enum methodForDiffuse */
SIXELAPI void
sixel_dither_set_diffusion_type(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_diffuse)
{
    if (method_for_diffuse == SIXEL_DIFFUSE_AUTO) {
        if (dither->ncolors > 16) {
            method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else {
            method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        }
    }
    dither->method_for_diffuse = method_for_diffuse;
}


/* set scan order for diffusion */
SIXELAPI void
sixel_dither_set_diffusion_scan(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_scan)
{
    if (method_for_scan != SIXEL_SCAN_AUTO &&
            method_for_scan != SIXEL_SCAN_RASTER &&
            method_for_scan != SIXEL_SCAN_SERPENTINE) {
        method_for_scan = SIXEL_SCAN_RASTER;
    }
    dither->method_for_scan = method_for_scan;
}


/* set carry buffer mode for diffusion */
SIXELAPI void
sixel_dither_set_diffusion_carry(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_carry)
{
    if (method_for_carry != SIXEL_CARRY_AUTO &&
            method_for_carry != SIXEL_CARRY_DISABLE &&
            method_for_carry != SIXEL_CARRY_ENABLE) {
        method_for_carry = SIXEL_CARRY_AUTO;
    }
    dither->method_for_carry = method_for_carry;
}


/* get number of palette colors */
SIXELAPI int
sixel_dither_get_num_of_palette_colors(
    sixel_dither_t  /* in */ *dither)
{
    return dither->ncolors;
}


/* get number of histogram colors */
SIXELAPI int
sixel_dither_get_num_of_histogram_colors(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    return dither->origcolors;
}


/* typoed: remained for keeping compatibility */
SIXELAPI int
sixel_dither_get_num_of_histgram_colors(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    return sixel_dither_get_num_of_histogram_colors(dither);
}


/* get palette */
SIXELAPI unsigned char *
sixel_dither_get_palette(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    if (dither == NULL || dither->palette == NULL) {
        return NULL;
    }

    return sixel_palette_get_entries(dither->palette);
}


/* set palette */
SIXELAPI void
sixel_dither_set_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    unsigned char  /* in */ *palette)
{
    if (dither == NULL || dither->palette == NULL) {
        return;
    }

    (void)sixel_palette_set_entries(dither->palette,
                                    palette,
                                    (unsigned int)dither->ncolors,
                                    3,
                                    dither->allocator);
}


/* set the factor of complexion color correcting */
SIXELAPI void
sixel_dither_set_complexion_score(
    sixel_dither_t /* in */ *dither,  /* dither context object */
    int            /* in */ score)    /* complexion score (>= 1) */
{
    dither->complexion = score;
    if (dither->palette != NULL) {
        dither->palette->complexion = score;
    }
}


/* set whether omitting palette difinition */
SIXELAPI void
sixel_dither_set_body_only(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ bodyonly)    /* 0: output palette section
                                            1: do not output palette section  */
{
    dither->bodyonly = bodyonly;
}


/* set whether optimize palette size */
SIXELAPI void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt)    /* 0: optimize palette size
                                          1: don't optimize palette size */
{
    dither->optimize_palette = do_opt;
}


/* set pixelformat */
SIXELAPI void
sixel_dither_set_pixelformat(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ pixelformat) /* one of enum pixelFormat */
{
    dither->pixelformat = pixelformat;
}


/* toggle SIXEL reversible palette mode */
SIXELAPI void
sixel_dither_set_sixel_reversible(
    sixel_dither_t /* in */ *dither,
    int            /* in */ enable)
{
    /*
     * The diagram below shows how the flag routes palette generation:
     *
     *   pixels --> [histogram]
     *                  |
     *                  v
     *           (optional reversible snap)
     *                  |
     *                  v
     *               palette
     */
    if (dither == NULL) {
        return;
    }
    dither->sixel_reversible = enable ? 1 : 0;
    if (dither->palette != NULL) {
        dither->palette->sixel_reversible = dither->sixel_reversible;
    }
}

/* select final merge policy */
SIXELAPI void
sixel_dither_set_final_merge(
    sixel_dither_t /* in */ *dither,
    int            /* in */ final_merge)
{
    int mode;

    if (dither == NULL) {
        return;
    }
    mode = SIXEL_FINAL_MERGE_AUTO;
    if (final_merge == SIXEL_FINAL_MERGE_NONE
        || final_merge == SIXEL_FINAL_MERGE_WARD
        || final_merge == SIXEL_FINAL_MERGE_HKMEANS) {
        mode = final_merge;
    } else if (final_merge == SIXEL_FINAL_MERGE_AUTO) {
        mode = SIXEL_FINAL_MERGE_AUTO;
    }
    dither->final_merge_mode = mode;
    if (dither->palette != NULL) {
        dither->palette->final_merge = mode;
    }
}

/* set transparent */
SIXELAPI void
sixel_dither_set_transparent(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ transparent)  /* transparent color index */
{
    dither->keycolor = transparent;
}


/* set transparent */
sixel_index_t *
sixel_dither_apply_palette(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t bufsize;
    size_t normalized_size;
    sixel_index_t *dest = NULL;
    int ncolors;
    int method_for_scan;
    int method_for_carry;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;
    int palette_probe_active;
    double palette_started_at;
    double palette_finished_at;
    double palette_duration;
    sixel_palette_t *palette;

    /* ensure dither object is not null */
    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: dither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    sixel_dither_ref(dither);

    palette = dither->palette;
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: palette is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    bufsize = (size_t)(width * height) * sizeof(sixel_index_t);
    dest = (sixel_index_t *)sixel_allocator_malloc(dither->allocator, bufsize);
    if (dest == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /* if quality_mode is full, do not use palette caching */
    if (dither->quality_mode == SIXEL_QUALITY_FULL) {
        dither->optimized = 0;
    }

    if (dither->optimized) {
        if (!sixel_palette_is_builtin_mono(palette)) {
            int policy;
            int wR;
            int wG;
            int wB;

            policy = dither->lut_policy;
            if (policy != SIXEL_LUT_POLICY_CERTLUT
                && policy != SIXEL_LUT_POLICY_5BIT
                && policy != SIXEL_LUT_POLICY_6BIT) {
                policy = SIXEL_LUT_POLICY_6BIT;
            }
            if (palette->lut == NULL) {
                status = sixel_lut_new(&palette->lut,
                                       policy,
                                       palette->allocator);
                if (SIXEL_FAILED(status)) {
                    sixel_helper_set_additional_message(
                        "sixel_dither_apply_palette: lut allocation failed.");
                    goto end;
                }
            }
            if (policy == SIXEL_LUT_POLICY_CERTLUT) {
                if (dither->method_for_largest == SIXEL_LARGE_LUM) {
                    wR = dither->complexion * 299;
                    wG = 587;
                    wB = 114;
                } else {
                    wR = dither->complexion;
                    wG = 1;
                    wB = 1;
                }
            } else {
                wR = dither->complexion;
                wG = 1;
                wB = 1;
            }
            status = sixel_lut_configure(palette->lut,
                                         palette->entries,
                                         palette->depth,
                                         (int)palette->entry_count,
                                         dither->complexion,
                                         wR,
                                         wG,
                                         wB,
                                         policy);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_dither_apply_palette: lut configuration failed.");
                goto end;
            }
        }
    }

    if (dither->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        /* normalize pixelformat */
        normalized_size = (size_t)width * (size_t)height * 3U;
        normalized_pixels
            = (unsigned char *)sixel_allocator_malloc(dither->allocator,
                                                      normalized_size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_new: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &dither->pixelformat,
                                                    pixels, dither->pixelformat,
                                                    width, height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        input_pixels = normalized_pixels;
    } else {
        input_pixels = pixels;
    }

    method_for_scan = dither->method_for_scan;
    if (method_for_scan == SIXEL_SCAN_AUTO) {
        method_for_scan = SIXEL_SCAN_RASTER;
    }

    method_for_carry = dither->method_for_carry;
    if (method_for_carry == SIXEL_CARRY_AUTO) {
        method_for_carry = SIXEL_CARRY_DISABLE;
    }

    palette_probe_active = sixel_assessment_palette_probe_enabled();
    palette_started_at = 0.0;
    palette_finished_at = 0.0;
    palette_duration = 0.0;
    if (palette_probe_active) {
        /*
         * Palette spans execute inside the encode stage.  We sample the
         * duration here so the assessment can reassign the work to the
         * PaletteApply bucket.
         */
        palette_started_at = sixel_assessment_timer_now();
    }
    palette->lut_policy = dither->lut_policy;
    palette->method_for_largest = dither->method_for_largest;
    status = sixel_dither_resolve_indexes(dest,
                                          input_pixels,
                                          width,
                                          height,
                                          3,
                                          palette,
                                          dither->ncolors,
                                          dither->method_for_diffuse,
                                          method_for_scan,
                                          method_for_carry,
                                          dither->optimized,
                                          dither->optimize_palette,
                                          dither->complexion,
                                          dither->lut_policy,
                                          dither->method_for_largest,
                                          &ncolors,
                                          dither->allocator);
    if (palette_probe_active) {
        palette_finished_at = sixel_assessment_timer_now();
        palette_duration = palette_finished_at - palette_started_at;
        if (palette_duration < 0.0) {
            palette_duration = 0.0;
        }
        sixel_assessment_record_palette_apply_span(palette_duration);
    }
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(dither->allocator, dest);
        dest = NULL;
        goto end;
    }

    dither->ncolors = ncolors;
    palette->entry_count = (unsigned int)ncolors;

end:
    if (normalized_pixels != NULL) {
        sixel_allocator_free(dither->allocator, normalized_pixels);
    }
    sixel_dither_unref(dither);
    return dest;
}


#if HAVE_TESTS
static int
test1(void)
{
    sixel_dither_t *dither = NULL;
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;

    status = sixel_dither_new(&dither, 2, NULL);
    if (dither == NULL || status != SIXEL_OK) {
        goto error;
    }
    sixel_dither_ref(dither);
    sixel_dither_unref(dither);
    sixel_dither_unref(dither);
    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    return nret;
}

static int
test2(void)
{
    sixel_dither_t *dither = NULL;
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;

    status = sixel_dither_new(&dither, INT_MAX, NULL);
    if (status != SIXEL_BAD_INPUT || dither != NULL) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


SIXELAPI int
sixel_dither_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
