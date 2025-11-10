/*
 * SPDX-License-Identifier: MIT
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
 */

#include "config.h"

#include <stdlib.h>
#include <limits.h>

#include "lut.h"
#include "palette.h"
#include "allocator.h"
#include "status.h"
#include "compat_stub.h"

#include <stdarg.h>

#if HAVE_STRING_H
# include <string.h>
#endif

static int palette_default_lut_policy = SIXEL_LUT_POLICY_AUTO;
static int palette_method_for_largest = SIXEL_LARGE_NORM;

static float env_final_merge_target_factor = 1.81f;

/*
 * Median-cut and k-means helpers live here so palette.c can
 * coordinate palette generation end-to-end without jumping through
 * quant.c.  These routines are still based on the original Netpbm
 * sources, but comments were kept intact to aid maintenance.
 */

void
sixel_palette_reversible_tuple(sample *tuple,
                               unsigned int depth)
{
    unsigned int plane;
    unsigned int sample_value;

    for (plane = 0U; plane < depth; ++plane) {
        sample_value = (unsigned int)tuple[plane];
        tuple[plane] = (sample)sixel_palette_reversible_value(sample_value);
    }
}

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 unsigned int depth)
{
    unsigned int index;
    unsigned int plane;
    unsigned int sample_value;
    size_t offset;

    for (index = 0U; index < colors; ++index) {
        for (plane = 0U; plane < depth; ++plane) {
            offset = (size_t)index * (size_t)depth + (size_t)plane;
            sample_value = (unsigned int)palette[offset];
            palette[offset] = sixel_palette_reversible_value(sample_value);
        }
    }
}

void
sixel_palette_set_lut_policy(int lut_policy)
{
    int normalized;

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT
        || lut_policy == SIXEL_LUT_POLICY_ROBINHOOD
        || lut_policy == SIXEL_LUT_POLICY_HOPSCOTCH
        || lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        normalized = lut_policy;
    }

    palette_default_lut_policy = normalized;
}

void
sixel_palette_set_method_for_largest(int method)
{
    int normalized;

    normalized = SIXEL_LARGE_NORM;
    if (method == SIXEL_LARGE_NORM || method == SIXEL_LARGE_LUM) {
        normalized = method;
    } else if (method == SIXEL_LARGE_AUTO) {
        normalized = SIXEL_LARGE_NORM;
    }

    palette_method_for_largest = normalized;
}

typedef struct box* boxVector;
struct box {
    unsigned int ind;
    unsigned int colors;
    unsigned int sum;
};

static unsigned int compareplanePlane;
static tupletable2 const *force_palette_source;
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
        sixel_helper_set_additional_message("out of memory allocating box vector table");
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
               int const use_reversible,
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
#if HAVE_ASSERT
            assert("Internal error: invalid value of methodForRep");
#endif  /* HAVE_ASSERT */
            break;
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[bi]->tuple,
                                          depth);
        }
    }
    return colormap;
}


static int
force_palette_compare(const void *lhs, const void *rhs)
{
    unsigned int left;
    unsigned int right;
    unsigned int left_value;
    unsigned int right_value;

    left = *(const unsigned int *)lhs;
    right = *(const unsigned int *)rhs;
    left_value = force_palette_source->table[left]->value;
    right_value = force_palette_source->table[right]->value;
    if (left_value > right_value) {
        return -1;
    }
    if (left_value < right_value) {
        return 1;
    }
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}


static SIXELSTATUS
force_palette_completion(tupletable2 *colormapP,
                         unsigned int depth,
                         unsigned int reqColors,
                         tupletable2 const colorfreqtable,
                         sixel_allocator_t *allocator)
{
    /*
     * We enqueue "losers" from the histogram so that we can revive them:
     *
     *   histogram --> sort by hit count --> append to palette tail
     *        ^                             |
     *        +-----------------------------+
     *
     * The ASCII loop shows how discarded colors walk back into the
     * palette when the user demands an exact size.
     */
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable new_table = NULL;
    unsigned int *order = NULL;
    unsigned int current;
    unsigned int fill;
    unsigned int candidate;
    unsigned int plane;
    unsigned int source;

    current = colormapP->size;
    if (current >= reqColors) {
        return SIXEL_OK;
    }

    status = alloctupletable(&new_table, depth, reqColors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (colorfreqtable.size > 0U) {
        order = (unsigned int *)sixel_allocator_malloc(
            allocator, colorfreqtable.size * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (candidate = 0; candidate < colorfreqtable.size; ++candidate) {
            order[candidate] = candidate;
        }
        force_palette_source = &colorfreqtable;
        qsort(order, colorfreqtable.size, sizeof(unsigned int),
              force_palette_compare);
        force_palette_source = NULL;
    }

    for (fill = 0; fill < current; ++fill) {
        new_table[fill]->value = colormapP->table[fill]->value;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] =
                colormapP->table[fill]->tuple[plane];
        }
    }

    candidate = 0U;
    fill = current;
    if (order != NULL) {
        while (fill < reqColors && candidate < colorfreqtable.size) {
            unsigned int index;

            index = order[candidate];
            new_table[fill]->value = colorfreqtable.table[index]->value;
            for (plane = 0; plane < depth; ++plane) {
                new_table[fill]->tuple[plane] =
                    colorfreqtable.table[index]->tuple[plane];
            }
            ++fill;
            ++candidate;
        }
    }

    if (fill < reqColors && fill == 0U) {
        new_table[fill]->value = 0U;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = 0U;
        }
        ++fill;
    }

    source = 0U;
    while (fill < reqColors) {
        new_table[fill]->value = new_table[source]->value;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = new_table[source]->tuple[plane];
        }
        ++fill;
        ++source;
        if (source >= fill) {
            source = 0U;
        }
    }

    sixel_allocator_free(allocator, colormapP->table);
    colormapP->table = new_table;
    colormapP->size = reqColors;
    status = SIXEL_OK;

end:
    if (status != SIXEL_OK && new_table != NULL) {
        sixel_allocator_free(allocator, new_table);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    return status;
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



static unsigned int sixel_final_merge_target(unsigned int reqcolors,
                                             int final_merge_mode);
static void sixel_merge_clusters_ward(unsigned long *weights,
                                      unsigned long *sums,
                                      unsigned int depth,
                                      int *cluster_count,
                                      int target);
static SIXELSTATUS sixel_palette_clusters_to_colormap(unsigned long *weights,
                                                      unsigned long *sums,
                                                      unsigned int depth,
                                                      unsigned int cluster_count,
                                                      int use_reversible,
                                                      tupletable2 *colormapP,
                                                      sixel_allocator_t *allocator);

static SIXELSTATUS
mediancut(tupletable2 const colorfreqtable,
          unsigned int const depth,
          unsigned int const newcolors,
          int const methodForLargest,
          int const methodForRep,
          int const use_reversible,
          int const final_merge_mode,
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
    unsigned int working_colors;
    int apply_merge;
    unsigned long *cluster_weight;
    unsigned long *cluster_sums;
    int cluster_total;
    unsigned int plane;
    unsigned int offset;
    unsigned int size;
    unsigned long value;
    struct tupleint *entry;
    SIXELSTATUS merge_status;
    SIXELSTATUS status = SIXEL_FALSE;

    sum = 0;
    working_colors = newcolors;
    apply_merge = (final_merge_mode == SIXEL_FINAL_MERGE_AUTO
                   || final_merge_mode == SIXEL_FINAL_MERGE_WARD);
    bv = NULL;
    cluster_weight = NULL;
    cluster_sums = NULL;
    cluster_total = 0;
    plane = 0U;
    offset = 0U;
    size = 0U;
    value = 0UL;
    entry = NULL;
    merge_status = SIXEL_OK;

    for (i = 0; i < colorfreqtable.size; ++i) {
        sum += colorfreqtable.table[i]->value;
    }

    if (apply_merge) {
        /* Choose an oversplit target so that the merge stage has slack. */
        working_colors = sixel_final_merge_target(newcolors,
                                                  final_merge_mode);
        if (working_colors > colorfreqtable.size) {
            working_colors = colorfreqtable.size;
        }
        sixel_debugf("overshoot: %u", working_colors);
    }
    if (working_colors == 0U) {
        working_colors = 1U;
    }

    /* There is at least one box that contains at least 2 colors; ergo,
       there is more splitting we can do.  */
    bv = newBoxVector(colorfreqtable.size, sum, working_colors, allocator);
    if (bv == NULL) {
        goto end;
    }
    boxes = 1;
    multicolorBoxesExist = (colorfreqtable.size > 1);

    /* Main loop: split boxes until we have enough. */
    while (boxes < working_colors && multicolorBoxesExist) {
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
    if (apply_merge && boxes > newcolors) {
        /* Capture weight and component sums for each temporary box. */
        cluster_weight = (unsigned long *)sixel_allocator_malloc(
            allocator, (size_t)boxes * sizeof(unsigned long));
        cluster_sums = (unsigned long *)sixel_allocator_malloc(
            allocator, (size_t)boxes * (size_t)depth * sizeof(unsigned long));
        if (cluster_weight == NULL || cluster_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (bi = 0U; bi < boxes; ++bi) {
            offset = bv[bi].ind;
            size = bv[bi].colors;
            cluster_weight[bi] = 0UL;
            for (plane = 0U; plane < depth; ++plane) {
                cluster_sums[(size_t)bi * (size_t)depth + plane] = 0UL;
            }
            for (i = 0U; i < size; ++i) {
                entry = colorfreqtable.table[offset + i];
                value = (unsigned long)entry->value;
                cluster_weight[bi] += value;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane] +=
                        (unsigned long)entry->tuple[plane] * value;
                }
            }
        }
        cluster_total = (int)boxes;
        /* Merge clusters greedily using Ward's minimum variance rule. */
        sixel_merge_clusters_ward(cluster_weight, cluster_sums, depth,
                                  &cluster_total, (int)newcolors);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > newcolors) {
            cluster_total = (int)newcolors;
        }
        /* Rebuild the palette using the merged cluster statistics. */
        merge_status = sixel_palette_clusters_to_colormap(cluster_weight,
                                                         cluster_sums,
                                                         depth,
                                                         (unsigned int)cluster_total,
                                                         use_reversible,
                                                         colormapP,
                                                         allocator);
        if (SIXEL_FAILED(merge_status)) {
            status = merge_status;
            goto end;
        }
    } else {
        *colormapP = colormapFromBv(newcolors, bv, boxes,
                                    colorfreqtable, depth,
                                    methodForRep, use_reversible,
                                    allocator);
    }

    status = SIXEL_OK;

end:
    if (bv != NULL) {
        sixel_allocator_free(allocator, bv);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weight != NULL) {
        sixel_allocator_free(allocator, cluster_weight);
    }
    return status;
}


/* Determine how many clusters to create before the final merge step. */
static unsigned int
sixel_final_merge_target(unsigned int reqcolors,
                         int final_merge_mode)
{
    double factor;
    unsigned int scaled;

    if (final_merge_mode != SIXEL_FINAL_MERGE_AUTO
        && final_merge_mode != SIXEL_FINAL_MERGE_WARD) {
        return reqcolors;
    }
    factor = env_final_merge_target_factor;
    scaled = (unsigned int)((double)reqcolors * factor);
    if (scaled <= reqcolors) {
        scaled = reqcolors;
    }
    if (scaled < 1U) {
        scaled = 1U;
    }

    return scaled;
}


/* Merge clusters to the requested size using Ward linkage. */
static void
sixel_merge_clusters_ward(unsigned long *weights,
                          unsigned long *sums,
                          unsigned int depth,
                          int *cluster_count,
                          int target)
{
    int n;
    int desired;
    int best_i;
    int best_j;
    int i;
    int j;
    int k;
    int channel;
    double best_cost;
    double wi;
    double wj;
    double distance_sq;
    double delta;
    double mean_i;
    double mean_j;
    double diff;
    size_t offset_i;
    size_t offset_j;
    size_t dst;
    size_t src;

    if (cluster_count == NULL) {
        return;
    }
    n = *cluster_count;
    desired = target;
    if (desired < 1) {
        desired = 1;
    }
    while (n > desired) {
        best_i = -1;
        best_j = -1;
        best_cost = 1.0e300;
        for (i = 0; i < n; ++i) {
            if (weights[i] == 0UL) {
                continue;
            }
            wi = (double)weights[i];
            offset_i = (size_t)i * (size_t)depth;
            for (j = i + 1; j < n; ++j) {
                if (weights[j] == 0UL) {
                    continue;
                }
                wj = (double)weights[j];
                offset_j = (size_t)j * (size_t)depth;
                distance_sq = 0.0;
                for (channel = 0; channel < (int)depth; ++channel) {
                    mean_i = (double)sums[offset_i + (size_t)channel] / wi;
                    mean_j = (double)sums[offset_j + (size_t)channel] / wj;
                    diff = mean_i - mean_j;
                    distance_sq += diff * diff;
                }
                delta = (wi * wj) / (wi + wj) * distance_sq;
                if (delta < best_cost) {
                    best_cost = delta;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        if (best_i < 0 || best_j < 0) {
            break;
        }
        weights[best_i] += weights[best_j];
        offset_i = (size_t)best_i * (size_t)depth;
        offset_j = (size_t)best_j * (size_t)depth;
        for (channel = 0; channel < (int)depth; ++channel) {
            sums[offset_i + (size_t)channel] +=
                sums[offset_j + (size_t)channel];
        }
        for (k = best_j; k < n - 1; ++k) {
            weights[k] = weights[k + 1];
            dst = (size_t)k * (size_t)depth;
            src = (size_t)(k + 1) * (size_t)depth;
            for (channel = 0; channel < (int)depth; ++channel) {
                sums[dst + (size_t)channel] = sums[src + (size_t)channel];
            }
        }
        --n;
    }
    *cluster_count = n;
}


/* Translate merged cluster statistics into a tupletable palette. */
static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   unsigned long *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   tupletable2 *colormapP,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable2 colormap;
    unsigned int index;
    unsigned int plane;
    double component;
    unsigned long weight;

    status = SIXEL_BAD_ARGUMENT;
    if (colormapP == NULL) {
        return status;
    }
    colormap = newColorMap(cluster_count, depth, allocator);
    if (colormap.size == 0U) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < cluster_count; ++index) {
        weight = weights[index];
        if (weight == 0UL) {
            weight = 1UL;
        }
        colormap.table[index]->value =
            (unsigned int)((weight > (unsigned long)UINT_MAX)
                ? UINT_MAX
                : weight);
        for (plane = 0U; plane < depth; ++plane) {
            component = (double)sums[(size_t)index * (size_t)depth + plane];
            component /= (double)weight;
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            colormap.table[index]->tuple[plane] =
                (sample)(component + 0.5);
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[index]->tuple,
                                          depth);
        }
    }
    *colormapP = colormap;
    status = SIXEL_OK;

    return status;
}


static SIXELSTATUS
computeColorMapFromInput(unsigned char const *data,
                         unsigned int const length,
                         unsigned int const depth,
                         unsigned int const reqColors,
                         int const methodForLargest,
                         int const methodForRep,
                         int const qualityMode,
                         int const force_palette,
                         int const use_reversible,
                         int const final_merge_mode,
                         int const lut_policy,
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

    /*
     * Build a histogram using the same LUT policy that the palette
     * application stage will employ.  This keeps bucket packing and
     * sparse table strategies consistent across the pipeline.
     */
    status = sixel_lut_build_histogram(data,
                                       length,
                                       depth,
                                       qualityMode,
                                       use_reversible,
                                       lut_policy,
                                       &colorfreqtable,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= reqColors) {
        sixel_debugf("Image already has few enough colors (<=%u). "
                     "Keeping same colors.",
                     reqColors);
        /* *colormapP = colorfreqtable; */
        colormapP->size = colorfreqtable.size;
        status = alloctupletable(&colormapP->table,
                                 depth,
                                 colorfreqtable.size,
                                 allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (i = 0; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0; n < depth; ++n) {
                colormapP->table[i]->tuple[n] =
                    colorfreqtable.table[i]->tuple[n];
            }
            if (use_reversible) {
                sixel_palette_reversible_tuple(colormapP->table[i]->tuple,
                                              depth);
            }
        }
    } else {
        sixel_debugf("choosing %u colors...", reqColors);
        status = mediancut(colorfreqtable, depth, reqColors,
                           methodForLargest, methodForRep,
                           use_reversible, final_merge_mode,
                           colormapP, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_debugf("%u colors are chosen.",
                     colorfreqtable.size);
    }
    if (force_palette) {
        status = force_palette_completion(colormapP, depth, reqColors,
                                          colorfreqtable, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, colorfreqtable.table);
    return status;
}


static SIXELSTATUS
build_palette_kmeans(unsigned char **result,
                     unsigned char const *data,
                     unsigned int length,
                     unsigned int depth,
                     unsigned int reqcolors,
                     unsigned int *ncolors,
                     unsigned int *origcolors,
                     int quality_mode,
                     int force_palette,
                     int final_merge_mode,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_count;
    unsigned int sample_limit;
    unsigned int sample_cap;
    unsigned int valid_seen;
    unsigned int sample_count;
    unsigned int k;
    unsigned int index;
    unsigned int channel;
    unsigned int center_index;
    unsigned int sample_index;
    unsigned int replace;
    unsigned int max_iterations;
    unsigned int iteration;
    unsigned int best_index;
    unsigned int old_cluster;
    unsigned int farthest_index;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    unsigned int base;
    unsigned int extra_component;
    unsigned int *membership;
    unsigned int *order;
    unsigned char *samples;
    unsigned char *palette;
    unsigned char *new_palette;
    double *centers;
    double *distance_cache;
    double total_weight;
    double random_point;
    double best_distance;
    double distance;
    double diff;
    double update;
    double farthest_distance;
    unsigned long *counts;
    unsigned long *accum;
    unsigned long *channel_sum;
    unsigned long rand_value;
    int changed;
    int apply_merge;
    unsigned int overshoot;
    unsigned int refine_iterations;
    int cluster_total;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_count = 0U;
    sample_limit = 50000U;
    sample_cap = sample_limit;
    valid_seen = 0U;
    sample_count = 0U;
    k = 0U;
    index = 0U;
    channel = 0U;
    center_index = 0U;
    sample_index = 0U;
    replace = 0U;
    max_iterations = 0U;
    iteration = 0U;
    best_index = 0U;
    old_cluster = 0U;
    farthest_index = 0U;
    fill = 0U;
    source = 0U;
    swap_temp = 0U;
    base = 0U;
    extra_component = 0U;
    membership = NULL;
    order = NULL;
    samples = NULL;
    palette = NULL;
    new_palette = NULL;
    centers = NULL;
    distance_cache = NULL;
    counts = NULL;
    accum = NULL;
    channel_sum = NULL;
    rand_value = 0UL;
    total_weight = 0.0;
    random_point = 0.0;
    best_distance = 0.0;
    distance = 0.0;
    diff = 0.0;
    update = 0.0;
    farthest_distance = 0.0;
    changed = 0;
    apply_merge = 0;
    overshoot = 0U;
    refine_iterations = 0U;
    cluster_total = 0;

    if (result != NULL) {
        *result = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0U;
    }
    if (origcolors != NULL) {
        *origcolors = 0U;
    }

    if (channels != 3U && channels != 4U) {
        goto end;
    }
    if (channels == 0U) {
        goto end;
    }

    pixel_count = length / channels;
    if (pixel_count == 0U) {
        goto end;
    }
    if (pixel_count < sample_cap) {
        sample_cap = pixel_count;
    }
    if (sample_cap == 0U) {
        sample_cap = 1U;
    }

    samples = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)sample_cap * 3U);
    if (samples == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Reservoir sampling keeps the distribution fair when the image is
     * larger than our budget. Transparent pixels are skipped so that the
     * solver only sees visible colors.
     */
    for (index = 0U; index < pixel_count; ++index) {
        base = index * channels;
        if (channels == 4U && data[base + 3U] == 0U) {
            continue;
        }
        ++valid_seen;
        if (sample_count < sample_cap) {
            for (channel = 0U; channel < 3U; ++channel) {
                samples[sample_count * 3U + channel] =
                    data[base + channel];
            }
            ++sample_count;
        } else {
            rand_value = (unsigned long)rand();
            replace = (unsigned int)(rand_value % valid_seen);
            if (replace < sample_cap) {
                for (channel = 0U; channel < 3U; ++channel) {
                    samples[replace * 3U + channel] =
                        data[base + channel];
                }
            }
        }
    }

    if (origcolors != NULL) {
        *origcolors = valid_seen;
    }
    if (sample_count == 0U) {
        goto end;
    }

    if (reqcolors == 0U) {
        reqcolors = 1U;
    }
    apply_merge = (final_merge_mode == SIXEL_FINAL_MERGE_AUTO
                   || final_merge_mode == SIXEL_FINAL_MERGE_WARD);
    refine_iterations = 2U;
    overshoot = reqcolors;
    /* Oversplit so the subsequent Ward merge has room to consolidate. */
    if (apply_merge) {
        overshoot = sixel_final_merge_target(reqcolors,
                                             final_merge_mode);
        sixel_debugf("overshoot: %u", overshoot);
    }
    if (overshoot > sample_count) {
        overshoot = sample_count;
    }
    k = overshoot;
    if (k == 0U) {
        goto end;
    }

    centers = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    distance_cache = (double *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(double));
    counts = (unsigned long *)sixel_allocator_malloc(
        allocator, (size_t)k * sizeof(unsigned long));
    accum = (unsigned long *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(unsigned long));
    membership = (unsigned int *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(unsigned int));
    if (centers == NULL || distance_cache == NULL || counts == NULL ||
            accum == NULL || membership == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Seed the first center uniformly from the sampled set. Subsequent
     * centers use k-means++ to favour distant samples.
     */
    rand_value = (unsigned long)rand();
    replace = (unsigned int)(rand_value % sample_count);
    for (channel = 0U; channel < 3U; ++channel) {
        centers[channel] =
            (double)samples[replace * 3U + channel];
    }
    for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
        distance = 0.0;
        for (channel = 0U; channel < 3U; ++channel) {
            diff = (double)samples[sample_index * 3U + channel]
                - centers[channel];
            distance += diff * diff;
        }
        distance_cache[sample_index] = distance;
    }

    for (center_index = 1U; center_index < k; ++center_index) {
        total_weight = 0.0;
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            total_weight += distance_cache[sample_index];
        }
        random_point = 0.0;
        if (total_weight > 0.0) {
            random_point =
                ((double)rand() / ((double)RAND_MAX + 1.0)) *
                total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > distance_cache[sample_index]) {
            random_point -= distance_cache[sample_index];
            ++sample_index;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[center_index * 3U + channel] =
                (double)samples[sample_index * 3U + channel];
        }
        for (index = 0U; index < sample_count; ++index) {
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[index * 3U + channel]
                    - centers[center_index * 3U + channel];
                distance += diff * diff;
            }
            if (distance < distance_cache[index]) {
                distance_cache[index] = distance;
            }
        }
    }

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        max_iterations = 6U;
        break;
    case SIXEL_QUALITY_HIGH:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_FULL:
        max_iterations = 48U;
        break;
    case SIXEL_QUALITY_HIGHCOLOR:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_AUTO:
    default:
        max_iterations = 12U;
        break;
    }
    if (max_iterations == 0U) {
        max_iterations = 1U;
    }
    if (max_iterations > 20U) {
        /*
         * The requirements cap the Lloyd refinement at twenty passes to
         * keep runtime bounded even for demanding quality presets.
         */
        max_iterations = 20U;
    }

    /*
     * Lloyd refinement assigns samples to their nearest center and moves
     * each center to the mean of its cluster. Empty clusters are reseeded
     * using the farthest sample to improve stability.
     */
    for (iteration = 0U; iteration < max_iterations; ++iteration) {
        for (index = 0U; index < k; ++index) {
            counts[index] = 0UL;
        }
        for (index = 0U; index < k * 3U; ++index) {
            accum[index] = 0UL;
        }
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            best_index = 0U;
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[sample_index * 3U + channel]
                    - centers[channel];
                distance += diff * diff;
            }
            best_distance = distance;
            for (center_index = 1U; center_index < k;
                    ++center_index) {
                distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[center_index * 3U + channel];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = center_index;
                }
            }
            membership[sample_index] = best_index;
            distance_cache[sample_index] = best_distance;
            counts[best_index] += 1UL;
            channel_sum = accum + (size_t)best_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] +=
                    (unsigned long)samples[sample_index * 3U + channel];
            }
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] != 0UL) {
                continue;
            }
            farthest_distance = -1.0;
            farthest_index = 0U;
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                if (distance_cache[sample_index] > farthest_distance) {
                    farthest_distance = distance_cache[sample_index];
                    farthest_index = sample_index;
                }
            }
            old_cluster = membership[farthest_index];
            if (counts[old_cluster] > 0UL) {
                counts[old_cluster] -= 1UL;
                channel_sum = accum + (size_t)old_cluster * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    extra_component =
                        (unsigned int)samples[farthest_index * 3U + channel];
                    if (channel_sum[channel] >=
                            (unsigned long)extra_component) {
                        channel_sum[channel] -=
                            (unsigned long)extra_component;
                    } else {
                        channel_sum[channel] = 0UL;
                    }
                }
            }
            membership[farthest_index] = center_index;
            counts[center_index] = 1UL;
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] =
                    (unsigned long)samples[farthest_index * 3U + channel];
            }
            distance_cache[farthest_index] = 0.0;
        }
        changed = 0;
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
                continue;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                update = (double)channel_sum[channel]
                    / (double)counts[center_index];
                diff = centers[center_index * 3U + channel] - update;
                if (diff < 0.0) {
                    diff = -diff;
                }
                if (diff > 0.5) {
                    changed = 1;
                }
                centers[center_index * 3U + channel] = update;
            }
        }
        if (!changed) {
            break;
        }
    }

    if (apply_merge && k > reqcolors) {
        /* Merge the provisional clusters and polish with a few Lloyd steps. */
        cluster_total = (int)k;
        sixel_merge_clusters_ward(counts, accum, 3U,
                                  &cluster_total, (int)reqcolors);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > reqcolors) {
            cluster_total = (int)reqcolors;
        }
        k = (unsigned int)cluster_total;
        if (k == 0U) {
            k = 1U;
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
                counts[center_index] = 1UL;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] =
                    (double)channel_sum[channel]
                    / (double)counts[center_index];
            }
        }
        for (iteration = 0U; iteration < refine_iterations; ++iteration) {
            for (index = 0U; index < k; ++index) {
                counts[index] = 0UL;
            }
            for (index = 0U; index < k * 3U; ++index) {
                accum[index] = 0UL;
            }
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                best_index = 0U;
                best_distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[channel];
                    best_distance += diff * diff;
                }
                for (center_index = 1U; center_index < k;
                        ++center_index) {
                    distance = 0.0;
                    for (channel = 0U; channel < 3U; ++channel) {
                        diff = (double)samples[sample_index * 3U + channel]
                            - centers[center_index * 3U + channel];
                        distance += diff * diff;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = center_index;
                    }
                }
                membership[sample_index] = best_index;
                distance_cache[sample_index] = best_distance;
                counts[best_index] += 1UL;
                channel_sum = accum + (size_t)best_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] +=
                        (unsigned long)samples[sample_index * 3U + channel];
                }
            }
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] != 0UL) {
                    continue;
                }
                farthest_distance = -1.0;
                farthest_index = 0U;
                for (sample_index = 0U; sample_index < sample_count;
                        ++sample_index) {
                    if (distance_cache[sample_index] > farthest_distance) {
                        farthest_distance = distance_cache[sample_index];
                        farthest_index = sample_index;
                    }
                }
                old_cluster = membership[farthest_index];
                if (counts[old_cluster] > 0UL) {
                    counts[old_cluster] -= 1UL;
                    channel_sum = accum + (size_t)old_cluster * 3U;
                    for (channel = 0U; channel < 3U; ++channel) {
                        extra_component =
                            (unsigned int)samples[farthest_index * 3U + channel];
                        if (channel_sum[channel] >=
                                (unsigned long)extra_component) {
                            channel_sum[channel] -=
                                (unsigned long)extra_component;
                        } else {
                            channel_sum[channel] = 0UL;
                        }
                    }
                }
                membership[farthest_index] = center_index;
                counts[center_index] = 1UL;
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] =
                        (unsigned long)samples[farthest_index * 3U + channel];
                }
                distance_cache[farthest_index] = 0.0;
            }
            changed = 0;
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] == 0UL) {
                    continue;
                }
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    update = (double)channel_sum[channel]
                        / (double)counts[center_index];
                    diff = centers[center_index * 3U + channel] - update;
                    if (diff < 0.0) {
                        diff = -diff;
                    }
                    if (diff > 0.5) {
                        changed = 1;
                    }
                    centers[center_index * 3U + channel] = update;
                }
            }
            if (!changed) {
                break;
            }
        }
    }

    /*
     * Convert the floating point centers back into the byte palette that
     * callers expect.
     */
    palette = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (center_index = 0U; center_index < k; ++center_index) {
        for (channel = 0U; channel < 3U; ++channel) {
            update = centers[center_index * 3U + channel];
            if (update < 0.0) {
                update = 0.0;
            }
            if (update > 255.0) {
                update = 255.0;
            }
            palette[center_index * 3U + channel] =
                (unsigned char)(update + 0.5);
        }
    }

    if (force_palette && k < reqcolors) {
        /*
         * Populate the tail of the palette by repeating the most frequent
         * clusters so the caller still receives the requested palette size.
         */
        new_palette = (unsigned char *)sixel_allocator_malloc(
            allocator, (size_t)reqcolors * 3U);
        if (new_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k * 3U; ++index) {
            new_palette[index] = palette[index];
        }
        order = (unsigned int *)sixel_allocator_malloc(
            allocator, (size_t)k * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k; ++index) {
            order[index] = index;
        }
        for (index = 0U; index < k; ++index) {
            for (center_index = index + 1U; center_index < k;
                    ++center_index) {
                if (counts[order[center_index]] >
                        counts[order[index]]) {
                    swap_temp = order[index];
                    order[index] = order[center_index];
                    order[center_index] = swap_temp;
                }
            }
        }
        fill = k;
        source = 0U;
        while (fill < reqcolors && k > 0U) {
            center_index = order[source];
            for (channel = 0U; channel < 3U; ++channel) {
                new_palette[fill * 3U + channel] =
                    palette[center_index * 3U + channel];
            }
            ++fill;
            ++source;
            if (source >= k) {
                source = 0U;
            }
        }
        sixel_allocator_free(allocator, palette);
        palette = new_palette;
        new_palette = NULL;
        k = reqcolors;
    }

    status = SIXEL_OK;
    if (result != NULL) {
        *result = palette;
    } else {
        palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = k;
    }

end:
    if (status != SIXEL_OK && palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (new_palette != NULL) {
        sixel_allocator_free(allocator, new_palette);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (membership != NULL) {
        sixel_allocator_free(allocator, membership);
    }
    if (accum != NULL) {
        sixel_allocator_free(allocator, accum);
    }
    if (counts != NULL) {
        sixel_allocator_free(allocator, counts);
    }
    if (distance_cache != NULL) {
        sixel_allocator_free(allocator, distance_cache);
    }
    if (centers != NULL) {
        sixel_allocator_free(allocator, centers);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    return status;
}




static void
sixel_palette_dispose(sixel_palette_t *palette);

/*
 * Resize the palette entry buffer.
 *
 * The helper keeps the allocation logic in a single place so both k-means and
 * median-cut paths can rely on the same growth strategy.  When the caller
 * requests a size of zero the buffer is released entirely.
 */
static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_new(sixel_palette_t **palette, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *object;

    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_new: palette pointer is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *palette = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    object = (sixel_palette_t *)sixel_allocator_malloc(
        allocator, sizeof(*object));
    if (object == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_palette_new: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        *palette = NULL;
        goto end;
    }

    object->ref = 1U;
    object->allocator = allocator;
    object->entries = NULL;
    object->entries_size = 0U;
    object->entry_count = 0U;
    object->requested_colors = 0U;
    object->original_colors = 0U;
    object->depth = 0;
    object->method_for_largest = SIXEL_LARGE_AUTO;
    object->method_for_rep = SIXEL_REP_AUTO;
    object->quality_mode = SIXEL_QUALITY_AUTO;
    object->force_palette = 0;
    object->use_reversible = 0;
    object->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    object->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    object->complexion = 1;
    object->lut_policy = SIXEL_LUT_POLICY_AUTO;
    object->sixel_reversible = 0;
    object->final_merge = 0;
    object->lut = NULL;

    *palette = object;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI sixel_palette_t *
sixel_palette_ref(sixel_palette_t *palette)
{
    if (palette != NULL) {
        ++palette->ref;
    }

    return palette;
}

static void
sixel_palette_dispose(sixel_palette_t *palette)
{
    sixel_allocator_t *allocator;

    if (palette == NULL) {
        return;
    }

    allocator = palette->allocator;
    if (palette->entries != NULL) {
        sixel_allocator_free(allocator, palette->entries);
        palette->entries = NULL;
    }

    if (palette->lut != NULL) {
        sixel_lut_unref(palette->lut);
        palette->lut = NULL;
    }

    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI void
sixel_palette_unref(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return;
    }

    if (palette->ref > 1U) {
        --palette->ref;
        return;
    }

    sixel_palette_dispose(palette);
    sixel_allocator_free(palette->allocator, palette);
}

static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t required;
    unsigned char *resized;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    required = (size_t)colors * (size_t)depth;
    if (required == 0U) {
        if (palette->entries != NULL) {
            sixel_allocator_free(allocator, palette->entries);
            palette->entries = NULL;
        }
        palette->entries_size = 0U;
        return SIXEL_OK;
    }

    if (palette->entries != NULL && palette->entries_size >= required) {
        return SIXEL_OK;
    }

    if (palette->entries == NULL) {
        resized = (unsigned char *)sixel_allocator_malloc(allocator, required);
    } else {
        resized = (unsigned char *)sixel_allocator_realloc(allocator,
                                                           palette->entries,
                                                           required);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    palette->entries = resized;
    palette->entries_size = required;

    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize_entries(palette,
                                          colors,
                                          (unsigned int)depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette->entry_count = colors;
    palette->depth = depth;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize(palette, colors, depth, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (entries != NULL && palette->entries != NULL && payload_size > 0U) {
        memcpy(palette->entries, entries, payload_size);
    }

    return SIXEL_OK;
}

SIXELAPI unsigned char *
sixel_palette_get_entries(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return NULL;
    }

    return palette->entries;
}

SIXELAPI unsigned int
sixel_palette_get_entry_count(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0U;
    }

    return palette->entry_count;
}

SIXELAPI int
sixel_palette_get_depth(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0;
    }

    return palette->depth;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The procedure performs two major steps:
 *
 *   1. Try the k-means implementation when explicitly requested.  Successful
 *      runs populate a temporary buffer that is copied into the palette.
 *   2. Fall back to the existing histogram and median-cut pipeline.  The
 *      resulting tuple table is converted into tightly packed palette bytes.
 *
 * Both branches share helper routines for cache management and post-processing
 * (for example reversible palette transformation).  The palette object tracks
 * the generated metadata so the caller can publish it without recomputing.
 */
SIXELSTATUS
sixel_palette_generate(sixel_palette_t *palette,
                       unsigned char const *data,
                       unsigned int length,
                       int pixelformat,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colormap = { 0U, NULL };
    unsigned char *kmeans_entries = NULL;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result_depth = sixel_helper_compute_depth(pixelformat);
    if (result_depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)result_depth;

    status = SIXEL_FALSE;
    payload_size = 0U;

    if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = build_palette_kmeans(&kmeans_entries,
                                      data,
                                      length,
                                      depth,
                                      palette->requested_colors,
                                      &ncolors,
                                      &origcolors,
                                      palette->quality_mode,
                                      palette->force_palette,
                                      palette->final_merge_mode,
                                      work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            if (palette->use_reversible) {
                sixel_palette_reversible_palette(kmeans_entries,
                                                 ncolors,
                                                 depth);
            }
            payload_size = (size_t)ncolors * (size_t)depth;
            status = sixel_palette_resize_entries(palette,
                                                  ncolors,
                                                  depth,
                                                  work_allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (payload_size > 0U) {
                memcpy(palette->entries, kmeans_entries, payload_size);
            }
            status = SIXEL_OK;
            goto success;
        }
    }

    status = computeColorMapFromInput(data,
                                      length,
                                      depth,
                                      palette->requested_colors,
                                      palette->method_for_largest,
                                      palette->method_for_rep,
                                      palette->quality_mode,
                                      palette->force_palette,
                                      palette->use_reversible,
                                      palette->final_merge_mode,
                                      palette->lut_policy,
                                      &colormap,
                                      &origcolors,
                                      work_allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = colormap.size;
    status = sixel_palette_resize_entries(palette,
                                          ncolors,
                                          depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    payload_size = (size_t)ncolors * (size_t)depth;
    if (payload_size > 0U && palette->entries != NULL) {
        unsigned int i;
        unsigned int plane;

        for (i = 0U; i < ncolors; ++i) {
            for (plane = 0U; plane < depth; ++plane) {
                palette->entries[i * depth + plane]
                    = (unsigned char)colormap.table[i]->tuple[plane];
            }
        }
    }
    if (palette->use_reversible && palette->entries != NULL) {
        sixel_palette_reversible_palette(palette->entries,
                                         ncolors,
                                         depth);
    }
    status = SIXEL_OK;

success:
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)depth;

end:
    if (colormap.table != NULL) {
        sixel_allocator_free(work_allocator, colormap.table);
    }
    if (kmeans_entries != NULL) {
        sixel_allocator_free(work_allocator, kmeans_entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_make_palette(unsigned char **result,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           unsigned int reqcolors,
                           unsigned int *ncolors,
                           unsigned int *origcolors,
                           int methodForLargest,
                           int methodForRep,
                           int qualityMode,
                           int force_palette,
                           int use_reversible,
                           int quantize_model,
                           int final_merge_mode,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *palette = NULL;
    sixel_allocator_t *work_allocator;
    size_t payload_size;
    unsigned int depth;

    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;

    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (methodForLargest == SIXEL_LARGE_AUTO) {
        methodForLargest = palette_method_for_largest;
    }

    palette->requested_colors = reqcolors;
    palette->method_for_largest = methodForLargest;
    palette->method_for_rep = methodForRep;
    palette->quality_mode = qualityMode;
    palette->force_palette = force_palette;
    palette->use_reversible = use_reversible;
    palette->quantize_model = quantize_model;
    palette->final_merge_mode = final_merge_mode;
    palette->lut_policy = palette_default_lut_policy;

    status = sixel_palette_generate(palette,
                                    data,
                                    length,
                                    pixelformat,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (ncolors != NULL) {
        *ncolors = palette->entry_count;
    }
    if (origcolors != NULL) {
        *origcolors = palette->original_colors;
    }

    if (palette->depth <= 0 || palette->entry_count == 0U) {
        status = SIXEL_OK;
        goto end;
    }

    depth = (unsigned int)palette->depth;
    payload_size = (size_t)palette->entry_count * (size_t)depth;
    work_allocator = (allocator != NULL) ? allocator : palette->allocator;
    if (work_allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *result = (unsigned char *)sixel_allocator_malloc(work_allocator,
                                                      payload_size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_make_palette: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(*result, palette->entries, payload_size);

    status = SIXEL_OK;

end:
    if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    return status;
}

void
sixel_palette_free_palette(unsigned char *data,
                           sixel_allocator_t *allocator)
{
    sixel_allocator_free(allocator, data);
}

#if HAVE_TESTS
static int
palette_test_luminosity(void)
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

SIXELAPI int
sixel_palette_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (*palette_testcase)(void);

    static palette_testcase const testcases[] = {
        palette_test_luminosity,
    };

    for (i = 0U; i < sizeof(testcases) / sizeof(palette_testcase); ++i) {
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

/*
 * Palette generation, reversible tone helpers, and histogram coordination now
 * live together in this module.  Future refactors can continue to fold any
 * remaining quantization utilities here so palette.c stays the central entry
 * point for palette lifecycle management.
 */
