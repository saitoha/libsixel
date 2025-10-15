/*
 *
 * mediancut algorithm implementation is imported from pnmcolormap.c
 * in netpbm library.
 * http://netpbm.sourceforge.net/
 *
 * *******************************************************************************
 *                  original license block of pnmcolormap.c
 * *******************************************************************************
 *
 *   Derived from ppmquant, originally by Jef Poskanzer.
 *
 *   Copyright (C) 1989, 1991 by Jef Poskanzer.
 *   Copyright (C) 2001 by Bryan Henderson.
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 * ******************************************************************************
 *
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
 *
 *
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_MATH_H
#include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_MATH_H */
#if HAVE_STDINT_H
# include <stdint.h>
#else
# if HAVE_INTTYPES_H
#  include <inttypes.h>
# endif
#endif  /* HAVE_STDINT_H */

#include "quant.h"

/*
 * Random threshold modulation described by Zhou & Fang (Table 2) assigns
 * jitter amplitudes to nine key tone distances (0, 44, 64, 85, 95, 102, 107,
 * 112, 127).  We reconstruct the full 0-127 table by linearly interpolating
 * between those key levels, mirroring the result to cover 0-255.  See
 * https://observablehq.com/@jobleonard/variable-coefficient-dithering for a
 * reconstruction reference.
 * The entries below store the percentage gain (0-100) used by T(n).
 */
static const int zhoufang_threshold_gain[128] = {
        0,    1,    2,    2,    3,    4,    5,    5,    6,    7,
        8,    8,    9,   10,   11,   12,   12,   13,   14,   15,
       15,   16,   17,   18,   19,   19,   20,   21,   22,   22,
       23,   24,   25,   26,   26,   27,   28,   29,   29,   30,
       31,   32,   32,   33,   34,   35,   36,   36,   37,   38,
       39,   40,   40,   41,   42,   43,   44,   44,   45,   46,
       47,   48,   48,   49,   50,   52,   55,   57,   60,   62,
       64,   67,   69,   71,   74,   76,   79,   81,   83,   86,
       88,   90,   93,   95,   98,  100,   92,   83,   75,   67,
       59,   50,   42,   34,   25,   17,   22,   26,   31,   36,
       41,   45,   50,   54,   58,   62,   66,   70,   72,   74,
       75,   77,   79,   80,   82,   83,   85,   86,   87,   89,
       90,   92,   93,   94,   96,   97,   99,  100,
};


static const int (*
ostromoukhov_table(void))[4]
{
    /*
     * Coefficients follow Victor Ostromoukhov, "A Simple and Efficient
     * Error-Diffusion Algorithm", SIGGRAPH 2001
     * (https://perso.liris.cnrs.fr/victor.ostromoukhov/publications/pdf/
     *  SIGGRAPH01_varcoeffED.pdf).  Each tuple lists the weights for the
     * neighbours documented in diffuse_varerr_common().  The fourth element
     * matches the sum of the first three, preserving the normalization from
     * the paper.
     */
    static const int table[][4] = {
        {        13,         0,         5,        18 },
        {        13,         0,         5,        18 },
        {        21,         0,        10,        31 },
        {         7,         0,         4,        11 },
        {         8,         0,         5,        13 },
        {        47,         3,        28,        78 },
        {        23,         3,        13,        39 },
        {        15,         3,         8,        26 },
        {        22,         6,        11,        39 },
        {        43,        15,        20,        78 },
        {         7,         3,         3,        13 },
        {       501,       224,       211,       936 },
        {       249,       116,       103,       468 },
        {       165,        80,        67,       312 },
        {       123,        62,        49,       234 },
        {       489,       256,       191,       936 },
        {        81,        44,        31,       156 },
        {       483,       272,       181,       936 },
        {        60,        35,        22,       117 },
        {        53,        32,        19,       104 },
        {       237,       148,        83,       468 },
        {       471,       304,       161,       936 },
        {         3,         2,         1,         6 },
        {       459,       304,       161,       924 },
        {        38,        25,        14,        77 },
        {       453,       296,       175,       924 },
        {       225,       146,        91,       462 },
        {       149,        96,        63,       308 },
        {       111,        71,        49,       231 },
        {        63,        40,        29,       132 },
        {        73,        46,        35,       154 },
        {       435,       272,       217,       924 },
        {       108,        67,        56,       231 },
        {        13,         8,         7,        28 },
        {       213,       130,       119,       462 },
        {       423,       256,       245,       924 },
        {         5,         3,         3,        11 },
        {       281,       173,       162,       616 },
        {       141,        89,        78,       308 },
        {       283,       183,       150,       616 },
        {        71,        47,        36,       154 },
        {       285,       193,       138,       616 },
        {        13,         9,         6,        28 },
        {        41,        29,        18,        88 },
        {        36,        26,        15,        77 },
        {       289,       213,       114,       616 },
        {       145,       109,        54,       308 },
        {       291,       223,       102,       616 },
        {        73,        57,        24,       154 },
        {       293,       233,        90,       616 },
        {        21,        17,         6,        44 },
        {       295,       243,        78,       616 },
        {        37,        31,         9,        77 },
        {        27,        23,         6,        56 },
        {       149,       129,        30,       308 },
        {       299,       263,        54,       616 },
        {        75,        67,        12,       154 },
        {        43,        39,         6,        88 },
        {       151,       139,        18,       308 },
        {       303,       283,        30,       616 },
        {        38,        36,         3,        77 },
        {       305,       293,        18,       616 },
        {       153,       149,         6,       308 },
        {       307,       303,         6,       616 },
        {         1,         1,         0,         2 },
        {       101,       105,         2,       208 },
        {        49,        53,         2,       104 },
        {        95,       107,         6,       208 },
        {        23,        27,         2,        52 },
        {        89,       109,        10,       208 },
        {        43,        55,         6,       104 },
        {        83,       111,        14,       208 },
        {         5,         7,         1,        13 },
        {       172,       181,        37,       390 },
        {        97,        76,        22,       195 },
        {        72,        41,        17,       130 },
        {       119,        47,        29,       195 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {        65,        18,        17,       100 },
        {        95,        29,        26,       150 },
        {       185,        62,        53,       300 },
        {        30,        11,         9,        50 },
        {        35,        14,        11,        60 },
        {        85,        37,        28,       150 },
        {        55,        26,        19,       100 },
        {        80,        41,        29,       150 },
        {       155,        86,        59,       300 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {       305,       176,       119,       600 },
        {       155,        86,        59,       300 },
        {       105,        56,        39,       200 },
        {        80,        41,        29,       150 },
        {        65,        32,        23,       120 },
        {        55,        26,        19,       100 },
        {       335,       152,       113,       600 },
        {        85,        37,        28,       150 },
        {       115,        48,        37,       200 },
        {        35,        14,        11,        60 },
        {       355,       136,       109,       600 },
        {        30,        11,         9,        50 },
        {       365,       128,       107,       600 },
        {       185,        62,        53,       300 },
        {        25,         8,         7,        40 },
        {        95,        29,        26,       150 },
        {       385,       112,       103,       600 },
        {        65,        18,        17,       100 },
        {       395,       104,       101,       600 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {       395,       104,       101,       600 },
        {        65,        18,        17,       100 },
        {       385,       112,       103,       600 },
        {        95,        29,        26,       150 },
        {        25,         8,         7,        40 },
        {       185,        62,        53,       300 },
        {       365,       128,       107,       600 },
        {        30,        11,         9,        50 },
        {       355,       136,       109,       600 },
        {        35,        14,        11,        60 },
        {       115,        48,        37,       200 },
        {        85,        37,        28,       150 },
        {       335,       152,       113,       600 },
        {        55,        26,        19,       100 },
        {        65,        32,        23,       120 },
        {        80,        41,        29,       150 },
        {       105,        56,        39,       200 },
        {       155,        86,        59,       300 },
        {       305,       176,       119,       600 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {         5,         3,         2,        10 },
        {       155,        86,        59,       300 },
        {        80,        41,        29,       150 },
        {        55,        26,        19,       100 },
        {        85,        37,        28,       150 },
        {        35,        14,        11,        60 },
        {        30,        11,         9,        50 },
        {       185,        62,        53,       300 },
        {        95,        29,        26,       150 },
        {        65,        18,        17,       100 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {         4,         1,         1,         6 },
        {       119,        47,        29,       195 },
        {        72,        41,        17,       130 },
        {        97,        76,        22,       195 },
        {       172,       181,        37,       390 },
        {         5,         7,         1,        13 },
        {        83,       111,        14,       208 },
        {        43,        55,         6,       104 },
        {        89,       109,        10,       208 },
        {        23,        27,         2,        52 },
        {        95,       107,         6,       208 },
        {        49,        53,         2,       104 },
        {       101,       105,         2,       208 },
        {         1,         1,         0,         2 },
        {       307,       303,         6,       616 },
        {       153,       149,         6,       308 },
        {       305,       293,        18,       616 },
        {        38,        36,         3,        77 },
        {       303,       283,        30,       616 },
        {       151,       139,        18,       308 },
        {        43,        39,         6,        88 },
        {        75,        67,        12,       154 },
        {       299,       263,        54,       616 },
        {       149,       129,        30,       308 },
        {        27,        23,         6,        56 },
        {        37,        31,         9,        77 },
        {       295,       243,        78,       616 },
        {        21,        17,         6,        44 },
        {       293,       233,        90,       616 },
        {        73,        57,        24,       154 },
        {       291,       223,       102,       616 },
        {       145,       109,        54,       308 },
        {       289,       213,       114,       616 },
        {        36,        26,        15,        77 },
        {        41,        29,        18,        88 },
        {        13,         9,         6,        28 },
        {       285,       193,       138,       616 },
        {        71,        47,        36,       154 },
        {       283,       183,       150,       616 },
        {       141,        89,        78,       308 },
        {       281,       173,       162,       616 },
        {         5,         3,         3,        11 },
        {       423,       256,       245,       924 },
        {       213,       130,       119,       462 },
        {        13,         8,         7,        28 },
        {       108,        67,        56,       231 },
        {       435,       272,       217,       924 },
        {        73,        46,        35,       154 },
        {        63,        40,        29,       132 },
        {       111,        71,        49,       231 },
        {       149,        96,        63,       308 },
        {       225,       146,        91,       462 },
        {       453,       296,       175,       924 },
        {        38,        25,        14,        77 },
        {       459,       304,       161,       924 },
        {         3,         2,         1,         6 },
        {       471,       304,       161,       936 },
        {       237,       148,        83,       468 },
        {        53,        32,        19,       104 },
        {        60,        35,        22,       117 },
        {       483,       272,       181,       936 },
        {        81,        44,        31,       156 },
        {       489,       256,       191,       936 },
        {       123,        62,        49,       234 },
        {       165,        80,        67,       312 },
        {       249,       116,       103,       468 },
        {       501,       224,       211,       936 },
        {         7,         3,         3,        13 },
        {        43,        15,        20,        78 },
        {        22,         6,        11,        39 },
        {        15,         3,         8,        26 },
        {        23,         3,        13,        39 },
        {        47,         3,        28,        78 },
        {         8,         0,         5,        13 },
        {         7,         0,         4,        11 },
        {        21,         0,        10,        31 },
        {        13,         0,         5,        18 },
        {        13,         0,         5,        18 }
    };

    return table;
}

static const int (*
zhoufang_table(void))[4]
{
    /*
     * Coefficients follow Zhou & Fang, "Improving mid-tone quality of
     * variable-coefficient error diffusion using threshold modulation"
     * (https://www.wict.pku.edu.cn/GraphicsLab/docs/20200416110604416469.pdf).
     * Appendix A tabulates ratios for selected "key levels".  The discrete
     * table below reproduces the procedure used in Job Leonard's reference
     * implementation (https://observablehq.com/@jobleonard/variable-
     * coefficient-dithering).  We interpolate each interval linearly, truncate
     * to integers, and keep the first 128 tones; tones 128-255 mirror the same
     * coefficients.  The fourth element of every tuple stores the shared
     * denominator, i.e. the sum of the first three values.  All tuples use the
     * neighbour ordering described in diffuse_varerr_common().
     */
    static const int table[][4] = {
        {         13,          0,          5,         18 },
        {    1300249,          0,     499250,    1799499 },
        {     213113,        287,      99357,     312757 },
        {     351854,          0,     199965,     551819 },
        {     801100,          0,     490999,    1292099 },
        {     784929,      49577,     459781,    1294287 },
        {     768758,      99155,     428564,    1296477 },
        {     752587,     148733,     397346,    1298666 },
        {     736416,     198310,     366129,    1300855 },
        {     720245,     247888,     334911,    1303044 },
        {     704075,     297466,     303694,    1305235 },
        {     649286,     275336,     280175,    1204797 },
        {     594498,     253207,     256656,    1104361 },
        {     539709,     231078,     233137,    1003924 },
        {     484921,     208949,     209619,     903489 },
        {     430132,     186820,     186100,     803052 },
        {     375344,     164691,     162581,     702616 },
        {     320555,     142562,     139062,     602179 },
        {     265767,     120433,     115544,     501744 },
        {     210978,      98304,      92025,     401307 },
        {     156190,      76175,      68506,     300871 },
        {     101401,      54046,      44987,     200434 },
        {      46613,      31917,      21469,      99999 },
        {      46699,      31787,      21512,      99998 },
        {      46786,      31657,      21555,      99998 },
        {      46873,      31527,      21598,      99998 },
        {      46960,      31397,      21641,      99998 },
        {      47047,      31267,      21684,      99998 },
        {      47134,      31137,      21727,      99998 },
        {      47221,      31007,      21770,      99998 },
        {      47308,      30877,      21813,      99998 },
        {      47395,      30747,      21856,      99998 },
        {      47482,      30617,      21900,      99999 },
        {      47110,      31576,      21310,      99996 },
        {      46739,      32536,      20721,      99996 },
        {      46367,      33495,      20131,      99993 },
        {      45996,      34455,      19542,      99993 },
        {      45624,      35414,      18952,      99990 },
        {      45253,      36374,      18363,      99990 },
        {      44881,      37333,      17773,      99987 },
        {      44510,      38293,      17184,      99987 },
        {      44138,      39252,      16594,      99984 },
        {      43767,      40212,      16005,      99984 },
        {      43395,      41171,      15415,      99981 },
        {      43024,      42131,      14826,      99981 },
        {      42693,      42185,      15103,      99981 },
        {      42362,      42239,      15380,      99981 },
        {      42032,      42294,      15657,      99983 },
        {      41701,      42348,      15934,      99983 },
        {      41370,      42403,      16211,      99984 },
        {      41040,      42457,      16488,      99985 },
        {      40709,      42511,      16766,      99986 },
        {      40378,      42566,      17043,      99987 },
        {      40048,      42620,      17320,      99988 },
        {      39717,      42675,      17597,      99989 },
        {      39386,      42729,      17874,      99989 },
        {      39056,      42783,      18151,      99990 },
        {      38725,      42838,      18428,      99991 },
        {      38394,      42892,      18706,      99992 },
        {      38064,      42947,      18983,      99994 },
        {      37733,      43001,      19260,      99994 },
        {      37402,      43055,      19537,      99994 },
        {      37072,      43110,      19814,      99996 },
        {      36741,      43164,      20091,      99996 },
        {      36411,      43219,      20369,      99999 },
        {      36669,      44547,      18782,      99998 },
        {      36927,      45875,      17196,      99998 },
        {      37185,      47203,      15609,      99997 },
        {      37444,      48531,      14023,      99998 },
        {      37702,      49859,      12437,      99998 },
        {      37960,      51187,      10850,      99997 },
        {      38218,      52515,       9264,      99997 },
        {      38477,      53843,       7678,      99998 },
        {      38882,      53383,       7732,      99997 },
        {      39287,      52924,       7786,      99997 },
        {      39692,      52465,       7840,      99997 },
        {      40097,      52006,       7894,      99997 },
        {      40503,      51547,       7948,      99998 },
        {      39923,      49367,      10707,      99997 },
        {      39343,      47187,      13467,      99997 },
        {      38763,      45007,      16227,      99997 },
        {      38184,      42827,      18987,      99998 },
        {      37604,      40647,      21746,      99997 },
        {      37024,      38467,      24506,      99997 },
        {      36444,      36287,      27266,      99997 },
        {      35865,      34108,      30026,      99999 },
        {      35690,      34387,      29921,      99998 },
        {      35515,      34666,      29817,      99998 },
        {      35340,      34945,      29713,      99998 },
        {      35165,      35224,      29608,      99997 },
        {      34991,      35503,      29504,      99998 },
        {      34816,      35782,      29400,      99998 },
        {      34641,      36061,      29295,      99997 },
        {      34466,      36340,      29191,      99997 },
        {      34291,      36619,      29087,      99997 },
        {      34117,      36899,      28983,      99999 },
        {      34309,      36634,      29054,      99997 },
        {      34501,      36370,      29126,      99997 },
        {      34694,      36106,      29198,      99998 },
        {      34886,      35841,      29269,      99996 },
        {      35079,      35577,      29341,      99997 },
        {      35271,      35313,      29413,      99997 },
        {      35464,      35049,      29485,      99998 },
        {      31666,      31801,      26530,      89997 },
        {      27869,      28553,      23575,      79997 },
        {      24071,      25305,      20621,      69997 },
        {      20274,      22057,      17666,      59997 },
        {      16477,      18810,      14712,      49999 },
        {      19853,      22638,      17506,      59997 },
        {      23230,      26467,      20301,      69998 },
        {      26606,      30296,      23095,      79997 },
        {      29983,      34125,      25890,      89998 },
        {      33360,      37954,      28685,      99999 },
        {      33487,      37828,      28683,      99998 },
        {      33614,      37702,      28682,      99998 },
        {      33741,      37576,      28680,      99997 },
        {      33869,      37450,      28679,      99998 },
        {      33996,      37324,      28678,      99998 },
        {      34123,      37198,      28676,      99997 },
        {      34250,      37072,      28675,      99997 },
        {      34378,      36947,      28673,      99998 },
        {      34505,      36821,      28672,      99998 },
        {      34632,      36695,      28671,      99998 },
        {      34759,      36569,      28669,      99997 },
        {      34887,      36443,      28668,      99998 },
        {      35014,      36317,      28666,      99997 },
        {      35141,      36191,      28665,      99997 },
        {      35269,      36066,      28664,      99999 }
    };

    return table;
}


#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

static inline unsigned char
varerr_to_byte(int32_t value)
{
    int32_t rounded;

    if (value < 0) {
        return 0;
    }
    if (value > VARERR_MAX_VALUE) {
        return 255;
    }
    rounded = (value + VARERR_ROUND) >> VARERR_SCALE_SHIFT;
    if (rounded < 0) {
        rounded = 0;
    }
    if (rounded > 255) {
        rounded = 255;
    }

    return (unsigned char)rounded;
}

static inline int32_t
varerr_from_byte(unsigned char value)
{
    return ((int32_t)value) << VARERR_SCALE_SHIFT;
}

/*
 * Compute a tone index for variable error diffusion tables.  Color inputs
 * use Rec.601 luma weights to reduce RGB into a single 0-255 luminance
 * value.  Grayscale inputs fall back to the first channel.
 */
static inline int
varerr_tone_from_pixel(const unsigned char *pixel, int depth)
{
    int tone;

    if (depth >= 3) {
        int r;
        int g;
        int b;

        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
        tone = (299 * r + 587 * g + 114 * b + 500) / 1000;
    } else {
        tone = pixel[0];
    }
    if (tone < 0) {
        tone = 0;
    }
    if (tone > 255) {
        tone = 255;
    }

    return tone;
}

#if HAVE_DEBUG
#define quant_trace fprintf
#else
static inline void quant_trace(FILE *f, ...) { (void) f; }
#endif

/*****************************************************************************
 *
 * quantization
 *
 *****************************************************************************/

typedef struct box* boxVector;
struct box {
    unsigned int ind;
    unsigned int colors;
    unsigned int sum;
};

typedef unsigned long sample;
typedef sample * tuple;

struct tupleint {
    /* An ordered pair of a tuple value and an integer, such as you
       would find in a tuple table or tuple hash.
       Note that this is a variable length structure.
    */
    unsigned int value;
    sample tuple[1];
    /* This is actually a variable size array -- its size is the
       depth of the tuple in question.  Some compilers do not let us
       declare a variable length array.
    */
};
typedef struct tupleint ** tupletable;

typedef struct {
    unsigned int size;
    tupletable table;
} tupletable2;

static unsigned int compareplanePlane;
    /* This is a parameter to compareplane().  We use this global variable
       so that compareplane() can be called by qsort(), to compare two
       tuples.  qsort() doesn't pass any arguments except the two tuples.
    */
static int
compareplane(const void * const arg1,
             const void * const arg2)
{
    int lhs, rhs;

    typedef const struct tupleint * const * const sortarg;
    sortarg comparandPP  = (sortarg) arg1;
    sortarg comparatorPP = (sortarg) arg2;
    lhs = (int)(*comparandPP)->tuple[compareplanePlane];
    rhs = (int)(*comparatorPP)->tuple[compareplanePlane];

    return lhs - rhs;
}


static int
sumcompare(const void * const b1, const void * const b2)
{
    return (int)((boxVector)b2)->sum - (int)((boxVector)b1)->sum;
}


static SIXELSTATUS
alloctupletable(
    tupletable          /* out */ *result,
    unsigned int const  /* in */  depth,
    unsigned int const  /* in */  size,
    sixel_allocator_t   /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    enum { message_buffer_size = 256 };
    char message[message_buffer_size];
    int nwrite;
    unsigned int mainTableSize;
    unsigned int tupleIntSize;
    unsigned int allocSize;
    void * pool;
    tupletable tbl;
    unsigned int i;

    if (UINT_MAX / sizeof(struct tupleint) < size) {
        nwrite = sprintf(message,
                         "size %u is too big for arithmetic",
                         size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    mainTableSize = size * sizeof(struct tupleint *);
    tupleIntSize = sizeof(struct tupleint) - sizeof(sample)
        + depth * sizeof(sample);

    /* To save the enormous amount of time it could take to allocate
       each individual tuple, we do a trick here and allocate everything
       as a single malloc block and suballocate internally.
    */
    if ((UINT_MAX - mainTableSize) / tupleIntSize < size) {
        nwrite = sprintf(message,
                         "size %u is too big for arithmetic",
                         size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    allocSize = mainTableSize + size * tupleIntSize;

    pool = sixel_allocator_malloc(allocator, allocSize);
    if (pool == NULL) {
        sprintf(message,
                "unable to allocate %u bytes for a %u-entry "
                "tuple table",
                 allocSize, size);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    tbl = (tupletable) pool;

    for (i = 0; i < size; ++i)
        tbl[i] = (struct tupleint *)
            ((char*)pool + mainTableSize + i * tupleIntSize);

    *result = tbl;

    status = SIXEL_OK;

end:
    return status;
}


/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

static tupletable2
newColorMap(unsigned int const newcolors, unsigned int const depth, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colormap;
    unsigned int i;

    colormap.size = 0;
    status = alloctupletable(&colormap.table, depth, newcolors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (colormap.table) {
        for (i = 0; i < newcolors; ++i) {
            unsigned int plane;
            for (plane = 0; plane < depth; ++plane)
                colormap.table[i]->tuple[plane] = 0;
        }
        colormap.size = newcolors;
    }

end:
    return colormap;
}


static boxVector
newBoxVector(
    unsigned int const  /* in */ colors,
    unsigned int const  /* in */ sum,
    unsigned int const  /* in */ newcolors,
    sixel_allocator_t   /* in */ *allocator)
{
    boxVector bv;

    bv = (boxVector)sixel_allocator_malloc(allocator,
                                           sizeof(struct box) * (size_t)newcolors);
    if (bv == NULL) {
        quant_trace(stderr, "out of memory allocating box vector table\n");
        return NULL;
    }

    /* Set up the initial box. */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;

    return bv;
}


static void
findBoxBoundaries(tupletable2  const colorfreqtable,
                  unsigned int const depth,
                  unsigned int const boxStart,
                  unsigned int const boxSize,
                  sample             minval[],
                  sample             maxval[])
{
/*----------------------------------------------------------------------------
  Go through the box finding the minimum and maximum of each
  component - the boundaries of the box.
-----------------------------------------------------------------------------*/
    unsigned int plane;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        minval[plane] = colorfreqtable.table[boxStart]->tuple[plane];
        maxval[plane] = minval[plane];
    }

    for (i = 1; i < boxSize; ++i) {
        for (plane = 0; plane < depth; ++plane) {
            sample const v = colorfreqtable.table[boxStart + i]->tuple[plane];
            if (v < minval[plane]) minval[plane] = v;
            if (v > maxval[plane]) maxval[plane] = v;
        }
    }
}



static unsigned int
largestByNorm(sample minval[], sample maxval[], unsigned int const depth)
{

    unsigned int largestDimension;
    unsigned int plane;
    sample largestSpreadSoFar;

    largestSpreadSoFar = 0;
    largestDimension = 0;
    for (plane = 0; plane < depth; ++plane) {
        sample const spread = maxval[plane]-minval[plane];
        if (spread > largestSpreadSoFar) {
            largestDimension = plane;
            largestSpreadSoFar = spread;
        }
    }
    return largestDimension;
}



static unsigned int
largestByLuminosity(sample minval[], sample maxval[], unsigned int const depth)
{
/*----------------------------------------------------------------------------
   This subroutine presumes that the tuple type is either
   BLACKANDWHITE, GRAYSCALE, or RGB (which implies pamP->depth is 1 or 3).
   To save time, we don't actually check it.
-----------------------------------------------------------------------------*/
    unsigned int retval;

    double lumin_factor[3] = {0.2989, 0.5866, 0.1145};

    if (depth == 1) {
        retval = 0;
    } else {
        /* An RGB tuple */
        unsigned int largestDimension;
        unsigned int plane;
        double largestSpreadSoFar;

        largestSpreadSoFar = 0.0;
        largestDimension = 0;

        for (plane = 0; plane < 3; ++plane) {
            double const spread =
                lumin_factor[plane] * (maxval[plane]-minval[plane]);
            if (spread > largestSpreadSoFar) {
                largestDimension = plane;
                largestSpreadSoFar = spread;
            }
        }
        retval = largestDimension;
    }
    return retval;
}



static void
centerBox(unsigned int const boxStart,
          unsigned int const boxSize,
          tupletable2  const colorfreqtable,
          unsigned int const depth,
          tuple        const newTuple)
{

    unsigned int plane;
    sample minval, maxval;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        minval = maxval = colorfreqtable.table[boxStart]->tuple[plane];

        for (i = 1; i < boxSize; ++i) {
            sample v = colorfreqtable.table[boxStart + i]->tuple[plane];
            minval = minval < v ? minval: v;
            maxval = maxval > v ? maxval: v;
        }
        newTuple[plane] = (minval + maxval) / 2;
    }
}



static void
averageColors(unsigned int const boxStart,
              unsigned int const boxSize,
              tupletable2  const colorfreqtable,
              unsigned int const depth,
              tuple        const newTuple)
{
    unsigned int plane;
    sample sum;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        sum = 0;

        for (i = 0; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane];
        }

        newTuple[plane] = sum / boxSize;
    }
}



static void
averagePixels(unsigned int const boxStart,
              unsigned int const boxSize,
              tupletable2 const colorfreqtable,
              unsigned int const depth,
              tuple const newTuple)
{

    unsigned int n;
        /* Number of tuples represented by the box */
    unsigned int plane;
    unsigned int i;

    /* Count the tuples in question */
    n = 0;  /* initial value */
    for (i = 0; i < boxSize; ++i) {
        n += (unsigned int)colorfreqtable.table[boxStart + i]->value;
    }

    for (plane = 0; plane < depth; ++plane) {
        sample sum;

        sum = 0;

        for (i = 0; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane]
                * (unsigned int)colorfreqtable.table[boxStart + i]->value;
        }

        newTuple[plane] = sum / n;
    }
}



static tupletable2
colormapFromBv(unsigned int const newcolors,
               boxVector const bv,
               unsigned int const boxes,
               tupletable2 const colorfreqtable,
               unsigned int const depth,
               int const methodForRep,
               sixel_allocator_t *allocator)
{
    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.
    */
    tupletable2 colormap;
    unsigned int bi;

    colormap = newColorMap(newcolors, depth, allocator);
    if (!colormap.size) {
        return colormap;
    }

    for (bi = 0; bi < boxes; ++bi) {
        switch (methodForRep) {
        case SIXEL_REP_CENTER_BOX:
            centerBox(bv[bi].ind, bv[bi].colors,
                      colorfreqtable, depth,
                      colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_COLORS:
            averageColors(bv[bi].ind, bv[bi].colors,
                          colorfreqtable, depth,
                          colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_PIXELS:
            averagePixels(bv[bi].ind, bv[bi].colors,
                          colorfreqtable, depth,
                          colormap.table[bi]->tuple);
            break;
        default:
            quant_trace(stderr, "Internal error: "
                                "invalid value of methodForRep: %d\n",
                        methodForRep);
        }
    }
    return colormap;
}


static SIXELSTATUS
splitBox(boxVector const bv,
         unsigned int *const boxesP,
         unsigned int const bi,
         tupletable2 const colorfreqtable,
         unsigned int const depth,
         int const methodForLargest)
{
/*----------------------------------------------------------------------------
   Split Box 'bi' in the box vector bv (so that bv contains one more box
   than it did as input).  Split it so that each new box represents about
   half of the pixels in the distribution given by 'colorfreqtable' for
   the colors in the original box, but with distinct colors in each of the
   two new boxes.

   Assume the box contains at least two colors.
-----------------------------------------------------------------------------*/
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int const boxStart = bv[bi].ind;
    unsigned int const boxSize  = bv[bi].colors;
    unsigned int const sm       = bv[bi].sum;

    enum { max_depth= 16 };
    sample minval[max_depth];
    sample maxval[max_depth];

    /* assert(max_depth >= depth); */

    unsigned int largestDimension;
        /* number of the plane with the largest spread */
    unsigned int medianIndex;
    unsigned int lowersum;
        /* Number of pixels whose value is "less than" the median */

    findBoxBoundaries(colorfreqtable, depth, boxStart, boxSize,
                      minval, maxval);

    /* Find the largest dimension, and sort by that component.  I have
       included two methods for determining the "largest" dimension;
       first by simply comparing the range in RGB space, and second by
       transforming into luminosities before the comparison.
    */
    switch (methodForLargest) {
    case SIXEL_LARGE_NORM:
        largestDimension = largestByNorm(minval, maxval, depth);
        break;
    case SIXEL_LARGE_LUM:
        largestDimension = largestByLuminosity(minval, maxval, depth);
        break;
    default:
        sixel_helper_set_additional_message(
            "Internal error: invalid value of methodForLargest.");
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    /* TODO: I think this sort should go after creating a box,
       not before splitting.  Because you need the sort to use
       the SIXEL_REP_CENTER_BOX method of choosing a color to
       represent the final boxes
    */

    /* Set the gross global variable 'compareplanePlane' as a
       parameter to compareplane(), which is called by qsort().
    */
    compareplanePlane = largestDimension;
    qsort((char*) &colorfreqtable.table[boxStart], boxSize,
          sizeof(colorfreqtable.table[boxStart]),
          compareplane);

    {
        /* Now find the median based on the counts, so that about half
           the pixels (not colors, pixels) are in each subdivision.  */

        unsigned int i;

        lowersum = colorfreqtable.table[boxStart]->value; /* initial value */
        for (i = 1; i < boxSize - 1 && lowersum < sm / 2; ++i) {
            lowersum += colorfreqtable.table[boxStart + i]->value;
        }
        medianIndex = i;
    }
    /* Split the box, and sort to bring the biggest boxes to the top.  */

    bv[bi].colors = medianIndex;
    bv[bi].sum = lowersum;
    bv[*boxesP].ind = boxStart + medianIndex;
    bv[*boxesP].colors = boxSize - medianIndex;
    bv[*boxesP].sum = sm - lowersum;
    ++(*boxesP);
    qsort((char*) bv, *boxesP, sizeof(struct box), sumcompare);

    status = SIXEL_OK;

end:
    return status;
}



static SIXELSTATUS
mediancut(tupletable2 const colorfreqtable,
          unsigned int const depth,
          unsigned int const newcolors,
          int const methodForLargest,
          int const methodForRep,
          tupletable2 *const colormapP,
          sixel_allocator_t *allocator)
{
/*----------------------------------------------------------------------------
   Compute a set of only 'newcolors' colors that best represent an
   image whose pixels are summarized by the histogram
   'colorfreqtable'.  Each tuple in that table has depth 'depth'.
   colorfreqtable.table[i] tells the number of pixels in the subject image
   have a particular color.

   As a side effect, sort 'colorfreqtable'.
-----------------------------------------------------------------------------*/
    boxVector bv;
    unsigned int bi;
    unsigned int boxes;
    int multicolorBoxesExist;
    unsigned int i;
    unsigned int sum;
    SIXELSTATUS status = SIXEL_FALSE;

    sum = 0;

    for (i = 0; i < colorfreqtable.size; ++i) {
        sum += colorfreqtable.table[i]->value;
    }

    /* There is at least one box that contains at least 2 colors; ergo,
       there is more splitting we can do.  */
    bv = newBoxVector(colorfreqtable.size, sum, newcolors, allocator);
    if (bv == NULL) {
        goto end;
    }
    boxes = 1;
    multicolorBoxesExist = (colorfreqtable.size > 1);

    /* Main loop: split boxes until we have enough. */
    while (boxes < newcolors && multicolorBoxesExist) {
        /* Find the first splittable box. */
        for (bi = 0; bi < boxes && bv[bi].colors < 2; ++bi)
            ;
        if (bi >= boxes) {
            multicolorBoxesExist = 0;
        } else {
            status = splitBox(bv, &boxes, bi,
                              colorfreqtable, depth,
                              methodForLargest);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }
    *colormapP = colormapFromBv(newcolors, bv, boxes,
                                colorfreqtable, depth,
                                methodForRep, allocator);

    sixel_allocator_free(allocator, bv);

    status = SIXEL_OK;

end:
    return status;
}


static unsigned int
computeHash(unsigned char const *data, unsigned int const depth)
{
    unsigned int hash = 0;
    unsigned int n;

    for (n = 0; n < depth; n++) {
        hash |= (unsigned int)(data[depth - 1 - n] >> 3) << n * 5;
    }

    return hash;
}


static SIXELSTATUS
computeHistogram(unsigned char const    /* in */  *data,
                 unsigned int           /* in */  length,
                 unsigned long const    /* in */  depth,
                 tupletable2 * const    /* out */ colorfreqtableP,
                 int const              /* in */  qualityMode,
                 sixel_allocator_t      /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    typedef unsigned short unit_t;
    unsigned int i, n;
    unit_t *histogram = NULL;
    unit_t *refmap = NULL;
    unit_t *ref;
    unit_t *it;
    unsigned int bucket_index;
    unsigned int step;
    unsigned int max_sample;

    switch (qualityMode) {
    case SIXEL_QUALITY_LOW:
        max_sample = 18383;
        break;
    case SIXEL_QUALITY_HIGH:
        max_sample = 1118383;
        break;
    case SIXEL_QUALITY_FULL:
    default:
        max_sample = 4003079;
        break;
    }

    step = length / depth / max_sample * depth;
    if (step <= 0) {
        step = depth;
    }

    quant_trace(stderr, "making histogram...\n");

    histogram = (unit_t *)sixel_allocator_calloc(allocator,
                                                 (size_t)(1 << depth * 5),
                                                 sizeof(unit_t));
    if (histogram == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for histogram.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    it = ref = refmap
        = (unsigned short *)sixel_allocator_malloc(allocator,
                                                   (size_t)(1 << depth * 5) * sizeof(unit_t));
    if (!it) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for lookup table.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (i = 0; i < length; i += step) {
        bucket_index = computeHash(data + i, 3);
        if (histogram[bucket_index] == 0) {
            *ref++ = bucket_index;
        }
        if (histogram[bucket_index] < (unsigned int)(1 << sizeof(unsigned short) * 8) - 1) {
            histogram[bucket_index]++;
        }
    }

    colorfreqtableP->size = (unsigned int)(ref - refmap);

    status = alloctupletable(&colorfreqtableP->table, depth, (unsigned int)(ref - refmap), allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (i = 0; i < colorfreqtableP->size; ++i) {
        if (histogram[refmap[i]] > 0) {
            colorfreqtableP->table[i]->value = histogram[refmap[i]];
            for (n = 0; n < depth; n++) {
                colorfreqtableP->table[i]->tuple[depth - 1 - n]
                    = (sample)((*it >> n * 5 & 0x1f) << 3);
            }
        }
        it++;
    }

    quant_trace(stderr, "%u colors found\n", colorfreqtableP->size);

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, refmap);
    sixel_allocator_free(allocator, histogram);

    return status;
}


static int
computeColorMapFromInput(unsigned char const *data,
                         unsigned int const length,
                         unsigned int const depth,
                         unsigned int const reqColors,
                         int const methodForLargest,
                         int const methodForRep,
                         int const qualityMode,
                         tupletable2 * const colormapP,
                         unsigned int *origcolors,
                         sixel_allocator_t *allocator)
{
/*----------------------------------------------------------------------------
   Produce a colormap containing the best colors to represent the
   image stream in file 'ifP'.  Figure it out using the median cut
   technique.

   The colormap will have 'reqcolors' or fewer colors in it, unless
   'allcolors' is true, in which case it will have all the colors that
   are in the input.

   The colormap has the same maxval as the input.

   Put the colormap in newly allocated storage as a tupletable2
   and return its address as *colormapP.  Return the number of colors in
   it as *colorsP and its maxval as *colormapMaxvalP.

   Return the characteristics of the input file as
   *formatP and *freqPamP.  (This information is not really
   relevant to our colormap mission; just a fringe benefit).
-----------------------------------------------------------------------------*/
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colorfreqtable = {0, NULL};
    unsigned int i;
    unsigned int n;

    status = computeHistogram(data, length, depth,
                              &colorfreqtable, qualityMode, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= reqColors) {
        quant_trace(stderr,
                    "Image already has few enough colors (<=%d).  "
                    "Keeping same colors.\n", reqColors);
        /* *colormapP = colorfreqtable; */
        colormapP->size = colorfreqtable.size;
        status = alloctupletable(&colormapP->table, depth, colorfreqtable.size, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (i = 0; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0; n < depth; ++n) {
                colormapP->table[i]->tuple[n] = colorfreqtable.table[i]->tuple[n];
            }
        }
    } else {
        quant_trace(stderr, "choosing %d colors...\n", reqColors);
        status = mediancut(colorfreqtable, depth, reqColors,
                           methodForLargest, methodForRep, colormapP, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        quant_trace(stderr, "%d colors are choosed.\n", colorfreqtable.size);
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, colorfreqtable.table);
    return status;
}


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


typedef void (*diffuse_varerr_func)(void *target, int depth, size_t offset,
                                    int32_t error, int weight, int denom);
typedef void (*diffuse_varerr_mode)(int32_t *workspace,
                                    int width,
                                    int height,
                                    int x,
                                    int y,
                                    int depth,
                                    int32_t error,
                                    int index,
                                    int direction);

static void diffuse_varerr_accum_pixel(int32_t *workspace, int depth,
                                       size_t offset, int32_t error,
                                       int weight, int denom);

static void
diffuse_varerr_apply_accum(void *target, int depth, size_t offset,
                           int32_t error, int weight, int denom)
{
    int32_t *workspace;

    workspace = (int32_t *)target;
    diffuse_varerr_accum_pixel(workspace, depth, offset,
                               error, weight, denom);
}

static void
diffuse_varerr_common(void *target, int width, int height,
                      int x, int y, int depth, int32_t error,
                      int index, const int (*table)[4], int direction,
                      diffuse_varerr_func apply)
{
    const int *entry;
    int denom;

    /*
     * Table lookups expect a tone index derived from the pixel luminance.
     * Each table entry provides weights for the neighbours shown below.  The
     * fourth value is the shared denominator, equal to the sum of the first
     * three.  Forward scan applies the offsets on the left, while reverse scan
     * uses the layout on the right.
     *
     *   forward scan (direction >= 0)              reverse scan (direction < 0)
     *
     *              [0] (x+1, y)                                 (x-1, y) [0]
     *                 ^                                              ^
     *                 | processing direction                         |
     *                 |                                              |
     *   (x, y)  o---->o                                        o<----o  (x, y)
     *                 \                                              /
     *                  \                                            /
     *      [1] (x-1, y+1)   [2] (x, y+1)         [1] (x+1, y+1)   [2] (x, y+1)
     */

    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    entry = table[index];
    denom = entry[3];
    if (denom == 0 || error == 0) {
        return;
    }

    if (direction >= 0) {
        size_t offset;

        if (x + 1 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 1);
            apply(target, depth, offset, error, entry[0], denom);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            apply(target, depth, offset, error, entry[1], denom);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            apply(target, depth, offset, error, entry[2], denom);
        }
    } else {
        size_t offset;

        if (x - 1 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 1);
            apply(target, depth, offset, error, entry[0], denom);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            apply(target, depth, offset, error, entry[1], denom);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            apply(target, depth, offset, error, entry[2], denom);
        }
    }
}

static int
zhoufang_index_from_byte(unsigned char value)
{
    double px;
    double remapped;
    int scale_index;
    double scale;
    double jitter;
    int index;

    px = (double)value / 255.0;
    remapped = px;
    if (remapped >= 0.5) {
        remapped = 1.0 - remapped;
    }
    if (remapped < 0.0) {
        remapped = 0.0;
    }
    if (remapped > 0.5) {
        remapped = 0.5;
    }

    scale_index = (int)(remapped * 128.0);
    if (scale_index < 0) {
        scale_index = 0;
    }
    if (scale_index > 127) {
        scale_index = 127;
    }

    scale = zhoufang_threshold_gain[scale_index] / 100.0;
    jitter = ((double)(rand() & 127) / 128.0) * scale;
    remapped += (0.5 - remapped) * jitter;
    if (remapped < 0.0) {
        remapped = 0.0;
    }
    if (remapped > 0.5) {
        remapped = 0.5;
    }

    index = (int)(remapped * 255.0 + 0.5);
    if (index > 127) {
        index = 127;
    }
    if (index < 0) {
        index = 0;
    }

    return index;
}


static void
diffuse_varerr_accum_pixel(int32_t *workspace, int depth, size_t offset,
                           int32_t error, int weight, int denom)
{
    int64_t value;
    int64_t delta;

    value = workspace[offset * depth];
    delta = (int64_t)error * weight;
    if (delta >= 0) {
        delta = (delta + denom / 2) / denom;
    } else {
        delta = (delta - denom / 2) / denom;
    }
    value += delta;
    if (value < 0) {
        value = 0;
    } else if (value > VARERR_MAX_VALUE) {
        value = VARERR_MAX_VALUE;
    }
    workspace[offset * depth] = (int32_t)value;
}


static void
diffuse_varerr_accum(int32_t *workspace, int width, int height,
                     int x, int y, int depth, int32_t error,
                     int index, const int (*table)[4], int direction)
{
    diffuse_varerr_common(workspace, width, height, x, y, depth,
                          error, index, table, direction,
                          diffuse_varerr_apply_accum);
}


static void
diffuse_ostromoukhov(int32_t *workspace, int width, int height,
                     int x, int y, int depth, int32_t error,
                     int index, int direction)
{
    diffuse_varerr_accum(workspace, width, height, x, y, depth,
                         error, index, ostromoukhov_table(), direction);
}


static void
diffuse_zhoufang(int32_t *workspace, int width, int height,
                 int x, int y, int depth, int32_t error,
                 int index, int direction)
{
    diffuse_varerr_accum(workspace, width, height, x, y, depth,
                         error, index, zhoufang_table(), direction);
}


static SIXELSTATUS
apply_palette_positional(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    float (*f_mask)(int x, int y, int c),
    int serpentine,
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
    int y;

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

            if (serpentine && (y & 1)) {
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                int d;
                int color_index;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + f_mask(x, y, d) * 32;
                    copy[d] = val < 0 ? 0 : val > 255 ? 255 : val;
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

            if (serpentine && (y & 1)) {
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                int d;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + f_mask(x, y, d) * 32;
                    copy[d] = val < 0 ? 0 : val > 255 ? 255 : val;
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
apply_palette_variable(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int serpentine,
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
    int32_t *varerr_work,
    int32_t varerr_value[],
    diffuse_varerr_mode varerr_diffuse,
    int methodForDiffuse)
{
    int y;

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

            if (serpentine && (y & 1)) {
                direction = -1;
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                direction = 1;
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                size_t base;
                int n;
                int color_index;
                int tone_index;
                int table_index;

                pos = y * width + x;
                base = (size_t)pos * (size_t)depth;
                for (n = 0; n < depth; ++n) {
                    int32_t value;
                    unsigned char byte;

                    value = varerr_work[base + n];
                    if (value < 0) {
                        value = 0;
                    } else if (value > VARERR_MAX_VALUE) {
                        value = VARERR_MAX_VALUE;
                    }
                    varerr_work[base + n] = value;
                    byte = varerr_to_byte(value);
                    data[base + n] = byte;
                    varerr_value[n] = value;
                }
                tone_index = varerr_tone_from_pixel(data + base, depth);
                if (methodForDiffuse == SIXEL_DIFFUSE_ZHOUFANG) {
                    table_index = zhoufang_index_from_byte(
                        (unsigned char)tone_index);
                } else {
                    table_index = tone_index;
                }
                color_index = f_lookup(data + base, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                if (migration_map[color_index] == 0) {
                    result[pos] = *ncolors;
                    for (n = 0; n < depth; ++n) {
                        new_palette[*ncolors * depth + n]
                            = palette[color_index * depth + n];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    result[pos] = migration_map[color_index] - 1;
                }
                for (n = 0; n < depth; ++n) {
                    size_t idx;
                    int32_t target;
                    int32_t error_scaled;

                    idx = base + n;
                    target = varerr_from_byte(
                                palette[color_index * depth + n]);
                    error_scaled = varerr_value[n] - target;
                    varerr_work[idx] = target;
                    varerr_diffuse(varerr_work + n, width, height,
                                   x, y, depth, error_scaled,
                                   table_index, direction);
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

            if (serpentine && (y & 1)) {
                direction = -1;
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                direction = 1;
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                size_t base;
                int n;
                int color_index;
                int tone_index;
                int table_index;

                pos = y * width + x;
                base = (size_t)pos * (size_t)depth;
                for (n = 0; n < depth; ++n) {
                    int32_t value;
                    unsigned char byte;

                    value = varerr_work[base + n];
                    if (value < 0) {
                        value = 0;
                    } else if (value > VARERR_MAX_VALUE) {
                        value = VARERR_MAX_VALUE;
                    }
                    varerr_work[base + n] = value;
                    byte = varerr_to_byte(value);
                    data[base + n] = byte;
                    varerr_value[n] = value;
                }
                tone_index = varerr_tone_from_pixel(data + base, depth);
                if (methodForDiffuse == SIXEL_DIFFUSE_ZHOUFANG) {
                    table_index = zhoufang_index_from_byte(
                        (unsigned char)tone_index);
                } else {
                    table_index = tone_index;
                }
                color_index = f_lookup(data + base, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                result[pos] = color_index;
                for (n = 0; n < depth; ++n) {
                    size_t idx;
                    int32_t target;
                    int32_t error_scaled;

                    idx = base + n;
                    target = varerr_from_byte(
                                palette[color_index * depth + n]);
                    error_scaled = varerr_value[n] - target;
                    varerr_work[idx] = target;
                    varerr_diffuse(varerr_work + n, width, height,
                                   x, y, depth, error_scaled,
                                   table_index, direction);
                }
            }
        }
        *ncolors = reqcolor;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
apply_palette_fixed(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int serpentine,
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
    void (*f_diffuse)(unsigned char *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      int offset,
                      int direction))
{
    int y;

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

            if (serpentine && (y & 1)) {
                direction = -1;
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                direction = 1;
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                size_t base;
                int n;
                int color_index;

                pos = y * width + x;
                base = (size_t)pos * (size_t)depth;
                color_index = f_lookup(data + base, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                if (migration_map[color_index] == 0) {
                    result[pos] = *ncolors;
                    for (n = 0; n < depth; ++n) {
                        new_palette[*ncolors * depth + n]
                            = palette[color_index * depth + n];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    result[pos] = migration_map[color_index] - 1;
                }
                for (n = 0; n < depth; ++n) {
                    int offset;

                    offset = (int)data[base + n]
                           - (int)palette[color_index * depth + n];
                    f_diffuse(data + n, width, height, x, y,
                              depth, offset, direction);
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

            if (serpentine && (y & 1)) {
                direction = -1;
                start = width - 1;
                end = -1;
                step = -1;
            } else {
                direction = 1;
                start = 0;
                end = width;
                step = 1;
            }
            for (x = start; x != end; x += step) {
                int pos;
                size_t base;
                int n;
                int color_index;

                pos = y * width + x;
                base = (size_t)pos * (size_t)depth;
                color_index = f_lookup(data + base, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                result[pos] = color_index;
                for (n = 0; n < depth; ++n) {
                    int offset;

                    offset = (int)data[base + n]
                           - (int)palette[color_index * depth + n];
                    f_diffuse(data + n, width, height, x, y,
                              depth, offset, direction);
                }
            }
        }
        *ncolors = reqcolor;
    }

    return SIXEL_OK;
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
diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int error, int direction)
{
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
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
diffuse_atkinson(unsigned char *data, int width, int height,
                 int x, int y, int depth, int error, int direction)
{
    int pos;
    int sign;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
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
diffuse_jajuni(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
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

    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
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
diffuse_stucki(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
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

    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
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
diffuse_burkes(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
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

    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
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
diffuse_lso1(unsigned char *data, int width, int height,
             int x, int y, int depth, int error, int direction)
{
    int pos;
    int sign;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    /* lso1 (libsixel original) method:
     *
     * libsixel-specific error diffusion (dithering) to improve sixel
     * compression; by steering error propagation so out-of-palette
     * intermediate colors render as horizontal bands rather than grainy
     * noise, we increase RLE more effective.
     *
     *          curr
     *   1/8    4/8    1/8
     *          2/8
     */
    if (y < height - 1) {
        int row;

        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_fast(data,
                               row + (-sign),
                               depth, error, 1, 8);
        }
        error_diffuse_fast(data, row, depth, error, 4, 8);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_fast(data,
                               row + sign,
                               depth, error, 1, 8);
        }
    }
    if (y < height - 2) {
        error_diffuse_fast(data, pos + width * 2, depth, error, 2, 8);
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


/* lookup closest color from palette with "fast" strategy */
static int
lookup_fast(unsigned char const * const pixel,
            int const depth,
            unsigned char const * const palette,
            int const reqcolor,
            unsigned short * const cachetable,
            int const complexion)
{
    int result;
    unsigned int hash;
    int diff;
    int cache;
    int i;
    int distant;

    /* don't use depth in 'fast' strategy because it's always 3 */
    (void) depth;

    result = (-1);
    diff = INT_MAX;
    hash = computeHash(pixel, 3);

    cache = cachetable[hash];
    if (cache) {  /* fast lookup */
        return cache - 1;
    }
    /* collision */
    for (i = 0; i < reqcolor; i++) {
        distant = 0;
#if 0
        for (n = 0; n < 3; ++n) {
            r = pixel[n] - palette[i * 3 + n];
            distant += r * r;
        }
#elif 1  /* complexion correction */
        distant = (pixel[0] - palette[i * 3 + 0]) * (pixel[0] - palette[i * 3 + 0]) * complexion
                + (pixel[1] - palette[i * 3 + 1]) * (pixel[1] - palette[i * 3 + 1])
                + (pixel[2] - palette[i * 3 + 2]) * (pixel[2] - palette[i * 3 + 2])
                ;
#endif
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }
    cachetable[hash] = result + 1;

    return result;
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


/* choose colors using median-cut method */
SIXELSTATUS
sixel_quant_make_palette(
    unsigned char          /* out */ **result,
    unsigned char const    /* in */  *data,
    unsigned int           /* in */  length,
    int                    /* in */  pixelformat,
    unsigned int           /* in */  reqcolors,
    unsigned int           /* in */  *ncolors,
    unsigned int           /* in */  *origcolors,
    int                    /* in */  methodForLargest,
    int                    /* in */  methodForRep,
    int                    /* in */  qualityMode,
    sixel_allocator_t      /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int i;
    unsigned int n;
    int ret;
    tupletable2 colormap;
    unsigned int depth;
    int result_depth;

    result_depth = sixel_helper_compute_depth(pixelformat);
    if (result_depth <= 0) {
        *result = NULL;
        goto end;
    }

    depth = (unsigned int)result_depth;

    ret = computeColorMapFromInput(data, length, depth,
                                   reqcolors, methodForLargest,
                                   methodForRep, qualityMode,
                                   &colormap, origcolors, allocator);
    if (ret != 0) {
        *result = NULL;
        goto end;
    }
    *ncolors = colormap.size;
    quant_trace(stderr, "tupletable size: %d\n", *ncolors);
    *result = (unsigned char *)sixel_allocator_malloc(allocator, *ncolors * depth);
    for (i = 0; i < *ncolors; i++) {
        for (n = 0; n < depth; ++n) {
            (*result)[i * depth + n] = colormap.table[i]->tuple[n];
        }
    }

    sixel_allocator_free(allocator, colormap.table);

    status = SIXEL_OK;

end:
    return status;
}


/* apply color palette into specified pixel buffers */
SIXELSTATUS
sixel_quant_apply_palette(
    sixel_index_t     /* out */ *result,
    unsigned char     /* in */  *data,
    int               /* in */  width,
    int               /* in */  height,
    int               /* in */  depth,
    unsigned char     /* in */  *palette,
    int               /* in */  reqcolor,
    int               /* in */  methodForDiffuse,
    int               /* in */  methodForScan,
    int               /* in */  foptimize,
    int               /* in */  foptimize_palette,
    int               /* in */  complexion,
    unsigned short    /* in */  *cachetable,
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
    int sum1, sum2;
    int n;
    unsigned short *indextable;
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    unsigned short migration_map[SIXEL_PALETTE_MAX];
    float (*f_mask) (int x, int y, int c) = NULL;
    void (*f_diffuse)(unsigned char *data, int width, int height,
                      int x, int y, int depth, int offset,
                      int direction);
    int (*f_lookup)(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion);
    int serpentine;
    int use_varerr = 0;
    int32_t *varerr_work = NULL;
    size_t varerr_count = 0;
    int32_t varerr_value[max_depth];
    diffuse_varerr_mode varerr_diffuse = NULL;

    /* check bad reqcolor */
    if (reqcolor < 1) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_quant_apply_palette: "
            "a bad argument is detected, reqcolor < 0.");
        goto end;
    }

    /* NOTE: diffuse_jajuni, diffuse_stucki, and diffuse_burkes reference at
     * minimum the position pos + width * 1 - 2, so width must be at least 2
     * to avoid underflow.
     * On the other hand, diffuse_fs and diffuse_atkinson
     * reference pos + width * 1 - 1, but since these functions are only called
     * when width >= 1, they do not cause underflow.
     */
    if (depth != 3) {
        f_diffuse = diffuse_none;
    } else {
        switch (methodForDiffuse) {
        case SIXEL_DIFFUSE_NONE:
            f_diffuse = diffuse_none;
            break;
        case SIXEL_DIFFUSE_ATKINSON:
            f_diffuse = diffuse_atkinson;
            break;
        case SIXEL_DIFFUSE_FS:
            f_diffuse = diffuse_fs;
            break;
        case SIXEL_DIFFUSE_JAJUNI:
            f_diffuse = diffuse_jajuni;
            break;
        case SIXEL_DIFFUSE_STUCKI:
            f_diffuse = diffuse_stucki;
            break;
        case SIXEL_DIFFUSE_BURKES:
            f_diffuse = diffuse_burkes;
            break;
        case SIXEL_DIFFUSE_OSTROMOUKHOV:
        case SIXEL_DIFFUSE_ZHOUFANG:
            /* Variable diffusion updates run through diffuse_varerr_accum. */
            f_diffuse = diffuse_none;
            break;
        case SIXEL_DIFFUSE_LSO1:
            f_diffuse = diffuse_lso1;
            break;
        case SIXEL_DIFFUSE_A_DITHER:
            f_diffuse = diffuse_none;
            f_mask = mask_a;
            break;
        case SIXEL_DIFFUSE_X_DITHER:
            f_diffuse = diffuse_none;
            f_mask = mask_x;
            break;
        default:
            quant_trace(stderr, "Internal error: invalid value of"
                                " methodForDiffuse: %d\n",
                        methodForDiffuse);
            f_diffuse = diffuse_none;
            break;
        }
    }

    if (methodForDiffuse == SIXEL_DIFFUSE_ZHOUFANG) {
        srand((unsigned int)time(NULL));
    }

    use_varerr = 0;
    if (depth == 3 &&
            (methodForDiffuse == SIXEL_DIFFUSE_OSTROMOUKHOV ||
             methodForDiffuse == SIXEL_DIFFUSE_ZHOUFANG)) {
        size_t idx;

        varerr_count = (size_t)width * (size_t)height * (size_t)depth;
        varerr_work = (int32_t *)sixel_allocator_malloc(allocator,
                            varerr_count * sizeof(int32_t));
        if (varerr_work == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "unable to allocate variable error workspace");
            goto end;
        }
        for (idx = 0; idx < varerr_count; ++idx) {
            varerr_work[idx] = varerr_from_byte(data[idx]);
        }
        use_varerr = 1;
        if (methodForDiffuse == SIXEL_DIFFUSE_OSTROMOUKHOV) {
            varerr_diffuse = diffuse_ostromoukhov;
        } else {
            varerr_diffuse = diffuse_zhoufang;
        }
    }

    f_lookup = NULL;
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
    if (f_lookup == NULL) {
        if (foptimize && depth == 3) {
            f_lookup = lookup_fast;
        } else {
            f_lookup = lookup_normal;
        }
    }

    indextable = cachetable;
    if (cachetable == NULL && f_lookup == lookup_fast) {
        indextable = (unsigned short *)sixel_allocator_calloc(allocator,
                                                              (size_t)(1 << depth * 5),
                                                              sizeof(unsigned short));
        if (!indextable) {
            quant_trace(stderr, "Unable to allocate memory for indextable.\n");
            goto end;
        }
    }

    serpentine = (methodForScan == SIXEL_SCAN_SERPENTINE);

    if (f_mask) {
        status = apply_palette_positional(result, data, width, height,
                                          depth, palette, reqcolor,
                                          f_mask, serpentine,
                                          foptimize_palette, f_lookup,
                                          indextable, complexion, copy,
                                          new_palette, migration_map,
                                          ncolors);
    } else if (use_varerr) {
        status = apply_palette_variable(result, data, width, height,
                                        depth, palette, reqcolor,
                                        serpentine, foptimize_palette,
                                        f_lookup, indextable, complexion,
                                        new_palette, migration_map,
                                        ncolors, varerr_work,
                                        varerr_value,
                                        varerr_diffuse, methodForDiffuse);
    } else {
        status = apply_palette_fixed(result, data, width, height,
                                     depth, palette, reqcolor,
                                     serpentine, foptimize_palette,
                                     f_lookup, indextable, complexion,
                                     new_palette, migration_map,
                                     ncolors, f_diffuse);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (cachetable == NULL) {
        sixel_allocator_free(allocator, indextable);
    }

    status = SIXEL_OK;

end:
    if (use_varerr) {
        sixel_allocator_free(allocator, varerr_work);
    }

    return status;
}


void
sixel_quant_free_palette(
    unsigned char       /* in */ *data,
    sixel_allocator_t   /* in */ *allocator)
{
    sixel_allocator_free(allocator, data);
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    sample minval[1] = { 1 };
    sample maxval[1] = { 2 };
    unsigned int retval;

    retval = largestByLuminosity(minval, maxval, 1);
    if (retval != 0) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}


int
sixel_quant_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
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
