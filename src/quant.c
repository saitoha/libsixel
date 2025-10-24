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
static void diffuse_lso1(unsigned char *data, int width, int height,
                         int x, int y, int depth, int error, int direction);
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
static void diffuse_lso1_carry(int32_t *carry_curr, int32_t *carry_next,
                               int32_t *carry_far, int width, int height,
                               int depth, int x, int y, int32_t error,
                               int direction, int channel);

static const int (*
lso2_table(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

static const int (*
lso3_table(void))[7]
{
/* Auto-generated by gen_varcoefs.awk */
#include "lso3.h"
    return var_coefs;
}


#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

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


static int histogram_lut_policy = SIXEL_LUT_POLICY_AUTO;

void
sixel_quant_set_lut_policy(int lut_policy)
{
    int normalized;

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT) {
        normalized = lut_policy;
    }

    histogram_lut_policy = normalized;
}

struct histogram_control {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
};

static struct histogram_control
histogram_control_make(unsigned int depth);
static size_t histogram_size(unsigned int depth,
                             struct histogram_control const *control);
static unsigned int histogram_reconstruct(unsigned int quantized,
                                          struct histogram_control const
                                              *control);

static size_t
histogram_size(unsigned int depth,
               struct histogram_control const *control)
{
    size_t size;
    unsigned int exponent;

    size = 1U;
    exponent = depth * control->channel_bits;
    size <<= exponent;

    return size;
}

static unsigned int
histogram_reconstruct(unsigned int quantized,
                      struct histogram_control const *control)
{
    unsigned int value;

    value = quantized << control->channel_shift;
    if (quantized == control->channel_mask) {
        value = 255U;
    } else {
        if (control->channel_shift > 0U) {
            value |= (1U << (control->channel_shift - 1U));
        }
    }
    if (value > 255U) {
        value = 255U;
    }

    return value;
}

static unsigned int
histogram_quantize(unsigned int sample8,
                   struct histogram_control const *control)
{
    unsigned int quantized;
    unsigned int shift;
    unsigned int mask;
    unsigned int rounding;

    /*
     * We want each bucket to capture its center value instead of the lower
     * edge.  The ASCII sketch below shows how rounding keeps the midpoint:
     *
     *   0---1---2---3        sample8 + round
     *   |   |   |   |  ==>  ----------------  -> bucket index
     *   0   1   2   3              2^shift
     */
    shift = control->channel_shift;
    mask = control->channel_mask;
    if (shift == 0U) {
        quantized = sample8;
    } else {
        rounding = 1U << (shift - 1U);
        quantized = (sample8 + rounding) >> shift;
        if (quantized > mask) {
            quantized = mask;
        }
    }

    return quantized;
}

static unsigned int
computeHash(unsigned char const *data,
            unsigned int const depth,
            struct histogram_control const *control)
{
    unsigned int hash;
    unsigned int n;
    unsigned int sample8;
    unsigned int bits;

    hash = 0;
    bits = control->channel_bits;
    for (n = 0; n < depth; n++) {
#if 0
        hash |= (unsigned int)(data[depth - 1 - n] >> 3) << n * 5;
#else
        sample8 = (unsigned int)data[depth - 1 - n];
        hash |= histogram_quantize(sample8, control) << (n * bits);
#endif
    }

    return hash;
}

static struct histogram_control
histogram_control_make(unsigned int depth)
{
    struct histogram_control control;

    /*
     * The ASCII ladder below shows how each policy selects bucket width.
     *
     *   auto / 6bit RGB : |--6--|
     *   forced 5bit     : |---5---|
     *   alpha fallback  : |---5---|  (avoids 2^(6*4) buckets)
     */
    control.channel_shift = 2U;
    if (depth > 3U) {
        control.channel_shift = 3U;
    }
    if (histogram_lut_policy == SIXEL_LUT_POLICY_5BIT) {
        control.channel_shift = 3U;
    } else if (histogram_lut_policy == SIXEL_LUT_POLICY_6BIT) {
        control.channel_shift = 2U;
        if (depth > 3U) {
            control.channel_shift = 3U;
        }
    }
    control.channel_bits = 8U - control.channel_shift;
    control.channel_mask = (1U << control.channel_bits) - 1U;

    return control;
}

size_t
sixel_quant_fast_cache_size(void)
{
    struct histogram_control control;
    size_t size;

    /*
     * The figure below shows how the fast RGB cache subdivides the cube.
     *
     *   RRRRRR GGGGGG BBBBBB
     *   |----| |----| |----|
     *
     * Each group of six bits selects one face of the histogram lattice,
     * producing 2^(6*3) buckets.
     */
    control = histogram_control_make(3U);
    size = histogram_size(3U, &control);

    return size;
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
    typedef uint32_t unit_t;
    unsigned int i, n;
    unit_t *histogram = NULL;
    unit_t *refmap = NULL;
    unit_t *ref;
    unsigned int bucket_index;
    unsigned int step;
    unsigned int max_sample;
    size_t hist_size;
    unit_t bucket_value;
    unsigned int component;
    unsigned int reconstructed;
    struct histogram_control control;

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

    control = histogram_control_make((unsigned int)depth);
    hist_size = histogram_size((unsigned int)depth, &control);
    histogram = (unit_t *)sixel_allocator_calloc(allocator,
                                                 hist_size,
                                                 sizeof(unit_t));
    if (histogram == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for histogram.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    ref = refmap = (unit_t *)sixel_allocator_malloc(allocator,
                                                    hist_size *
                                                    sizeof(unit_t));
    if (refmap == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for lookup table.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (i = 0; i < length; i += step) {
        bucket_index = computeHash(data + i,
                                   (unsigned int)depth,
                                   &control);
        if (histogram[bucket_index] == 0) {
            *ref++ = bucket_index;
        }
        if (histogram[bucket_index] < UINT32_MAX) {
            histogram[bucket_index]++;
        }
    }

    colorfreqtableP->size = (unsigned int)(ref - refmap);

    status = alloctupletable(&colorfreqtableP->table,
                             depth,
                             (unsigned int)(ref - refmap),
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (i = 0; i < colorfreqtableP->size; ++i) {
        bucket_value = refmap[i];
        if (histogram[bucket_value] > 0) {
            colorfreqtableP->table[i]->value = histogram[bucket_value];
            for (n = 0; n < depth; n++) {
                component = (unsigned int)
                    ((bucket_value >> (n * control.channel_bits)) &
                     control.channel_mask);
                reconstructed = histogram_reconstruct(component,
                                                      &control);
                colorfreqtableP->table[i]->tuple[depth - 1 - n]
                    = (sample)reconstructed;
            }
        }
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
diffuse_lso3(unsigned char *data, int width, int height,
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

    table = lso3_table();
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
        if (x - 1 >= 0) {
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
            carry_curr[base] += term_r;
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
diffuse_lso3_carry(int32_t *carry_curr, int32_t *carry_next, int32_t *carry_far,
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

    table = lso3_table();
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
apply_palette_positional(
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
apply_palette_variable(
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
    case SIXEL_DIFFUSE_LSO3:
        varerr_diffuse = diffuse_lso3;
        varerr_diffuse_carry = diffuse_lso3_carry;
        srand((unsigned int)time(NULL));
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
                if (methodForDiffuse == SIXEL_DIFFUSE_LSO3) {
                    table_index = zhoufang_index_from_byte(
                        (unsigned char)source_pixel[n]);
                }
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
apply_palette_fixed(
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
        case SIXEL_DIFFUSE_LSO1:
            f_diffuse = diffuse_lso1;
            f_diffuse_carry = diffuse_lso1_carry;
            break;
        default:
            quant_trace(stderr,
                        "Internal error: invalid methodForDiffuse: %d\n",
                        methodForDiffuse);
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
     *  3/16    5/48    1/16
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


static void
diffuse_lso1_carry(int32_t *carry_curr, int32_t *carry_next,
                   int32_t *carry_far, int width, int height,
                   int depth, int x, int y, int32_t error,
                   int direction, int channel)
{
    int sign;
    int32_t edge_term;
    int32_t center_term;
    int32_t far_term;

    /* unused */ (void) carry_curr;
    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    edge_term = diffuse_fixed_term(error, 1, 8);
    center_term = diffuse_fixed_term(error, 4, 8);
    far_term = diffuse_fixed_term(error, 2, 8);

    if (y + 1 < height) {
        if (x - sign >= 0 && x - sign < width) {
            size_t base;

            base = ((size_t)(x - sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += edge_term;
        }
        {
            size_t base;

            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += center_term;
        }
        if (x + sign >= 0 && x + sign < width) {
            size_t base;

            base = ((size_t)(x + sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += edge_term;
        }
    }
    if (y + 2 < height) {
        size_t base;

        base = ((size_t)x * (size_t)depth) + (size_t)channel;
        carry_far[base] += far_term;
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
    struct histogram_control control;

    /* don't use depth in 'fast' strategy because it's always 3 */
    (void) depth;

    result = (-1);
    diff = INT_MAX;
    control = histogram_control_make(3U);
    hash = computeHash(pixel, 3U, &control);

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
    int               /* in */  methodForCarry,
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
    size_t cache_size;
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    unsigned short migration_map[SIXEL_PALETTE_MAX];
    int (*f_lookup)(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion);
    int use_varerr;
    int use_positional;
    int carry_mode;

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
    use_varerr = (depth == 3
                  && (methodForDiffuse == SIXEL_DIFFUSE_LSO2
                      || methodForDiffuse == SIXEL_DIFFUSE_LSO3));
    use_positional = (methodForDiffuse == SIXEL_DIFFUSE_A_DITHER
                      || methodForDiffuse == SIXEL_DIFFUSE_X_DITHER);
    carry_mode = (methodForCarry == SIXEL_CARRY_ENABLE)
               ? SIXEL_CARRY_ENABLE
               : SIXEL_CARRY_DISABLE;

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
        cache_size = sixel_quant_fast_cache_size();
        indextable = (unsigned short *)
            sixel_allocator_calloc(allocator,
                                   cache_size,
                                   sizeof(unsigned short));
        if (!indextable) {
            quant_trace(stderr, "Unable to allocate memory for indextable.\n");
            goto end;
        }
    }

    if (use_positional) {
        status = apply_palette_positional(result, data, width, height,
                                          depth, palette, reqcolor,
                                          methodForDiffuse, methodForScan,
                                          foptimize_palette, f_lookup,
                                          indextable, complexion, copy,
                                          new_palette, migration_map,
                                          ncolors);
    } else if (use_varerr) {
        status = apply_palette_variable(result, data, width, height,
                                        depth, palette, reqcolor,
                                        methodForScan, foptimize_palette,
                                        f_lookup, indextable, complexion,
                                        new_palette, migration_map,
                                        ncolors,
                                        methodForDiffuse,
                                        carry_mode);
    } else {
        status = apply_palette_fixed(result, data, width, height,
                                     depth, palette, reqcolor,
                                     methodForScan, foptimize_palette,
                                     f_lookup, indextable, complexion,
                                     new_palette, migration_map,
                                     ncolors, methodForDiffuse,
                                     carry_mode);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (cachetable == NULL) {
        sixel_allocator_free(allocator, indextable);
    }

    status = SIXEL_OK;

end:
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
