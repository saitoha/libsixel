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
 *
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include "quant.h"
#include "sixel.h"

#if 0
#define quant_trace fprintf
#else
static inline void quant_trace(FILE *f, ...) {}
#endif

/*****************************************************************************
 *
 * quantization
 *
 *****************************************************************************/

typedef struct box* boxVector;
struct box {
    int ind;
    int colors;
    int sum;
};

typedef unsigned long sample;
typedef sample * tuple;

struct tupleint {
    /* An ordered pair of a tuple value and an integer, such as you
       would find in a tuple table or tuple hash.
       Note that this is a variable length structure.
    */
    int value;
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

    const struct tupleint * const * const comparandPP  = arg1;
    const struct tupleint * const * const comparatorPP = arg2;

    return (*comparandPP)->tuple[compareplanePlane] -
        (*comparatorPP)->tuple[compareplanePlane];
}


static int
sumcompare(const void * const b1, const void * const b2)
{
    return(((boxVector)b2)->sum - ((boxVector)b1)->sum);
}


static tupletable const
alloctupletable(unsigned int const depth, unsigned int const size)
{
    if (UINT_MAX / sizeof(struct tupleint) < size) {
        quant_trace(stderr, "size %u is too big for arithmetic\n", size);
        return NULL;
    }

    unsigned int const mainTableSize = size * sizeof(struct tupleint *);
    unsigned int const tupleIntSize =
        sizeof(struct tupleint) - sizeof(sample)
        + depth * sizeof(sample);

    /* To save the enormous amount of time it could take to allocate
       each individual tuple, we do a trick here and allocate everything
       as a single malloc block and suballocate internally.
    */
    if ((UINT_MAX - mainTableSize) / tupleIntSize < size) {
        quant_trace(stderr, "size %u is too big for arithmetic\n", size);
        return NULL;
    }

    unsigned int const allocSize = mainTableSize + size * tupleIntSize;
    void * pool;

    pool = malloc(allocSize);

    if (!pool) {
        quant_trace(stderr, "Unable to allocate %u bytes for a %u-entry "
                    "tuple table\n", allocSize, size);
        return NULL;
    }
    tupletable const tbl = (tupletable) pool;

    unsigned int i;

    for (i = 0; i < size; ++i)
        tbl[i] = (struct tupleint *)
            ((char*)pool + mainTableSize + i * tupleIntSize);

    return tbl;
}



/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

static tupletable2
newColorMap(unsigned int const newcolors, unsigned int const depth)
{
    tupletable2 colormap;
    unsigned int i;
    tupletable table;

    colormap.size = 0;
    colormap.table = alloctupletable(depth, newcolors);
    if (colormap.table) {
        for (i = 0; i < newcolors; ++i) {
            unsigned int plane;
            for (plane = 0; plane < depth; ++plane)
                colormap.table[i]->tuple[plane] = 0;
        }
        colormap.size = newcolors;
    }

    return colormap;
}


static boxVector
newBoxVector(int const colors, int const sum, int const newcolors)
{
    boxVector bv;

    bv = (boxVector)malloc(sizeof(struct box) * newcolors);
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
        unsigned int plane;
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
centerBox(int          const boxStart,
          int          const boxSize,
          tupletable2  const colorfreqtable,
          unsigned int const depth,
          tuple        const newTuple)
{

    unsigned int plane;

    for (plane = 0; plane < depth; ++plane) {
        int minval, maxval;
        unsigned int i;

        minval = maxval = colorfreqtable.table[boxStart]->tuple[plane];

        for (i = 1; i < boxSize; ++i) {
            int const v = colorfreqtable.table[boxStart + i]->tuple[plane];
            minval = minval < v ? minval: v;
            maxval = maxval > v ? maxval: v;
        }
        newTuple[plane] = (minval + maxval) / 2;
    }
}



static void
averageColors(int          const boxStart,
              int          const boxSize,
              tupletable2  const colorfreqtable,
              unsigned int const depth,
              tuple        const newTuple)
{
    unsigned int plane;

    for (plane = 0; plane < depth; ++plane) {
        sample sum;
        int i;

        sum = 0;

        for (i = 0; i < boxSize; ++i)
            sum += colorfreqtable.table[boxStart+i]->tuple[plane];

        newTuple[plane] = sum / boxSize;
    }
}



static void
averagePixels(int const boxStart,
              int const boxSize,
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
    for (i = 0; i < boxSize; ++i)
        n += colorfreqtable.table[boxStart + i]->value;

    for (plane = 0; plane < depth; ++plane) {
        sample sum;
        int i;

        sum = 0;

        for (i = 0; i < boxSize; ++i)
            sum += colorfreqtable.table[boxStart+i]->tuple[plane]
                * colorfreqtable.table[boxStart+i]->value;

        newTuple[plane] = sum / n;
    }
}



static tupletable2
colormapFromBv(unsigned int const newcolors,
               boxVector const bv,
               unsigned int const boxes,
               tupletable2 const colorfreqtable,
               unsigned int const depth,
               int const methodForRep)
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

    colormap = newColorMap(newcolors, depth);
    if (!colormap.size) {
        return colormap;
    }

    for (bi = 0; bi < boxes; ++bi) {
        switch (methodForRep) {
        case REP_CENTER_BOX:
            centerBox(bv[bi].ind, bv[bi].colors,
                      colorfreqtable, depth,
                      colormap.table[bi]->tuple);
            break;
        case REP_AVERAGE_COLORS:
            averageColors(bv[bi].ind, bv[bi].colors,
                          colorfreqtable, depth,
                          colormap.table[bi]->tuple);
            break;
        case REP_AVERAGE_PIXELS:
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


static int
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
    unsigned int const boxStart = bv[bi].ind;
    unsigned int const boxSize  = bv[bi].colors;
    unsigned int const sm       = bv[bi].sum;

    unsigned int const max_depth = 16;
    sample minval[max_depth];
    sample maxval[max_depth];

    /* assert(max_depth >= depth); */

    unsigned int largestDimension;
        /* number of the plane with the largest spread */
    unsigned int medianIndex;
    int lowersum;
        /* Number of pixels whose value is "less than" the median */

    findBoxBoundaries(colorfreqtable, depth, boxStart, boxSize,
                      minval, maxval);

    /* Find the largest dimension, and sort by that component.  I have
       included two methods for determining the "largest" dimension;
       first by simply comparing the range in RGB space, and second by
       transforming into luminosities before the comparison.
    */
    switch (methodForLargest) {
    case LARGE_NORM:
        largestDimension = largestByNorm(minval, maxval, depth);
        break;
    case LARGE_LUM:
        largestDimension = largestByLuminosity(minval, maxval, depth);
        break;
    default:
        quant_trace(stderr, "Internal error: invalid value of methodForLargest: %d\n",
                    methodForLargest);
        return -1;
    }

    /* TODO: I think this sort should go after creating a box,
       not before splitting.  Because you need the sort to use
       the REP_CENTER_BOX method of choosing a color to
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
        for (i = 1; i < boxSize - 1 && lowersum < sm/2; ++i) {
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
    return 0;
}



static int
mediancut(tupletable2 const colorfreqtable,
          unsigned int const depth,
          int const newcolors,
          int const methodForLargest,
          int const methodForRep,
          tupletable2 *const colormapP)
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

    sum = 0;

    for (i = 0; i < colorfreqtable.size; ++i)
        sum += colorfreqtable.table[i]->value;

        /* There is at least one box that contains at least 2 colors; ergo,
           there is more splitting we can do.
        */

    bv = newBoxVector(colorfreqtable.size, sum, newcolors);
    if (!bv) {
        return (-1);
    }
    boxes = 1;
    multicolorBoxesExist = (colorfreqtable.size > 1);

    /* Main loop: split boxes until we have enough. */
    while (boxes < newcolors && multicolorBoxesExist) {
        /* Find the first splittable box. */
        for (bi = 0; bi < boxes && bv[bi].colors < 2; ++bi);
        if (bi >= boxes)
            multicolorBoxesExist = 0;
        else
            splitBox(bv, &boxes, bi, colorfreqtable, depth, methodForLargest);
    }
    *colormapP = colormapFromBv(newcolors, bv, boxes,
                                colorfreqtable, depth,
                                methodForRep);

    free(bv);
    return 0;
}


static int
computeHistogram(unsigned char *data,
                 unsigned int length,
                 unsigned long const depth,
                 tupletable2 * const colorfreqtableP,
                 enum qualityMode const qualityMode)
{
    typedef unsigned short unit_t;
    unsigned int i, n;
    unit_t *histgram;
    unit_t *refmap;
    unit_t *ref;
    unit_t *it;
    struct tupleint *t;
    unsigned int index;
    unsigned int step;
    unsigned int max_sample;

    if (qualityMode == QUALITY_HIGH) {
        max_sample = 1118383;
    } else { /* if (qualityMode == QUALITY_LOW) */
        max_sample = 18383;
    }

    quant_trace(stderr, "making histogram...\n");

    histgram = malloc((1 << depth * 5) * sizeof(unit_t));
    if (!histgram) {
        quant_trace(stderr, "Unable to allocate memory for histgram.");
        return (-1);
    }
    memset(histgram, 0, (1 << depth * 5) * sizeof(unit_t));
    it = ref = refmap = (unsigned short *)malloc(max_sample * sizeof(unit_t));
    if (!it) {
        quant_trace(stderr, "Unable to allocate memory for lookup table.");
        return (-1);
    }

    if (length > max_sample * depth) {
        step = length / depth / max_sample;
    } else {
        step = depth;
    }

    for (i = 0; i < length; i += step) {
        index = 0;
        for (n = 0; n < depth; n++) {
            index |= data[i + depth - 1 - n] >> 3 << n * 5;
        }
        if (histgram[index] == 0) {
            *ref++ = index;
        }
        if (histgram[index] < (1 << sizeof(unsigned short) * 8) - 1) {
            histgram[index]++;
        }
    }

    colorfreqtableP->size = ref - refmap;
    colorfreqtableP->table = alloctupletable(depth, ref - refmap);
    for (i = 0; i < colorfreqtableP->size; ++i) {
        if (histgram[refmap[i]] > 0) {
            colorfreqtableP->table[i]->value = histgram[refmap[i]];
            for (n = 0; n < depth; n++) {
                colorfreqtableP->table[i]->tuple[depth - 1 - n]
                    = (*it >> n * 5 & 0x1f) << 3;
            }
        }
        it++;
    }

    free(refmap);
    free(histgram);

    quant_trace(stderr, "%u colors found\n", colorfreqtableP->size);
    return 0;
}


static int
computeColorMapFromInput(unsigned char *data,
                         size_t length,
                         unsigned int const depth,
                         int const reqColors,
                         enum methodForLargest const methodForLargest,
                         enum methodForRep const methodForRep,
                         enum qualityMode const qualityMode,
                         tupletable2 * const colormapP,
                         int *origcolors)
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
    tupletable2 colorfreqtable;
    int i, n;
    int ret;

    ret = computeHistogram(data, length, depth, &colorfreqtable, qualityMode);
    if (ret != 0) {
        return (-1);
    }
    if (origcolors) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= reqColors) {
        quant_trace(stderr, "Image already has few enough colors (<=%d).  "
                    "Keeping same colors.\n", reqColors);
        /* *colormapP = colorfreqtable; */
        colormapP->size = colorfreqtable.size;
        colormapP->table = alloctupletable(depth, colorfreqtable.size);
        for (i = 0; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0; n < depth; ++n) {
                colormapP->table[i]->tuple[n] = colorfreqtable.table[i]->tuple[n];
            }
        }
    } else {
        quant_trace(stderr, "choosing %d colors...\n", reqColors);
        ret = mediancut(colorfreqtable, depth, reqColors,
                        methodForLargest, methodForRep, colormapP);
        if (ret != 0) {
            return (-1);
        }
        quant_trace(stderr, "%d colors are choosed.\n", colorfreqtable.size);
    }

    free(colorfreqtable.table);
    return 0;
}


static void
error_diffuse(unsigned char *data, int pos, int depth,
              int offset, int mul, int div)
{
    int c;

    data += pos * depth;

    c = *data + offset * mul / div;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}


static void
diffuse_none(unsigned char *data, int width, int height,
             int x, int y, int depth, int offset)
{
}


static void
diffuse_atkinson(unsigned char *data, int width, int height,
                 int x, int y, int depth, int offset)
{
    int pos, n;

    pos = y * width + x;

    if (x < width - 2 && y < height - 2) {
        /* add offset to the right cell */
        error_diffuse(data, pos + width * 0 + 1, depth, offset, 1, 8);
        /* add offset to the 2th right cell */
        error_diffuse(data, pos + width * 0 + 2, depth, offset, 1, 8);
        /* add offset to the left-bottom cell */
        error_diffuse(data, pos + width * 1 - 1, depth, offset, 1, 8);
        /* add offset to the bottom cell */
        error_diffuse(data, pos + width * 1 + 0, depth, offset, 1, 8);
        /* add offset to the right-bottom cell */
        error_diffuse(data, pos + width * 1 + 1, depth, offset, 1, 8);
        /* add offset to the 2th bottom cell */
        error_diffuse(data, pos + width * 2 + 0, depth, offset, 1, 8);
    }
}


static void
diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int offset)
{
    int n;
    int pos;

    pos = y * width + x;

    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    if (x > 1 && x < width - 1 && y < height - 1) {
        /* add offset to the right cell */
        error_diffuse(data, pos + width * 0 + 1, depth, offset, 7, 16);
        /* add offset to the left-bottom cell */
        error_diffuse(data, pos + width * 1 - 1, depth, offset, 3, 16);
        /* add offset to the bottom cell */
        error_diffuse(data, pos + width * 1 + 0, depth, offset, 5, 16);
        /* add offset to the right-bottom cell */
        error_diffuse(data, pos + width * 1 + 1, depth, offset, 1, 16);
    }
}


static void
diffuse_jajuni(unsigned char *data, int width, int height,
               int x, int y, int depth, int offset)
{
    int n;
    int pos;

    pos = y * width + x;

    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    if (x > 2 && x < width - 2 && y < height - 2) {
        error_diffuse(data, pos + width * 0 + 1, depth, offset, 7, 48);
        error_diffuse(data, pos + width * 0 + 2, depth, offset, 5, 48);
        error_diffuse(data, pos + width * 1 - 2, depth, offset, 3, 48);
        error_diffuse(data, pos + width * 1 - 1, depth, offset, 5, 48);
        error_diffuse(data, pos + width * 1 + 0, depth, offset, 7, 48);
        error_diffuse(data, pos + width * 1 + 1, depth, offset, 5, 48);
        error_diffuse(data, pos + width * 1 + 2, depth, offset, 3, 48);
        error_diffuse(data, pos + width * 2 - 2, depth, offset, 1, 48);
        error_diffuse(data, pos + width * 2 - 1, depth, offset, 3, 48);
        error_diffuse(data, pos + width * 2 + 0, depth, offset, 5, 48);
        error_diffuse(data, pos + width * 2 + 1, depth, offset, 3, 48);
        error_diffuse(data, pos + width * 2 + 2, depth, offset, 1, 48);
    }
}


static void
diffuse_stucki(unsigned char *data, int width, int height,
               int x, int y, int depth, int offset)
{
    int n;
    int pos;

    pos = y * width + x;

    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    if (x > 2 && x < width - 2 && y < height - 2) {
        error_diffuse(data, pos + width * 0 + 1, depth, offset, 1, 6);
        error_diffuse(data, pos + width * 0 + 2, depth, offset, 1, 12);
        error_diffuse(data, pos + width * 1 - 2, depth, offset, 1, 24);
        error_diffuse(data, pos + width * 1 - 1, depth, offset, 1, 12);
        error_diffuse(data, pos + width * 1 + 0, depth, offset, 1, 6);
        error_diffuse(data, pos + width * 1 + 1, depth, offset, 1, 12);
        error_diffuse(data, pos + width * 1 + 2, depth, offset, 1, 24);
        error_diffuse(data, pos + width * 2 - 2, depth, offset, 1, 48);
        error_diffuse(data, pos + width * 2 - 1, depth, offset, 1, 24);
        error_diffuse(data, pos + width * 2 + 0, depth, offset, 1, 12);
        error_diffuse(data, pos + width * 2 + 1, depth, offset, 1, 24);
        error_diffuse(data, pos + width * 2 + 2, depth, offset, 1, 48);
    }
}


static void
diffuse_burkes(unsigned char *data, int width, int height,
               int x, int y, int depth, int offset)
{
    int n;
    int pos;

    pos = y * width + x;

    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    if (x > 2 && x < width - 2 && y < height - 2) {
        error_diffuse(data, pos + width * 0 + 1, depth, offset, 1, 4);
        error_diffuse(data, pos + width * 0 + 2, depth, offset, 1, 8);
        error_diffuse(data, pos + width * 1 - 2, depth, offset, 1, 16);
        error_diffuse(data, pos + width * 1 - 1, depth, offset, 1, 8);
        error_diffuse(data, pos + width * 1 + 0, depth, offset, 1, 4);
        error_diffuse(data, pos + width * 1 + 1, depth, offset, 1, 8);
        error_diffuse(data, pos + width * 1 + 2, depth, offset, 1, 16);
    }
}


static int
lookup_normal(unsigned char const * const pixel,
              int const depth,
              unsigned char const * const palette,
              int const ncolor,
              unsigned short * const cachetable)
{
    int index;
    int diff;
    int r;
    int i;
    int n;
    int distant;

    index = -1;
    diff = INT_MAX;

    for (i = 0; i < ncolor; i++) {
        distant = 0;
        for (n = 0; n < depth; ++n) {
            r = pixel[n] - palette[i * depth + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            index = i;
        }
    }

    return index;
}


static int
lookup_fast(unsigned char const * const pixel,
            int const depth,
            unsigned char const * const palette,
            int const ncolor,
            unsigned short * const cachetable)
{
    int hash;
    int index;
    int diff;
    int cache;
    int r;
    int i;
    int n;
    int distant;

    index = -1;
    diff = INT_MAX;
    hash = 0;

    for (n = 0; n < 3; ++n) {
        hash |= *(pixel + n) >> 3 << ((3 - 1 - n) * 5);
    }

    cache = cachetable[hash];
    if (cache) {  /* fast lookup */
        return cache - 1;
    }
    /* collision */
    for (i = 0; i < ncolor; i++) {
        distant = 0;
        for (n = 0; n < 3; ++n) {
            r = pixel[n] - palette[i * 3 + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            index = i;
        }
    }
    cachetable[hash] = index + 1;

    return index;
}


static int
lookup_mono_darkbg(unsigned char const * const pixel,
                   int const depth,
                   unsigned char const * const palette,
                   int const ncolor,
                   unsigned short * const cachetable)
{
    int n;
    int distant;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant >= 128 * ncolor ? 1: 0;
}


static int
lookup_mono_lightbg(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const ncolor,
                    unsigned short * const cachetable)
{
    int n;
    int distant;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant < 128 * ncolor ? 1: 0;
}


unsigned char *
LSQ_MakePalette(unsigned char *data, int x, int y, int depth,
                int reqcolors, int *ncolors, int *origcolors,
                int methodForLargest,
                int methodForRep,
                int qualityMode)
{
    int i, n;
    int ret;
    unsigned char *palette;
    tupletable2 colormap;

    ret = computeColorMapFromInput(data, x * y * depth, depth,
                                   reqcolors, methodForLargest,
                                   methodForRep, qualityMode,
                                   &colormap, origcolors);
    if (ret != 0) {
        return NULL;
    }
    *ncolors = colormap.size;
    quant_trace(stderr, "tupletable size: %d", *ncolors);
    palette = malloc(*ncolors * depth);
    for (i = 0; i < *ncolors; i++) {
        for (n = 0; n < depth; ++n) {
            palette[i * depth + n] = colormap.table[i]->tuple[n];
        }
    }

    free(colormap.table);
    return palette;
}

int
LSQ_ApplyPalette(unsigned char *data,
                 int width,
                 int height,
                 int depth,
                 unsigned char *palette,
                 int ncolor,
                 int methodForDiffuse,
                 int foptimize,
                 unsigned short *cachetable,
                 unsigned char *result)
{
    typedef int component_t;
    int pos, j, n, x, y, sum1, sum2;
    component_t offset;
    int diff;
    int index;
    unsigned short *indextable;
    void (*f_diffuse)(unsigned char *data, int width, int height,
                      int x, int y, int depth, int offset);
    int (*f_lookup)(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const ncolor,
                    unsigned short * const cachetable);

    if (depth != 3) {
        f_diffuse = diffuse_none;
    } else {
        switch (methodForDiffuse) {
        case DIFFUSE_NONE:
            f_diffuse = diffuse_none;
            break;
        case DIFFUSE_ATKINSON:
            f_diffuse = diffuse_atkinson;
            break;
        case DIFFUSE_FS:
            f_diffuse = diffuse_fs;
            break;
        case DIFFUSE_JAJUNI:
            f_diffuse = diffuse_jajuni;
            break;
        case DIFFUSE_STUCKI:
            f_diffuse = diffuse_stucki;
            break;
        case DIFFUSE_BURKES:
            f_diffuse = diffuse_burkes;
            break;
        default:
            quant_trace(stderr, "Internal error: invalid value of"
                                " methodForDiffuse: %d\n",
                        methodForDiffuse);
            f_diffuse = diffuse_none;
            break;
        }
    }

    f_lookup = NULL;
    if (ncolor == 2) {
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
        indextable = malloc((1 << depth * 5) * sizeof(unsigned short));
        if (!indextable) {
            quant_trace(stderr, "Unable to allocate memory for indextable.");
            return (-1);
        }
        memset(indextable, 0x00, (1 << depth * 5) * sizeof(unsigned short));
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            pos = y * width + x;
            index = f_lookup(data + (pos * depth), depth,
                             palette, ncolor, indextable);
            result[pos] = index;
            for (n = 0; n < depth; ++n) {
                offset = data[pos * depth + n] - palette[index * depth + n];
                f_diffuse(data + n, width, height, x, y, depth, offset);
            }
        }
    }

    if (cachetable == NULL) {
        free(indextable);
    }

    return 0;
}


void
LSQ_FreePalette(unsigned char * data)
{
    free(data);
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
