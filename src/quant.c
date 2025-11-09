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

#if defined(SIXEL_USE_SSE2)
# include <emmintrin.h>
# include <xmmintrin.h>
#elif defined(SIXEL_USE_NEON)
# include <arm_neon.h>
#endif

#include "quant.h"
#include "palette.h"
#include "quant-internal.h"
#include "compat_stub.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sixel_color_t;

static float env_final_merge_target_factor = 1.81;

static unsigned char const sixel_safe_tones[256] = {
    0,   0,   3,   3,   5,   5,   5,   8,   8,   10,  10,  10,  13,  13,  13,
    15,  15,  18,  18,  18,  20,  20,  23,  23,  23,  26,  26,  28,  28,  28,
    31,  31,  33,  33,  33,  36,  36,  38,  38,  38,  41,  41,  41,  43,  43,
    46,  46,  46,  48,  48,  51,  51,  51,  54,  54,  56,  56,  56,  59,  59,
    61,  61,  61,  64,  64,  64,  66,  66,  69,  69,  69,  71,  71,  74,  74,
    74,  77,  77,  79,  79,  79,  82,  82,  84,  84,  84,  87,  87,  89,  89,
    89,  92,  92,  92,  94,  94,  97,  97,  97,  99,  99,  102, 102, 102, 105,
    105, 107, 107, 107, 110, 110, 112, 112, 112, 115, 115, 115, 117, 117, 120,
    120, 120, 122, 122, 125, 125, 125, 128, 128, 130, 130, 130, 133, 133, 135,
    135, 135, 138, 138, 140, 140, 140, 143, 143, 143, 145, 145, 148, 148, 148,
    150, 150, 153, 153, 153, 156, 156, 158, 158, 158, 161, 161, 163, 163, 163,
    166, 166, 166, 168, 168, 171, 171, 171, 173, 173, 176, 176, 176, 179, 179,
    181, 181, 181, 184, 184, 186, 186, 186, 189, 189, 191, 191, 191, 194, 194,
    194, 196, 196, 199, 199, 199, 201, 201, 204, 204, 204, 207, 207, 209, 209,
    209, 212, 212, 214, 214, 214, 217, 217, 217, 219, 219, 222, 222, 222, 224,
    224, 227, 227, 227, 230, 230, 232, 232, 232, 235, 235, 237, 237, 237, 240,
    240, 242, 242, 242, 245, 245, 245, 247, 247, 250, 250, 250, 252, 252, 255,
    255
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

typedef struct {
    int index;
    int left;
    int right;
    unsigned char axis;
} sixel_certlut_node_t;

typedef struct {
    uint32_t *level0;
    uint8_t *pool;
    uint32_t pool_size;
    uint32_t pool_capacity;
    int wR;
    int wG;
    int wB;
    uint64_t wR2;
    uint64_t wG2;
    uint64_t wB2;
    int32_t wr_scale[256];
    int32_t wg_scale[256];
    int32_t wb_scale[256];
    int32_t *wr_palette;
    int32_t *wg_palette;
    int32_t *wb_palette;
    sixel_color_t const *palette;
    int ncolors;
    sixel_certlut_node_t *kdnodes;
    int kdnodes_count;
    int kdtree_root;
} sixel_certlut_t;

#define SIXEL_CERTLUT_BRANCH_FLAG 0x40000000U
/* #define DEBUG_CERTLUT_TRACE 1 */

static void sixel_quant_cell_center(int rmin, int gmin, int bmin, int size,
                                    int *cr, int *cg, int *cb);
static void sixel_quant_weight_init(sixel_certlut_t *lut, int wR, int wG,
                                    int wB);
static uint64_t sixel_certlut_distance_precomputed(
    sixel_certlut_t const *lut,
    int index,
    int32_t wr_r,
    int32_t wg_g,
    int32_t wb_b);
static int sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                           int index, int axis);
static void sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                                       int *indices, int count, int axis);
static SIXELSTATUS sixel_certlut_kdtree_build(sixel_certlut_t *lut);
static int sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
                                                int *indices,
                                                int count,
                                                int depth);
static uint64_t sixel_certlut_axis_distance(sixel_certlut_t const *lut,
                                            int diff, int axis);
static void sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                             int candidate,
                                             int32_t wr_r,
                                             int32_t wg_g,
                                             int32_t wb_b,
                                             int *best_idx,
                                             uint64_t *best_dist,
                                             int *second_idx,
                                             uint64_t *second_dist);
static void sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                                        int node_index,
                                        int r,
                                        int g,
                                        int b,
                                        int32_t wr_r,
                                        int32_t wg_g,
                                        int32_t wb_b,
                                        int *best_idx,
                                        uint64_t *best_dist,
                                        int *second_idx,
                                        uint64_t *second_dist);
static void sixel_quant_distance_pair(sixel_certlut_t const *lut, int r,
                                      int g, int b, int *best_idx,
                                      int *second_idx, uint64_t *best_dist,
                                      uint64_t *second_dist);
static int sixel_quant_is_cell_safe(sixel_certlut_t const *lut, int best_idx,
                                    int second_idx, int size,
                                    uint64_t best_dist,
                                    uint64_t second_dist);
static uint32_t sixel_quant_pool_alloc(sixel_certlut_t *lut, int *status);
static void sixel_quant_assign_leaf(uint32_t *cell, int palette_index);
static void sixel_quant_assign_branch(uint32_t *cell, uint32_t offset);
static uint8_t sixel_certlut_fallback(sixel_certlut_t const *lut,
                                      int r, int g, int b);
static int sixel_certlut_build_cell(sixel_certlut_t *lut, uint32_t *cell,
                                    int rmin, int gmin, int bmin, int size);
static int sixel_certlut_init(sixel_certlut_t *lut);
static void sixel_certlut_release(sixel_certlut_t *lut);
static int sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut);

static sixel_certlut_t *certlut_context = NULL;

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
 * quantization
 *
 *****************************************************************************/

static unsigned char
sixel_quant_reversible_value(unsigned int sample)
{
    if (sample > 255U) {
        sample = 255U;
    }

    return sixel_safe_tones[sample];
}

static void
sixel_quant_reversible_pixel(unsigned char const *src,
                             unsigned int depth,
                             unsigned char *dst)
{
    unsigned int plane;

    for (plane = 0U; plane < depth; ++plane) {
        dst[plane] = sixel_quant_reversible_value(src[plane]);
    }
}

static void
sixel_quant_reversible_tuple(sample *tuple,
                             unsigned int depth)
{
    unsigned int plane;
    unsigned int sample_value;

    for (plane = 0U; plane < depth; ++plane) {
        sample_value = (unsigned int)tuple[plane];
        tuple[plane] =
            (sample)sixel_quant_reversible_value(sample_value);
    }
}

void
sixel_quant_reversible_palette(unsigned char *palette,
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
            palette[offset] =
                sixel_quant_reversible_value(sample_value);
        }
    }
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
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
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
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
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
        sixel_compat_snprintf(
            message,
            sizeof(message),
            "unable to allocate %u bytes for a %u-entry tuple table",
            allocSize,
            size);
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
            quant_trace(stderr, "Internal error: "
                                "invalid value of methodForRep: %d\n",
                        methodForRep);
        }
        if (use_reversible) {
            sixel_quant_reversible_tuple(colormap.table[bi]->tuple,
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
static SIXELSTATUS sixel_quant_clusters_to_colormap(unsigned long *weights,
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
        quant_trace(stderr, "overshoot: %d\n", working_colors);
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
        merge_status = sixel_quant_clusters_to_colormap(cluster_weight,
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
sixel_quant_clusters_to_colormap(unsigned long *weights,
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
            sixel_quant_reversible_tuple(colormap.table[index]->tuple,
                                         depth);
        }
    }
    *colormapP = colormap;
    status = SIXEL_OK;

    return status;
}


static int histogram_lut_policy = SIXEL_LUT_POLICY_AUTO;
static int quant_method_for_largest = SIXEL_LARGE_NORM;

void
sixel_quant_set_lut_policy(int lut_policy)
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

    histogram_lut_policy = normalized;
}

void
sixel_quant_set_method_for_largest(int method)
{
    int normalized;

    normalized = SIXEL_LARGE_NORM;
    if (method == SIXEL_LARGE_NORM || method == SIXEL_LARGE_LUM) {
        normalized = method;
    } else if (method == SIXEL_LARGE_AUTO) {
        normalized = SIXEL_LARGE_NORM;
    }

    quant_method_for_largest = normalized;
}

size_t
histogram_dense_size(unsigned int depth,
                     struct histogram_control const *control)
{
    size_t size;
    unsigned int exponent;
    unsigned int i;

    size = 1U;
    exponent = depth * control->channel_bits;
    for (i = 0U; i < exponent; ++i) {
        if (size > SIZE_MAX / 2U) {
            size = SIZE_MAX;
            break;
        }
        size <<= 1U;
    }

    return size;
}

struct histogram_control
histogram_control_make_for_policy(unsigned int depth, int lut_policy)
{
    struct histogram_control control;

    control.reversible_rounding = 0;
    /*
     * The ASCII ladder below shows how each policy selects bucket width.
     *
     *   auto / 6bit RGB : |--6--|
     *   forced 5bit     : |---5---|
     *   robinhood       : |------8------|
     *   alpha fallback  : |---5---|  (avoids 2^(6*4) buckets)
     */
    control.channel_shift = 2U;
    if (depth > 3U) {
        control.channel_shift = 3U;
    }
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        control.channel_shift = 3U;
    } else if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        control.channel_shift = 2U;
        if (depth > 3U) {
            control.channel_shift = 3U;
        }
    } else if (lut_policy == SIXEL_LUT_POLICY_ROBINHOOD
               || lut_policy == SIXEL_LUT_POLICY_HOPSCOTCH) {
        control.channel_shift = 0U;
    } else if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        control.channel_shift = 2U;
    }
    control.channel_bits = 8U - control.channel_shift;
    control.channel_mask = (1U << control.channel_bits) - 1U;

    return control;
}

unsigned int
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
     * In reversible mode we already rounded once when snapping to
     * sixel_safe_tones[].  If we rounded to the nearest midpoint
     * again, the second pass would fall back to the lower bucket and break
     * the round-trip.  Biasing towards the upper edge keeps the bucket
     * stable after decoding and encoding again.  Non-reversible runs keep
     * symmetric midpoints to avoid nudging colors upwards.
     *
     *        midpoint bias        upper-edge bias
     *   |----*----|----*----|    |----|----*----|
     *   0         8        16    0    8        16
     *
     * The asterisk marks the midpoint captured by a bucket.  Moving that
     * midpoint to the upper edge keeps reversible tones from drifting.
     */
    shift = control->channel_shift;
    mask = control->channel_mask;
    if (shift == 0U) {
        quantized = sample8;
    } else {
        if (control->reversible_rounding) {
            rounding = 1U << shift;
        } else {
            rounding = 1U << (shift - 1U);
        }
        quantized = (sample8 + rounding) >> shift;
        if (quantized > mask) {
            quantized = mask;
        }
    }

    return quantized;
}

uint32_t
histogram_pack_color(unsigned char const *data,
                     unsigned int const depth,
                     struct histogram_control const *control)
{
    uint32_t packed;
    unsigned int n;
    unsigned int sample8;
    unsigned int bits;

    packed = 0U;
    bits = control->channel_bits;
    if (control->channel_shift == 0U) {
        /*
         * The channel shift being zero means each component keeps eight
         * bits.  We therefore pack pixels in RGB order, as illustrated
         * below:
         *
         *      R   G   B
         *     [ ]-[ ]-[ ]
         *      |   |   |
         *      v   v   v
         *     0xRRGGBB
         */
        for (n = 0U; n < depth; ++n) {
            packed |= (uint32_t)data[depth - 1U - n] << (n * bits);
        }
        return packed;
    }

    for (n = 0U; n < depth; ++n) {
        sample8 = (unsigned int)data[depth - 1U - n];
        packed |= histogram_quantize(sample8, control) << (n * bits);
    }

    return packed;
}

uint32_t
histogram_hash_mix(uint32_t key)
{
    uint32_t hash;

    /*
     * Multiplicative mixing with two avalanching rounds keeps nearby
     * colors far apart in the hash domain.  The final tweak avoids the
     * 0xffffffff sentinel used by hopscotch slots.
     */
    hash = key * 0x9e3779b9U;
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    hash *= 0x846ca68bU;
    hash ^= hash >> 16;
    if (hash == 0xffffffffU) {
        hash ^= 0x632be59bU;
    }

    return hash;
}

static unsigned int
computeHash(unsigned char const *data,
            unsigned int const depth,
            struct histogram_control const *control)
{
    uint32_t packed;

    packed = histogram_pack_color(data, depth, control);

    return histogram_hash_mix(packed);
}

#define CUCKOO_BUCKET_SIZE 4U
#define CUCKOO_MAX_KICKS 128U
#define CUCKOO_STASH_SIZE 32U
#define CUCKOO_EMPTY_KEY 0xffffffffU

struct cuckoo_bucket32 {
    uint32_t key[CUCKOO_BUCKET_SIZE];
    uint32_t value[CUCKOO_BUCKET_SIZE];
};

struct cuckoo_table32 {
    struct cuckoo_bucket32 *buckets;
    uint32_t stash_key[CUCKOO_STASH_SIZE];
    uint32_t stash_value[CUCKOO_STASH_SIZE];
    size_t bucket_count;
    size_t bucket_mask;
    size_t stash_count;
    size_t entry_count;
    sixel_allocator_t *allocator;
};

static size_t cuckoo_round_buckets(size_t hint);
static size_t cuckoo_hash_primary(uint32_t key, size_t mask);
static size_t cuckoo_hash_secondary(uint32_t key, size_t mask);
static size_t cuckoo_hash_alternate(uint32_t key,
                                    size_t bucket,
                                    size_t mask);
static uint32_t *cuckoo_bucket_find(struct cuckoo_bucket32 *bucket,
                                    uint32_t key);
static int cuckoo_bucket_insert_direct(struct cuckoo_bucket32 *bucket,
                                       uint32_t key,
                                       uint32_t value);
static SIXELSTATUS cuckoo_table32_init(struct cuckoo_table32 *table,
                                       size_t expected,
                                       sixel_allocator_t *allocator);
static void cuckoo_table32_clear(struct cuckoo_table32 *table);
static void cuckoo_table32_fini(struct cuckoo_table32 *table);
static uint32_t *cuckoo_table32_lookup(struct cuckoo_table32 *table,
                                       uint32_t key);
static SIXELSTATUS cuckoo_table32_insert(struct cuckoo_table32 *table,
                                         uint32_t key,
                                         uint32_t value);
static SIXELSTATUS cuckoo_table32_grow(struct cuckoo_table32 *table);

struct robinhood_slot32 {
    uint32_t key;
    uint32_t color;
    uint32_t value;
    uint16_t distance;
    uint16_t pad;
};

struct robinhood_table32 {
    struct robinhood_slot32 *slots;
    size_t capacity;
    size_t count;
    sixel_allocator_t *allocator;
};

static size_t robinhood_round_capacity(size_t hint);
static SIXELSTATUS robinhood_table32_init(struct robinhood_table32 *table,
                                         size_t expected,
                                         sixel_allocator_t *allocator);
static void robinhood_table32_fini(struct robinhood_table32 *table);
static struct robinhood_slot32 *
robinhood_table32_lookup(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color);
static SIXELSTATUS robinhood_table32_insert(struct robinhood_table32 *table,
                                           uint32_t key,
                                           uint32_t color,
                                           uint32_t value);
static SIXELSTATUS robinhood_table32_grow(struct robinhood_table32 *table);
static struct robinhood_slot32 *
robinhood_table32_place(struct robinhood_table32 *table,
                        struct robinhood_slot32 entry);

#define HOPSCOTCH_EMPTY_KEY 0xffffffffU
#define HOPSCOTCH_DEFAULT_NEIGHBORHOOD 32U
#define HOPSCOTCH_INSERT_RANGE 256U

struct hopscotch_slot32 {
    uint32_t key;
    uint32_t color;
    uint32_t value;
};

struct hopscotch_table32 {
    struct hopscotch_slot32 *slots;
    uint32_t *hopinfo;
    size_t capacity;
    size_t count;
    size_t neighborhood;
    sixel_allocator_t *allocator;
};

static SIXELSTATUS hopscotch_table32_init(struct hopscotch_table32 *table,
                                          size_t expected,
                                          sixel_allocator_t *allocator);
static void hopscotch_table32_fini(struct hopscotch_table32 *table);
static struct hopscotch_slot32 *
hopscotch_table32_lookup(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color);
static SIXELSTATUS hopscotch_table32_insert(struct hopscotch_table32 *table,
                                            uint32_t key,
                                            uint32_t color,
                                            uint32_t value);
static SIXELSTATUS hopscotch_table32_grow(struct hopscotch_table32 *table);

struct histogram_control
histogram_control_make(unsigned int depth)
{
    struct histogram_control control;

    control = histogram_control_make_for_policy(depth,
                                                histogram_lut_policy);

    return control;
}

static int
sixel_certlut_init(sixel_certlut_t *lut)
{
    int status;

    status = SIXEL_FALSE;
    if (lut == NULL) {
        goto end;
    }

    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wR = 1;
    lut->wG = 1;
    lut->wB = 1;
    lut->wR2 = 1U;
    lut->wG2 = 1U;
    lut->wB2 = 1U;
    memset(lut->wr_scale, 0, sizeof(lut->wr_scale));
    memset(lut->wg_scale, 0, sizeof(lut->wg_scale));
    memset(lut->wb_scale, 0, sizeof(lut->wb_scale));
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->palette = NULL;
    lut->ncolors = 0;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_certlut_release(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    free(lut->level0);
    free(lut->pool);
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    free(lut->kdnodes);
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

static int
sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut)
{
    int status;
    size_t count;
    int index;
    int32_t *wr_terms;
    int32_t *wg_terms;
    int32_t *wb_terms;

    status = SIXEL_FALSE;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    if (lut == NULL) {
        goto end;
    }
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    count = (size_t)lut->ncolors;
    wr_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wr_terms == NULL) {
        goto end;
    }
    wg_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wg_terms == NULL) {
        goto end;
    }
    wb_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wb_terms == NULL) {
        goto end;
    }
    for (index = 0; index < lut->ncolors; ++index) {
        wr_terms[index] = lut->wR * (int)lut->palette[index].r;
        wg_terms[index] = lut->wG * (int)lut->palette[index].g;
        wb_terms[index] = lut->wB * (int)lut->palette[index].b;
    }
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    lut->wr_palette = wr_terms;
    lut->wg_palette = wg_terms;
    lut->wb_palette = wb_terms;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    status = SIXEL_OK;

end:
    free(wr_terms);
    free(wg_terms);
    free(wb_terms);
    return status;
}

static void
sixel_quant_cell_center(int rmin, int gmin, int bmin, int size,
                        int *cr, int *cg, int *cb)
{
    int half;

    half = size / 2;
    *cr = rmin + half;
    *cg = gmin + half;
    *cb = bmin + half;
    if (size == 1) {
        *cr = rmin;
        *cg = gmin;
        *cb = bmin;
    }
}

static void
sixel_quant_weight_init(sixel_certlut_t *lut, int wR, int wG, int wB)
{
    int i;

    lut->wR = wR;
    lut->wG = wG;
    lut->wB = wB;
    lut->wR2 = (uint64_t)wR * (uint64_t)wR;
    lut->wG2 = (uint64_t)wG * (uint64_t)wG;
    lut->wB2 = (uint64_t)wB * (uint64_t)wB;
    for (i = 0; i < 256; ++i) {
        lut->wr_scale[i] = wR * i;
        lut->wg_scale[i] = wG * i;
        lut->wb_scale[i] = wB * i;
    }
}

static uint64_t
sixel_certlut_distance_precomputed(sixel_certlut_t const *lut,
                                   int index,
                                   int32_t wr_r,
                                   int32_t wg_g,
                                   int32_t wb_b)
{
    uint64_t distance;
    int64_t diff;

    diff = (int64_t)wr_r - (int64_t)lut->wr_palette[index];
    distance = (uint64_t)(diff * diff);
    diff = (int64_t)wg_g - (int64_t)lut->wg_palette[index];
    distance += (uint64_t)(diff * diff);
    diff = (int64_t)wb_b - (int64_t)lut->wb_palette[index];
    distance += (uint64_t)(diff * diff);

    return distance;
}

static void
sixel_quant_distance_pair(sixel_certlut_t const *lut, int r, int g, int b,
                          int *best_idx, int *second_idx,
                          uint64_t *best_dist, uint64_t *second_dist)
{
    int i;
    int best_candidate;
    int second_candidate;
    uint64_t best_value;
    uint64_t second_value;
    uint64_t distance;
    int rr;
    int gg;
    int bb;
    int32_t wr_r;
    int32_t wg_g;
    int32_t wb_b;

    best_candidate = (-1);
    second_candidate = (-1);
    best_value = UINT64_MAX;
    second_value = UINT64_MAX;
    rr = r;
    gg = g;
    bb = b;
    if (rr < 0) {
        rr = 0;
    } else if (rr > 255) {
        rr = 255;
    }
    if (gg < 0) {
        gg = 0;
    } else if (gg > 255) {
        gg = 255;
    }
    if (bb < 0) {
        bb = 0;
    } else if (bb > 255) {
        bb = 255;
    }
    wr_r = lut->wr_scale[rr];
    wg_g = lut->wg_scale[gg];
    wb_b = lut->wb_scale[bb];
    if (lut->kdnodes != NULL && lut->kdtree_root >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    lut->kdtree_root,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    &best_candidate,
                                    &best_value,
                                    &second_candidate,
                                    &second_value);
    } else {
        for (i = 0; i < lut->ncolors; ++i) {
            distance = sixel_certlut_distance_precomputed(lut,
                                                          i,
                                                          wr_r,
                                                          wg_g,
                                                          wb_b);
            if (distance < best_value) {
                second_value = best_value;
                second_candidate = best_candidate;
                best_value = distance;
                best_candidate = i;
            } else if (distance < second_value) {
                second_value = distance;
                second_candidate = i;
            }
        }
    }
    if (second_candidate < 0) {
        second_candidate = best_candidate;
        second_value = best_value;
    }
    *best_idx = best_candidate;
    *second_idx = second_candidate;
    *best_dist = best_value;
    *second_dist = second_value;
}

static int
sixel_quant_is_cell_safe(sixel_certlut_t const *lut, int best_idx,
                         int second_idx, int size, uint64_t best_dist,
                         uint64_t second_dist)
{
    uint64_t delta_sq;
    uint64_t rhs;
    uint64_t weight_term;
    int64_t wr_delta;
    int64_t wg_delta;
    int64_t wb_delta;

    if (best_idx < 0 || second_idx < 0) {
        return 1;
    }

    /*
     * The certification bound compares the squared distance gap against the
     * palette separation scaled by the cube diameter.  If the gap wins the
     * entire cube maps to the current best palette entry.
     */
    delta_sq = second_dist - best_dist;
    wr_delta = (int64_t)lut->wr_palette[second_idx]
        - (int64_t)lut->wr_palette[best_idx];
    wg_delta = (int64_t)lut->wg_palette[second_idx]
        - (int64_t)lut->wg_palette[best_idx];
    wb_delta = (int64_t)lut->wb_palette[second_idx]
        - (int64_t)lut->wb_palette[best_idx];
    weight_term = (uint64_t)(wr_delta * wr_delta);
    weight_term += (uint64_t)(wg_delta * wg_delta);
    weight_term += (uint64_t)(wb_delta * wb_delta);
    rhs = (uint64_t)3 * (uint64_t)size * (uint64_t)size * weight_term;

    return delta_sq * delta_sq > rhs;
}

static uint32_t
sixel_quant_pool_alloc(sixel_certlut_t *lut, int *status)
{
    uint32_t required;
    uint32_t next_capacity;
    uint32_t offset;
    uint8_t *resized;

    offset = 0U;
    if (status != NULL) {
        *status = SIXEL_FALSE;
    }
    required = lut->pool_size + (uint32_t)(8 * sizeof(uint32_t));
    if (required > lut->pool_capacity) {
        next_capacity = lut->pool_capacity;
        if (next_capacity == 0U) {
            next_capacity = (uint32_t)(8 * sizeof(uint32_t));
        }
        while (next_capacity < required) {
            if (next_capacity > UINT32_MAX / 2U) {
                return 0U;
            }
            next_capacity *= 2U;
        }
        resized = (uint8_t *)realloc(lut->pool, next_capacity);
        if (resized == NULL) {
            return 0U;
        }
        lut->pool = resized;
        lut->pool_capacity = next_capacity;
    }
    offset = lut->pool_size;
    memset(lut->pool + offset, 0, 8 * sizeof(uint32_t));
    lut->pool_size = required;
    if (status != NULL) {
        *status = SIXEL_OK;
    }

    return offset;
}

static void
sixel_quant_assign_leaf(uint32_t *cell, int palette_index)
{
    *cell = 0x80000000U | (uint32_t)(palette_index & 0xff);
}

static void
sixel_quant_assign_branch(uint32_t *cell, uint32_t offset)
{
    *cell = SIXEL_CERTLUT_BRANCH_FLAG | (offset & 0x3fffffffU);
}

static int
sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                int index, int axis)
{
    sixel_color_t const *color;

    color = &lut->palette[index];
    if (axis == 0) {
        return (int)color->r;
    }
    if (axis == 1) {
        return (int)color->g;
    }
    return (int)color->b;
}

static void
sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                           int *indices, int count, int axis)
{
    int i;
    int j;
    int key;
    int key_value;
    int current_value;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_certlut_palette_component(lut, key, axis);
        j = i - 1;
        while (j >= 0) {
            current_value = sixel_certlut_palette_component(lut,
                                                            indices[j],
                                                            axis);
            if (current_value <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
                                     int *indices,
                                     int count,
                                     int depth)
{
    int axis;
    int median;
    int node_index;

    if (count <= 0) {
        return -1;
    }

    axis = depth % 3;
    sixel_certlut_sort_indices(lut, indices, count, axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }
    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = (unsigned char)axis;
    lut->kdnodes[node_index].left =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices,
                                             median,
                                             depth + 1);
    lut->kdnodes[node_index].right =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices + median + 1,
                                             count - median - 1,
                                             depth + 1);

    return node_index;
}

static SIXELSTATUS
sixel_certlut_kdtree_build(sixel_certlut_t *lut)
{
    SIXELSTATUS status;
    int *indices;
    int i;

    status = SIXEL_FALSE;
    indices = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    lut->kdnodes = (sixel_certlut_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_certlut_node_t));
    if (lut->kdnodes == NULL) {
        goto end;
    }
    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        goto end;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_certlut_kdtree_build_recursive(lut,
                                                            indices,
                                                            lut->ncolors,
                                                            0);
    if (lut->kdtree_root < 0) {
        goto end;
    }
    status = SIXEL_OK;

end:
    free(indices);
    if (SIXEL_FAILED(status)) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        lut->kdnodes_count = 0;
        lut->kdtree_root = -1;
    }

    return status;
}

static uint64_t
sixel_certlut_axis_distance(sixel_certlut_t const *lut, int diff, int axis)
{
    uint64_t weight;
    uint64_t abs_diff;

    abs_diff = (uint64_t)(diff < 0 ? -diff : diff);
    if (axis == 0) {
        weight = lut->wR2;
    } else if (axis == 1) {
        weight = lut->wG2;
    } else {
        weight = lut->wB2;
    }

    return weight * abs_diff * abs_diff;
}

static void
sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                 int candidate,
                                 int32_t wr_r,
                                 int32_t wg_g,
                                 int32_t wb_b,
                                 int *best_idx,
                                 uint64_t *best_dist,
                                 int *second_idx,
                                 uint64_t *second_dist)
{
    uint64_t distance;

    distance = sixel_certlut_distance_precomputed(lut,
                                                  candidate,
                                                  wr_r,
                                                  wg_g,
                                                  wb_b);
    if (distance < *best_dist) {
        *second_dist = *best_dist;
        *second_idx = *best_idx;
        *best_dist = distance;
        *best_idx = candidate;
    } else if (distance < *second_dist) {
        *second_dist = distance;
        *second_idx = candidate;
    }
}

static void
sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                            int node_index,
                            int r,
                            int g,
                            int b,
                            int32_t wr_r,
                            int32_t wg_g,
                            int32_t wb_b,
                            int *best_idx,
                            uint64_t *best_dist,
                            int *second_idx,
                            uint64_t *second_dist)
{
    sixel_certlut_node_t const *node;
    int axis;
    int value;
    int diff;
    int near_child;
    int far_child;
    uint64_t axis_bound;
    int component;

    if (node_index < 0) {
        return;
    }
    node = &lut->kdnodes[node_index];
    sixel_certlut_consider_candidate(lut,
                                     node->index,
                                     wr_r,
                                     wg_g,
                                     wb_b,
                                     best_idx,
                                     best_dist,
                                     second_idx,
                                     second_dist);

    axis = (int)node->axis;
    value = sixel_certlut_palette_component(lut, node->index, axis);
    if (axis == 0) {
        component = r;
    } else if (axis == 1) {
        component = g;
    } else {
        component = b;
    }
    diff = component - value;
    if (diff <= 0) {
        near_child = node->left;
        far_child = node->right;
    } else {
        near_child = node->right;
        far_child = node->left;
    }
    if (near_child >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    near_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
    axis_bound = sixel_certlut_axis_distance(lut, diff, axis);
    if (far_child >= 0 && axis_bound <= *second_dist) {
        sixel_certlut_kdtree_search(lut,
                                    far_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
}

static uint8_t
sixel_certlut_fallback(sixel_certlut_t const *lut, int r, int g, int b)
{
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;

    best_idx = -1;
    second_idx = -1;
    best_dist = 0U;
    second_dist = 0U;
    if (lut == NULL) {
        return 0U;
    }
    /*
     * The lazy builder may fail when allocations run out.  Fall back to a
     * direct brute-force palette search so lookups still succeed even in low
     * memory conditions.
     */
    sixel_quant_distance_pair(lut,
                              r,
                              g,
                              b,
                              &best_idx,
                              &second_idx,
                              &best_dist,
                              &second_dist);
    if (best_idx < 0) {
        return 0U;
    }

    return (uint8_t)best_idx;
}

static int
sixel_certlut_build_cell(sixel_certlut_t *lut, uint32_t *cell,
                         int rmin, int gmin, int bmin, int size)
{
    SIXELSTATUS status;
    int cr;
    int cg;
    int cb;
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;
    uint32_t offset;
    int branch_status;
    uint8_t *pool_before;
    size_t pool_size_before;
    uint32_t cell_offset;
    int cell_in_pool;

    if (cell == NULL || lut == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (*cell == 0U) {
#ifdef DEBUG_CERTLUT_TRACE
        fprintf(stderr,
                "build_cell rmin=%d gmin=%d bmin=%d size=%d\n",
                rmin,
                gmin,
                bmin,
                size);
#endif
    }
    if (*cell != 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Each node represents an axis-aligned cube in RGB space.  The builder
     * certifies the dominant palette index by checking the distance gap at
     * the cell center.  When certification fails the cube is split into eight
     * octants backed by a pool block.  Children remain unbuilt until lookups
     * descend into them, keeping the workload proportional to actual queries.
     */
    status = SIXEL_FALSE;
    sixel_quant_cell_center(rmin, gmin, bmin, size, &cr, &cg, &cb);
    sixel_quant_distance_pair(lut, cr, cg, cb, &best_idx, &second_idx,
                              &best_dist, &second_dist);
    if (best_idx < 0) {
        best_idx = 0;
    }
    if (size == 1) {
        sixel_quant_assign_leaf(cell, best_idx);
#ifdef DEBUG_CERTLUT_TRACE
        fprintf(stderr,
                "  leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    if (sixel_quant_is_cell_safe(lut, best_idx, second_idx, size,
                                 best_dist, second_dist)) {
        sixel_quant_assign_leaf(cell, best_idx);
#ifdef DEBUG_CERTLUT_TRACE
        fprintf(stderr,
                "  safe leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    pool_before = lut->pool;
    pool_size_before = lut->pool_size;
    cell_in_pool = 0;
    cell_offset = 0U;
    /*
     * The pool may grow while building descendants.  Remember the caller's
     * offset so the cell pointer can be refreshed after realloc moves the
     * backing storage.
     */
    if (pool_before != NULL) {
        if ((uint8_t *)(void *)cell >= pool_before
                && (size_t)((uint8_t *)(void *)cell - pool_before)
                        < pool_size_before) {
            cell_in_pool = 1;
            cell_offset = (uint32_t)((uint8_t *)(void *)cell - pool_before);
        }
    }
    offset = sixel_quant_pool_alloc(lut, &branch_status);
    if (branch_status != SIXEL_OK) {
        goto end;
    }
    if (cell_in_pool != 0) {
        cell = (uint32_t *)(void *)(lut->pool + cell_offset);
    }
    sixel_quant_assign_branch(cell, offset);
#ifdef DEBUG_CERTLUT_TRACE
    fprintf(stderr,
            "  branch offset=%u\n",
            offset);
#endif
    status = SIXEL_OK;

end:
    return status;
}

static int
sixel_certlut_build(sixel_certlut_t *lut, sixel_color_t const *palette,
                    int ncolors, int wR, int wG, int wB)
{
    SIXELSTATUS status;
    int initialized;
    size_t level0_count;
    status = SIXEL_FALSE;
    initialized = sixel_certlut_init(lut);
    if (SIXEL_FAILED(initialized)) {
        goto end;
    }
    lut->palette = palette;
    lut->ncolors = ncolors;
    sixel_quant_weight_init(lut, wR, wG, wB);
    status = sixel_certlut_prepare_palette_terms(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_certlut_kdtree_build(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    level0_count = (size_t)64 * (size_t)64 * (size_t)64;
    lut->level0 = (uint32_t *)calloc(level0_count, sizeof(uint32_t));
    if (lut->level0 == NULL) {
        goto end;
    }
    /*
     * Level 0 cells start uninitialized.  The lookup routine materializes
     * individual subtrees on demand so we avoid evaluating the entire
     * 64x64x64 grid upfront.
     */
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_certlut_release(lut);
    }
    return status;
}

static uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t entry;
    uint32_t offset;
    uint32_t index;
    uint32_t *children;
    uint32_t *cell;
    int shift;
    int child;
    int status;
    int size;
    int rmin;
    int gmin;
    int bmin;
    int step;
    if (lut == NULL || lut->level0 == NULL) {
        return 0U;
    }
    /*
     * Cells are created lazily.  A zero entry indicates an uninitialized
     * subtree, so the builder is invoked with the cube bounds of the current
     * traversal.  Should allocation fail we fall back to a direct brute-force
     * palette search for the queried pixel.
     */
    index = ((uint32_t)(r >> 2) << 12)
          | ((uint32_t)(g >> 2) << 6)
          | (uint32_t)(b >> 2);
    cell = lut->level0 + index;
    size = 4;
    rmin = (int)(r & 0xfc);
    gmin = (int)(g & 0xfc);
    bmin = (int)(b & 0xfc);
    entry = *cell;
    if (entry == 0U) {
#ifdef DEBUG_CERTLUT_TRACE
        fprintf(stderr,
                "lookup build level0 r=%u g=%u b=%u\n",
                (unsigned int)r,
                (unsigned int)g,
                (unsigned int)b);
#endif
        status = sixel_certlut_build_cell(lut, cell, rmin, gmin, bmin, size);
        if (SIXEL_FAILED(status)) {
            return sixel_certlut_fallback(lut,
                                          (int)r,
                                          (int)g,
                                          (int)b);
        }
        entry = *cell;
    }
    shift = 1;
    while ((entry & 0x80000000U) == 0U) {
        offset = entry & 0x3fffffffU;
        children = (uint32_t *)(void *)(lut->pool + offset);
        child = (((int)(r >> shift) & 1) << 2)
              | (((int)(g >> shift) & 1) << 1)
              | ((int)(b >> shift) & 1);
#ifdef DEBUG_CERTLUT_TRACE
        fprintf(stderr,
                "descend child=%d size=%d offset=%u\n",
                child,
                size,
                offset);
#endif
        step = size / 2;
        if (step <= 0) {
            step = 1;
        }
        rmin += step * ((child >> 2) & 1);
        gmin += step * ((child >> 1) & 1);
        bmin += step * (child & 1);
        size = step;
        cell = children + (size_t)child;
        entry = *cell;
        if (entry == 0U) {
#ifdef DEBUG_CERTLUT_TRACE
            fprintf(stderr,
                    "lookup build child size=%d rmin=%d gmin=%d bmin=%d\n",
                    size,
                    rmin,
                    gmin,
                    bmin);
#endif
            status = sixel_certlut_build_cell(lut,
                                              cell,
                                              rmin,
                                              gmin,
                                              bmin,
                                              size);
            if (SIXEL_FAILED(status)) {
                return sixel_certlut_fallback(lut,
                                              (int)r,
                                              (int)g,
                                              (int)b);
            }
            children = (uint32_t *)(void *)(lut->pool + offset);
            cell = children + (size_t)child;
            entry = *cell;
        }
        if (size == 1) {
            break;
        }
        if (shift == 0) {
            break;
        }
        --shift;
    }

    return (uint8_t)(entry & 0xffU);
}

static void
sixel_certlut_free(sixel_certlut_t *lut)
{
    sixel_certlut_release(lut);
    if (lut != NULL) {
        lut->palette = NULL;
        lut->ncolors = 0;
    }
}

static size_t
robinhood_round_capacity(size_t hint)
{
    size_t capacity;

    capacity = 16U;
    if (hint < capacity) {
        return capacity;
    }

    capacity = hint - 1U;
    capacity |= capacity >> 1;
    capacity |= capacity >> 2;
    capacity |= capacity >> 4;
    capacity |= capacity >> 8;
    capacity |= capacity >> 16;
#if SIZE_MAX > UINT32_MAX
    capacity |= capacity >> 32;
#endif
    if (capacity == SIZE_MAX) {
        return SIZE_MAX;
    }
    capacity++;
    if (capacity < 16U) {
        capacity = 16U;
    }

    return capacity;
}

static SIXELSTATUS
robinhood_table32_init(struct robinhood_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator)
{
    size_t hint;
    size_t capacity;

    table->slots = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->allocator = allocator;

    if (expected < 16U) {
        expected = 16U;
    }
    if (expected > SIZE_MAX / 2U) {
        hint = SIZE_MAX / 2U;
    } else {
        hint = expected * 2U;
    }
    capacity = robinhood_round_capacity(hint);
    if (capacity == SIZE_MAX && hint != SIZE_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }

    table->slots = (struct robinhood_slot32 *)
        sixel_allocator_calloc(allocator,
                               capacity,
                               sizeof(struct robinhood_slot32));
    if (table->slots == NULL) {
        table->capacity = 0U;
        table->count = 0U;
        return SIXEL_BAD_ALLOCATION;
    }
    table->capacity = capacity;
    table->count = 0U;

    return SIXEL_OK;
}

static void
robinhood_table32_fini(struct robinhood_table32 *table)
{
    if (table->slots != NULL) {
        sixel_allocator_free(table->allocator, table->slots);
        table->slots = NULL;
    }
    table->capacity = 0U;
    table->count = 0U;
    table->allocator = NULL;
}

static struct robinhood_slot32 *
robinhood_table32_lookup(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color)
{
    size_t mask;
    size_t index;
    uint16_t distance;
    struct robinhood_slot32 *slot;

    if (table->capacity == 0U || table->slots == NULL) {
        return NULL;
    }

    mask = table->capacity - 1U;
    index = (size_t)(key & mask);
    distance = 0U;

    for (;;) {
        slot = &table->slots[index];
        if (slot->value == 0U) {
            return NULL;
        }
        if (slot->key == key && slot->color == color) {
            return slot;
        }
        if (slot->distance < distance) {
            return NULL;
        }
        index = (index + 1U) & mask;
        distance++;
    }
}

static struct robinhood_slot32 *
robinhood_table32_place(struct robinhood_table32 *table,
                        struct robinhood_slot32 entry)
{
    size_t mask;
    size_t index;
    struct robinhood_slot32 *slot;
    struct robinhood_slot32 tmp;

    mask = table->capacity - 1U;
    index = (size_t)(entry.key & mask);

    for (;;) {
        slot = &table->slots[index];
        if (slot->value == 0U) {
            *slot = entry;
            table->count++;
            return slot;
        }
        if (slot->key == entry.key && slot->color == entry.color) {
            slot->value = entry.value;
            return slot;
        }
        if (slot->distance < entry.distance) {
            tmp = *slot;
            *slot = entry;
            entry = tmp;
        }
        index = (index + 1U) & mask;
        entry.distance++;
    }
}

static SIXELSTATUS
robinhood_table32_grow(struct robinhood_table32 *table)
{
    struct robinhood_slot32 *old_slots;
    size_t old_capacity;
    size_t new_capacity;
    size_t i;

    if (table->allocator == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (table->capacity == 0U) {
        new_capacity = 16U;
    } else {
        if (table->capacity > SIZE_MAX / 2U) {
            return SIXEL_BAD_ALLOCATION;
        }
        new_capacity = table->capacity << 1U;
    }
    new_capacity = robinhood_round_capacity(new_capacity);
    if (new_capacity <= table->capacity) {
        return SIXEL_BAD_ALLOCATION;
    }

    old_slots = table->slots;
    old_capacity = table->capacity;
    table->slots = (struct robinhood_slot32 *)
        sixel_allocator_calloc(table->allocator,
                               new_capacity,
                               sizeof(struct robinhood_slot32));
    if (table->slots == NULL) {
        table->slots = old_slots;
        table->capacity = old_capacity;
        return SIXEL_BAD_ALLOCATION;
    }
    table->capacity = new_capacity;
    table->count = 0U;

    for (i = 0U; i < old_capacity; ++i) {
        struct robinhood_slot32 entry;

        if (old_slots[i].value == 0U) {
            continue;
        }
        entry.key = old_slots[i].key;
        entry.color = old_slots[i].color;
        entry.value = old_slots[i].value;
        entry.distance = 0U;
        entry.pad = 0U;  /* ensure padding bytes are initialized */
        (void)robinhood_table32_place(table, entry);
    }

    sixel_allocator_free(table->allocator, old_slots);

    return SIXEL_OK;
}

static SIXELSTATUS
robinhood_table32_insert(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value)
{
    SIXELSTATUS status;
    struct robinhood_slot32 entry;

    if (table->slots == NULL || table->capacity == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (table->count * 2U >= table->capacity) {
        status = robinhood_table32_grow(table);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    entry.key = key;
    entry.color = color;
    entry.value = value;
    entry.distance = 0U;
    entry.pad = 0U;  /* ensure padding bytes are initialized */
    (void)robinhood_table32_place(table, entry);

    return SIXEL_OK;
}

static SIXELSTATUS
hopscotch_table32_init(struct hopscotch_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator)
{
    size_t hint;
    size_t capacity;
    size_t i;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    table->slots = NULL;
    table->hopinfo = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->neighborhood = HOPSCOTCH_DEFAULT_NEIGHBORHOOD;
    table->allocator = allocator;

    if (expected < 16U) {
        expected = 16U;
    }
    if (expected > SIZE_MAX / 2U) {
        hint = SIZE_MAX / 2U;
    } else {
        hint = expected * 2U;
    }
    capacity = robinhood_round_capacity(hint);
    if (capacity == SIZE_MAX && hint != SIZE_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (capacity < table->neighborhood) {
        capacity = table->neighborhood;
    }

    table->slots = (struct hopscotch_slot32 *)
        sixel_allocator_malloc(allocator,
                               capacity * sizeof(struct hopscotch_slot32));
    if (table->slots == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->hopinfo = (uint32_t *)
        sixel_allocator_calloc(allocator,
                               capacity,
                               sizeof(uint32_t));
    if (table->hopinfo == NULL) {
        sixel_allocator_free(allocator, table->slots);
        table->slots = NULL;
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0U; i < capacity; ++i) {
        table->slots[i].key = HOPSCOTCH_EMPTY_KEY;
        table->slots[i].color = 0U;
        table->slots[i].value = 0U;
    }
    table->capacity = capacity;
    table->count = 0U;
    if (table->neighborhood > 32U) {
        table->neighborhood = 32U;
    }
    if (table->neighborhood > table->capacity) {
        table->neighborhood = table->capacity;
    }

    return SIXEL_OK;
}

static void
hopscotch_table32_fini(struct hopscotch_table32 *table)
{
    sixel_allocator_t *allocator;

    if (table == NULL) {
        return;
    }

    allocator = table->allocator;
    if (allocator != NULL) {
        if (table->slots != NULL) {
            sixel_allocator_free(allocator, table->slots);
        }
        if (table->hopinfo != NULL) {
            sixel_allocator_free(allocator, table->hopinfo);
        }
    }
    table->slots = NULL;
    table->hopinfo = NULL;
    table->capacity = 0U;
    table->count = 0U;
}

static struct hopscotch_slot32 *
hopscotch_table32_lookup(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color)
{
    size_t index;
    size_t bit;
    size_t candidate;
    uint32_t hop;
    size_t mask;
    size_t neighborhood;

    if (table == NULL || table->slots == NULL || table->hopinfo == NULL) {
        return NULL;
    }
    if (table->capacity == 0U) {
        return NULL;
    }

    mask = table->capacity - 1U;
    index = (size_t)key & mask;
    hop = table->hopinfo[index];
    neighborhood = table->neighborhood;
    for (bit = 0U; bit < neighborhood; ++bit) {
        if ((hop & (1U << bit)) == 0U) {
            continue;
        }
        candidate = (index + bit) & mask;
        if (table->slots[candidate].key == key
            && table->slots[candidate].color == color) {
            return &table->slots[candidate];
        }
    }

    return NULL;
}

static SIXELSTATUS
hopscotch_table32_grow(struct hopscotch_table32 *table)
{
    SIXELSTATUS status;
    struct hopscotch_table32 tmp;
    size_t i;
    struct hopscotch_slot32 *slot;

    tmp.slots = NULL;
    tmp.hopinfo = NULL;
    tmp.capacity = 0U;
    tmp.count = 0U;
    tmp.neighborhood = table->neighborhood;
    tmp.allocator = table->allocator;

    status = hopscotch_table32_init(&tmp,
                                    table->capacity * 2U,
                                    table->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (tmp.neighborhood > table->neighborhood) {
        tmp.neighborhood = table->neighborhood;
    }

    for (i = 0U; i < table->capacity; ++i) {
        slot = &table->slots[i];
        if (slot->key == HOPSCOTCH_EMPTY_KEY) {
            continue;
        }
        status = hopscotch_table32_insert(&tmp,
                                          slot->key,
                                          slot->color,
                                          slot->value);
        if (SIXEL_FAILED(status)) {
            hopscotch_table32_fini(&tmp);
            return status;
        }
    }

    hopscotch_table32_fini(table);

    table->slots = tmp.slots;
    table->hopinfo = tmp.hopinfo;
    table->capacity = tmp.capacity;
    table->count = tmp.count;
    table->neighborhood = tmp.neighborhood;
    table->allocator = tmp.allocator;

    return SIXEL_OK;
}

static SIXELSTATUS
hopscotch_table32_insert(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value)
{
    SIXELSTATUS status;
    struct hopscotch_slot32 *slot;
    size_t index;
    size_t mask;
    size_t distance;
    size_t attempts;
    size_t free_index;
    size_t neighborhood;
    int relocated;
    size_t offset;
    uint32_t hop;
    size_t bit;
    size_t move_index;
    struct hopscotch_slot32 tmp_slot;

    if (table == NULL || table->slots == NULL || table->hopinfo == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (table->capacity == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = hopscotch_table32_lookup(table, key, color);
    if (slot != NULL) {
        slot->value = value;
        return SIXEL_OK;
    }

    if (table->count * 2U >= table->capacity) {
        status = hopscotch_table32_grow(table);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        return hopscotch_table32_insert(table, key, color, value);
    }

    mask = table->capacity - 1U;
    neighborhood = table->neighborhood;
    index = (size_t)key & mask;
    slot = NULL;
    free_index = index;
    distance = 0U;
    for (attempts = 0U; attempts < HOPSCOTCH_INSERT_RANGE; ++attempts) {
        free_index = (index + attempts) & mask;
        slot = &table->slots[free_index];
        if (slot->key == HOPSCOTCH_EMPTY_KEY) {
            distance = attempts;
            break;
        }
    }
    if (slot == NULL || slot->key != HOPSCOTCH_EMPTY_KEY) {
        status = hopscotch_table32_grow(table);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        return hopscotch_table32_insert(table, key, color, value);
    }

    /*
     * Relocation diagram:
     *
     *   free slot <--- hop window <--- [base bucket]
     *      ^ move resident outward until distance < neighborhood
     */
    while (distance >= neighborhood) {
        relocated = 0;
        for (offset = neighborhood - 1U; offset > 0U; --offset) {
            size_t base;

            base = (free_index + table->capacity - offset) & mask;
            hop = table->hopinfo[base];
            if (hop == 0U) {
                continue;
            }
            for (bit = 0U; bit < offset; ++bit) {
                if ((hop & (1U << bit)) == 0U) {
                    continue;
                }
                move_index = (base + bit) & mask;
                tmp_slot = table->slots[move_index];
                table->slots[free_index] = tmp_slot;
                table->slots[move_index].key = HOPSCOTCH_EMPTY_KEY;
                table->slots[move_index].color = 0U;
                table->slots[move_index].value = 0U;
                table->hopinfo[base] &= (uint32_t)~(1U << bit);
                table->hopinfo[base] |= (uint32_t)(1U << offset);
                free_index = move_index;
                if (free_index >= index) {
                    distance = free_index - index;
                } else {
                    distance = free_index + table->capacity - index;
                }
                relocated = 1;
                break;
            }
            if (relocated) {
                break;
            }
        }
        if (!relocated) {
            status = hopscotch_table32_grow(table);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            return hopscotch_table32_insert(table, key, color, value);
        }
    }

    if (distance >= 32U) {
        status = hopscotch_table32_grow(table);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        return hopscotch_table32_insert(table, key, color, value);
    }

    table->slots[free_index].key = key;
    table->slots[free_index].color = color;
    table->slots[free_index].value = value;
    table->hopinfo[index] |= (uint32_t)(1U << distance);
    table->count++;

    return SIXEL_OK;
}

/*
 * The cuckoo hash backend stores entries in fixed-width buckets.
 *
 *   [bucket 0] -> key0 key1 key2 key3
 *                 val0 val1 val2 val3
 *   [bucket 1] -> ...
 *
 * Each key is compared against the 128-bit lane using SIMD instructions.
 * Two hash functions map a key to its primary and secondary buckets.  When
 * both are full, the eviction loop "kicks" an entry toward its alternate
 * bucket, as illustrated below:
 *
 *   bucket A --kick--> bucket B --kick--> bucket A ...
 *
 * A tiny stash buffers entries when the table momentarily fills up.  This
 * keeps lookups fast while letting us grow the table lazily.
 */
static size_t
cuckoo_round_buckets(size_t hint)
{
    size_t desired;
    size_t buckets;
    size_t prev;

    if (hint < CUCKOO_BUCKET_SIZE) {
        hint = CUCKOO_BUCKET_SIZE;
    }
    if (hint > SIZE_MAX / 2U) {
        hint = SIZE_MAX / 2U;
    }
    desired = (hint * 2U + CUCKOO_BUCKET_SIZE - 1U) / CUCKOO_BUCKET_SIZE;
    if (desired == 0U) {
        desired = 1U;
    }

    buckets = 1U;
    while (buckets < desired) {
        prev = buckets;
        if (buckets > SIZE_MAX / 2U) {
            buckets = prev;
            break;
        }
        buckets <<= 1U;
        if (buckets < prev) {
            buckets = prev;
            break;
        }
    }
    if (buckets == 0U) {
        buckets = 1U;
    }

    return buckets;
}

static size_t
cuckoo_hash_primary(uint32_t key, size_t mask)
{
    uint32_t mix;

    mix = key * 0x9e3779b1U;
    return (size_t)(mix & (uint32_t)mask);
}

static size_t
cuckoo_hash_secondary(uint32_t key, size_t mask)
{
    uint32_t mix;

    mix = key ^ 0x85ebca6bU;
    mix ^= mix >> 13;
    mix *= 0xc2b2ae35U;
    return (size_t)(mix & (uint32_t)mask);
}

static size_t
cuckoo_hash_alternate(uint32_t key, size_t bucket, size_t mask)
{
    size_t primary;
    size_t secondary;

    primary = cuckoo_hash_primary(key, mask);
    secondary = cuckoo_hash_secondary(key, mask);
    if (primary == bucket) {
        if (secondary == primary) {
            secondary = (secondary + 1U) & mask;
        }
        return secondary;
    }

    return primary;
}

static uint32_t *
cuckoo_bucket_find(struct cuckoo_bucket32 *bucket, uint32_t key)
{
    size_t i;

#if defined(SIXEL_USE_SSE2)
    __m128i needle;
    __m128i keys;
    __m128i cmp;
    int mask;

    needle = _mm_set1_epi32((int)key);
    keys = _mm_loadu_si128((const __m128i *)bucket->key);
    cmp = _mm_cmpeq_epi32(needle, keys);
    mask = _mm_movemask_ps(_mm_castsi128_ps(cmp));
    if ((mask & 1) != 0 && bucket->value[0] != 0U) {
        return &bucket->value[0];
    }
    if ((mask & 2) != 0 && bucket->value[1] != 0U) {
        return &bucket->value[1];
    }
    if ((mask & 4) != 0 && bucket->value[2] != 0U) {
        return &bucket->value[2];
    }
    if ((mask & 8) != 0 && bucket->value[3] != 0U) {
        return &bucket->value[3];
    }
#elif defined(SIXEL_USE_NEON)
    uint32x4_t needle;
    uint32x4_t keys;
    uint32x4_t cmp;

    needle = vdupq_n_u32(key);
    keys = vld1q_u32(bucket->key);
    cmp = vceqq_u32(needle, keys);
    if (vgetq_lane_u32(cmp, 0) != 0U && bucket->value[0] != 0U) {
        return &bucket->value[0];
    }
    if (vgetq_lane_u32(cmp, 1) != 0U && bucket->value[1] != 0U) {
        return &bucket->value[1];
    }
    if (vgetq_lane_u32(cmp, 2) != 0U && bucket->value[2] != 0U) {
        return &bucket->value[2];
    }
    if (vgetq_lane_u32(cmp, 3) != 0U && bucket->value[3] != 0U) {
        return &bucket->value[3];
    }
#else
    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (bucket->value[i] != 0U && bucket->key[i] == key) {
            return &bucket->value[i];
        }
    }
#endif

    return NULL;
}

static int
cuckoo_bucket_insert_direct(struct cuckoo_bucket32 *bucket,
                            uint32_t key,
                            uint32_t value)
{
    size_t i;

    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (bucket->value[i] == 0U) {
            bucket->key[i] = key;
            bucket->value[i] = value;
            return 1;
        }
    }

    return 0;
}

static SIXELSTATUS
cuckoo_table32_init(struct cuckoo_table32 *table,
                    size_t expected,
                    sixel_allocator_t *allocator)
{
    size_t buckets;
    size_t i;
    size_t j;

    if (table == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buckets = cuckoo_round_buckets(expected);
    if (buckets == 0U
        || buckets > SIZE_MAX / sizeof(struct cuckoo_bucket32)) {
        sixel_helper_set_additional_message(
            "unable to size cuckoo bucket array.");
        return SIXEL_BAD_ALLOCATION;
    }

    table->buckets = (struct cuckoo_bucket32 *)sixel_allocator_malloc(
        allocator, buckets * sizeof(struct cuckoo_bucket32));
    if (table->buckets == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate cuckoo buckets.");
        return SIXEL_BAD_ALLOCATION;
    }

    table->bucket_count = buckets;
    table->bucket_mask = buckets - 1U;
    table->stash_count = 0U;
    table->entry_count = 0U;
    table->allocator = allocator;
    for (i = 0U; i < buckets; ++i) {
        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            table->buckets[i].key[j] = CUCKOO_EMPTY_KEY;
            table->buckets[i].value[j] = 0U;
        }
    }
    for (i = 0U; i < CUCKOO_STASH_SIZE; ++i) {
        table->stash_key[i] = CUCKOO_EMPTY_KEY;
        table->stash_value[i] = 0U;
    }

    return SIXEL_OK;
}

static void
cuckoo_table32_clear(struct cuckoo_table32 *table)
{
    size_t i;
    size_t j;

    if (table == NULL || table->buckets == NULL) {
        return;
    }

    table->stash_count = 0U;
    table->entry_count = 0U;
    for (i = 0U; i < table->bucket_count; ++i) {
        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            table->buckets[i].key[j] = CUCKOO_EMPTY_KEY;
            table->buckets[i].value[j] = 0U;
        }
    }
    for (i = 0U; i < CUCKOO_STASH_SIZE; ++i) {
        table->stash_key[i] = CUCKOO_EMPTY_KEY;
        table->stash_value[i] = 0U;
    }
}

static void
cuckoo_table32_fini(struct cuckoo_table32 *table)
{
    if (table == NULL || table->allocator == NULL) {
        return;
    }
    if (table->buckets != NULL) {
        sixel_allocator_free(table->allocator, table->buckets);
        table->buckets = NULL;
    }
    table->bucket_count = 0U;
    table->bucket_mask = 0U;
    table->stash_count = 0U;
    table->entry_count = 0U;
}

static uint32_t *
cuckoo_table32_lookup(struct cuckoo_table32 *table, uint32_t key)
{
    size_t index;
    size_t i;
    uint32_t *slot;

    if (table == NULL || table->buckets == NULL) {
        return NULL;
    }

    index = cuckoo_hash_primary(key, table->bucket_mask);
    slot = cuckoo_bucket_find(&table->buckets[index], key);
    if (slot != NULL) {
        return slot;
    }

    index = cuckoo_hash_secondary(key, table->bucket_mask);
    slot = cuckoo_bucket_find(&table->buckets[index], key);
    if (slot != NULL) {
        return slot;
    }

    for (i = 0U; i < table->stash_count; ++i) {
        if (table->stash_value[i] != 0U && table->stash_key[i] == key) {
            return &table->stash_value[i];
        }
    }

    return NULL;
}

static SIXELSTATUS
cuckoo_table32_grow(struct cuckoo_table32 *table)
{
    struct cuckoo_table32 tmp;
    struct cuckoo_bucket32 *old_buckets;
    size_t old_count;
    size_t i;
    size_t j;
    SIXELSTATUS status;

    if (table == NULL || table->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    tmp.buckets = NULL;
    tmp.bucket_count = 0U;
    tmp.bucket_mask = 0U;
    tmp.stash_count = 0U;
    tmp.entry_count = 0U;
    tmp.allocator = table->allocator;
    for (i = 0U; i < CUCKOO_STASH_SIZE; ++i) {
        tmp.stash_key[i] = CUCKOO_EMPTY_KEY;
        tmp.stash_value[i] = 0U;
    }

    status = cuckoo_table32_init(&tmp,
                                 (table->entry_count + 1U) * 2U,
                                 table->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    old_buckets = table->buckets;
    old_count = table->bucket_count;
    for (i = 0U; i < old_count; ++i) {
        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            if (old_buckets[i].value[j] != 0U) {
                status = cuckoo_table32_insert(&tmp,
                                               old_buckets[i].key[j],
                                               old_buckets[i].value[j]);
                if (SIXEL_FAILED(status)) {
                    cuckoo_table32_fini(&tmp);
                    return status;
                }
            }
        }
    }
    for (i = 0U; i < table->stash_count; ++i) {
        if (table->stash_value[i] != 0U) {
            status = cuckoo_table32_insert(&tmp,
                                           table->stash_key[i],
                                           table->stash_value[i]);
            if (SIXEL_FAILED(status)) {
                cuckoo_table32_fini(&tmp);
                return status;
            }
        }
    }

    sixel_allocator_free(table->allocator, old_buckets);
    *table = tmp;

    return SIXEL_OK;
}

static SIXELSTATUS
cuckoo_table32_insert(struct cuckoo_table32 *table,
                      uint32_t key,
                      uint32_t value)
{
    uint32_t *slot;
    uint32_t cur_key;
    uint32_t cur_value;
    uint32_t victim_key;
    uint32_t victim_value;
    size_t bucket_index;
    size_t kicks;
    size_t victim_slot;
    struct cuckoo_bucket32 *bucket;
    SIXELSTATUS status;

    if (table == NULL || table->buckets == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = cuckoo_table32_lookup(table, key);
    if (slot != NULL) {
        *slot = value;
        return SIXEL_OK;
    }

    cur_key = key;
    cur_value = value;
    bucket_index = cuckoo_hash_primary(cur_key, table->bucket_mask);
    for (kicks = 0U; kicks < CUCKOO_MAX_KICKS; ++kicks) {
        bucket = &table->buckets[bucket_index];
        if (cuckoo_bucket_insert_direct(bucket, cur_key, cur_value)) {
            table->entry_count++;
            return SIXEL_OK;
        }
        victim_slot = (size_t)((cur_key + kicks) &
                               (CUCKOO_BUCKET_SIZE - 1U));
        victim_key = bucket->key[victim_slot];
        victim_value = bucket->value[victim_slot];
        bucket->key[victim_slot] = cur_key;
        bucket->value[victim_slot] = cur_value;
        cur_key = victim_key;
        cur_value = victim_value;
        bucket_index = cuckoo_hash_alternate(cur_key,
                                             bucket_index,
                                             table->bucket_mask);
    }

    if (table->stash_count < CUCKOO_STASH_SIZE) {
        table->stash_key[table->stash_count] = cur_key;
        table->stash_value[table->stash_count] = cur_value;
        table->stash_count++;
        table->entry_count++;
        return SIXEL_OK;
    }

    status = cuckoo_table32_grow(table);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return cuckoo_table32_insert(table, cur_key, cur_value);
}

static SIXELSTATUS
computeHistogram_robinhood(unsigned char const *data,
                           unsigned int length,
                           unsigned long depth,
                           unsigned int step,
                           unsigned int max_sample,
                           tupletable2 * const colorfreqtableP,
                           struct histogram_control const *control,
                           int use_reversible,
                           sixel_allocator_t *allocator);

static SIXELSTATUS
computeHistogram_hopscotch(unsigned char const *data,
                           unsigned int length,
                           unsigned long depth,
                           unsigned int step,
                           unsigned int max_sample,
                           tupletable2 * const colorfreqtableP,
                           struct histogram_control const *control,
                           int use_reversible,
                           sixel_allocator_t *allocator);

static SIXELSTATUS
computeHistogram(unsigned char const    /* in */  *data,
                 unsigned int           /* in */  length,
                 unsigned long const    /* in */  depth,
                 tupletable2 * const    /* out */ colorfreqtableP,
                 int const              /* in */  qualityMode,
                 int const              /* in */  use_reversible,
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
    unsigned int depth_u;
    unsigned char reversible_pixel[4];

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

    depth_u = (unsigned int)depth;
    control = histogram_control_make(depth_u);
    if (use_reversible) {
        control.reversible_rounding = 1;
    }
    if (histogram_lut_policy == SIXEL_LUT_POLICY_ROBINHOOD
        || histogram_lut_policy == SIXEL_LUT_POLICY_HOPSCOTCH) {
        if (histogram_lut_policy == SIXEL_LUT_POLICY_ROBINHOOD) {
            status = computeHistogram_robinhood(data,
                                                length,
                                                depth,
                                                step,
                                                max_sample,
                                                colorfreqtableP,
                                                &control,
                                                use_reversible,
                                                allocator);
        } else {
            status = computeHistogram_hopscotch(data,
                                                length,
                                                depth,
                                                step,
                                                max_sample,
                                                colorfreqtableP,
                                                &control,
                                                use_reversible,
                                                allocator);
        }
        goto end;
    }

    hist_size = histogram_dense_size((unsigned int)depth, &control);
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
        if (use_reversible) {
            sixel_quant_reversible_pixel(data + i,
                                         depth_u,
                                         reversible_pixel);
            bucket_index = histogram_pack_color(reversible_pixel,
                                                depth_u,
                                                &control);
        } else {
            bucket_index = histogram_pack_color(data + i,
                                                depth_u,
                                                &control);
        }
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
                if (use_reversible) {
                    reconstructed =
                        (unsigned int)sixel_quant_reversible_value(
                            reconstructed);
                }
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

static SIXELSTATUS
computeHistogram_robinhood(unsigned char const *data,
                           unsigned int length,
                           unsigned long depth,
                           unsigned int step,
                           unsigned int max_sample,
                           tupletable2 * const colorfreqtableP,
                           struct histogram_control const *control,
                           int use_reversible,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct robinhood_table32 table;
    size_t expected;
    size_t cap_limit;
    size_t index;
    unsigned int depth_u;
    unsigned int i;
    unsigned int n;
    uint32_t bucket_color;
    uint32_t bucket_hash;
    uint32_t entry_color;
    struct robinhood_slot32 *slot;
    unsigned int component;
    unsigned int reconstructed;
    unsigned char reversible_pixel[4];

    /*
     * The ASCII sketch below shows how the sparse table stores samples:
     *
     *   [hash]->(key,value,distance)  Robin Hood probing keeps dense tails.
     */
    table.slots = NULL;
    table.capacity = 0U;
    table.count = 0U;
    table.allocator = allocator;
    cap_limit = (size_t)1U << 20;
    expected = max_sample;
    if (expected < 256U) {
        expected = 256U;
    }
    if (expected > cap_limit) {
        expected = cap_limit;
    }

    status = robinhood_table32_init(&table, expected, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "unable to allocate robinhood histogram.");
        goto end;
    }

    depth_u = (unsigned int)depth;
    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            sixel_quant_reversible_pixel(data + i, depth_u,
                                         reversible_pixel);
            bucket_color = histogram_pack_color(reversible_pixel,
                                                depth_u, control);
        } else {
            bucket_color = histogram_pack_color(data + i,
                                                depth_u, control);
        }
        bucket_hash = histogram_hash_mix(bucket_color);
        /*
         * Hash probing uses the mixed key while the slot stores the
         * original quantized RGB value:
         *
         *   hash --> [slot]
         *             |color|count|
         */
        slot = robinhood_table32_lookup(&table,
                                        bucket_hash,
                                        bucket_color);
        if (slot == NULL) {
            status = robinhood_table32_insert(&table,
                                              bucket_hash,
                                              bucket_color,
                                              1U);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "unable to grow robinhood histogram.");
                goto end;
            }
        } else if (slot->value < UINT32_MAX) {
            slot->value++;
        }
    }

    if (table.count > UINT_MAX) {
        sixel_helper_set_additional_message(
            "too many unique colors for histogram.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    colorfreqtableP->size = (unsigned int)table.count;
    status = alloctupletable(&colorfreqtableP->table,
                             depth_u,
                             (unsigned int)table.count,
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    index = 0U;
    /*
     * Stream slots in the hash traversal order to avoid qsort overhead.
     * This favors throughput over identical palette ordering.
     */
    for (i = 0U; i < table.capacity; ++i) {
        slot = &table.slots[i];
        if (slot->value == 0U) {
            continue;
        }
        if (index >= colorfreqtableP->size) {
            break;
        }
        entry_color = slot->color;
        colorfreqtableP->table[index]->value = slot->value;
        for (n = 0U; n < depth_u; ++n) {
            component = (unsigned int)
                ((entry_color >> (n * control->channel_bits))
                 & control->channel_mask);
            reconstructed = histogram_reconstruct(component, control);
            if (use_reversible) {
                reconstructed =
                    (unsigned int)sixel_quant_reversible_value(
                        reconstructed);
            }
            colorfreqtableP->table[index]->tuple[depth_u - 1U - n]
                = (sample)reconstructed;
        }
        index++;
    }

    status = SIXEL_OK;

end:
    robinhood_table32_fini(&table);

    return status;
}

static SIXELSTATUS
computeHistogram_hopscotch(unsigned char const *data,
                           unsigned int length,
                           unsigned long depth,
                           unsigned int step,
                           unsigned int max_sample,
                           tupletable2 * const colorfreqtableP,
                           struct histogram_control const *control,
                           int use_reversible,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct hopscotch_table32 table;
    size_t expected;
    size_t cap_limit;
    size_t index;
    unsigned int depth_u;
    unsigned int i;
    unsigned int n;
    uint32_t bucket_color;
    uint32_t bucket_hash;
    uint32_t entry_color;
    struct hopscotch_slot32 *slot;
    unsigned int component;
    unsigned int reconstructed;
    unsigned char reversible_pixel[4];

    /*
     * Hopscotch hashing stores the local neighbourhood using the map below:
     *
     *   [home] hopinfo bits ---> |slot+0|slot+1|slot+2| ...
     *                              ^ entries hop within this window.
     */
    table.slots = NULL;
    table.hopinfo = NULL;
    table.capacity = 0U;
    table.count = 0U;
    table.neighborhood = HOPSCOTCH_DEFAULT_NEIGHBORHOOD;
    table.allocator = allocator;
    cap_limit = (size_t)1U << 20;
    expected = max_sample;
    if (expected < 256U) {
        expected = 256U;
    }
    if (expected > cap_limit) {
        expected = cap_limit;
    }

    status = hopscotch_table32_init(&table, expected, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "unable to allocate hopscotch histogram.");
        goto end;
    }

    depth_u = (unsigned int)depth;
    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            sixel_quant_reversible_pixel(data + i, depth_u,
                                         reversible_pixel);
            bucket_color = histogram_pack_color(reversible_pixel,
                                                depth_u, control);
        } else {
            bucket_color = histogram_pack_color(data + i,
                                                depth_u, control);
        }
        bucket_hash = histogram_hash_mix(bucket_color);
        /*
         * Hopscotch buckets mirror the robinhood layout, keeping the
         * quantized color next to the count so we never derive it from
         * the scrambled hash key.
         */
        slot = hopscotch_table32_lookup(&table,
                                        bucket_hash,
                                        bucket_color);
        if (slot == NULL) {
            status = hopscotch_table32_insert(&table,
                                              bucket_hash,
                                              bucket_color,
                                              1U);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "unable to grow hopscotch histogram.");
                goto end;
            }
        } else if (slot->value < UINT32_MAX) {
            slot->value++;
        }
    }

    if (table.count > UINT_MAX) {
        sixel_helper_set_additional_message(
            "too many unique colors for histogram.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    colorfreqtableP->size = (unsigned int)table.count;
    status = alloctupletable(&colorfreqtableP->table,
                             depth_u,
                             (unsigned int)table.count,
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    index = 0U;
    /*
     * Stream slots in the hash traversal order to avoid qsort overhead.
     * This favors throughput over identical palette ordering.
     */
    for (i = 0U; i < table.capacity; ++i) {
        slot = &table.slots[i];
        if (slot->key == HOPSCOTCH_EMPTY_KEY || slot->value == 0U) {
            continue;
        }
        if (index >= colorfreqtableP->size) {
            break;
        }
        entry_color = slot->color;
        colorfreqtableP->table[index]->value = slot->value;
        for (n = 0U; n < depth_u; ++n) {
            component = (unsigned int)
                ((entry_color >> (n * control->channel_bits))
                 & control->channel_mask);
            reconstructed = histogram_reconstruct(component, control);
            if (use_reversible) {
                reconstructed =
                    (unsigned int)sixel_quant_reversible_value(
                        reconstructed);
            }
            colorfreqtableP->table[index]->tuple[depth_u - 1U - n]
                = (sample)reconstructed;
        }
        index++;
    }

    status = SIXEL_OK;

end:
    hopscotch_table32_fini(&table);

    return status;
}

SIXELSTATUS
sixel_quant_cache_prepare(unsigned short **cachetable,
                          size_t *cachetable_size,
                          int lut_policy,
                          int reqcolor,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    struct histogram_control control;
    struct cuckoo_table32 *table;
    size_t expected;
    size_t buckets;
    size_t cap_limit;
    int normalized;

    if (cachetable == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * The cache pointer always references the same ladder:
     *
     *   cache -> cuckoo_table32 -> buckets
     *                           -> stash
     */
    normalized = lut_policy;
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = histogram_lut_policy;
    }
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    control = histogram_control_make_for_policy(3U, normalized);
    if (control.channel_shift == 0U) {
        expected = (size_t)reqcolor * 64U;
        cap_limit = (size_t)1U << 20;
        if (expected < 512U) {
            expected = 512U;
        }
        if (expected > cap_limit) {
            expected = cap_limit;
        }
    } else {
        expected = histogram_dense_size(3U, &control);
        if (expected == 0U) {
            expected = CUCKOO_BUCKET_SIZE;
        }
    }

    table = (struct cuckoo_table32 *)*cachetable;
    if (table != NULL) {
        buckets = cuckoo_round_buckets(expected);
        if (table->bucket_count < buckets) {
            cuckoo_table32_fini(table);
            sixel_allocator_free(allocator, table);
            table = NULL;
            *cachetable = NULL;
        } else {
            cuckoo_table32_clear(table);
        }
    }
    if (table == NULL) {
        table = (struct cuckoo_table32 *)sixel_allocator_malloc(
            allocator, sizeof(struct cuckoo_table32));
        if (table == NULL) {
            sixel_helper_set_additional_message(
                "unable to allocate cuckoo cache state.");
            return SIXEL_BAD_ALLOCATION;
        }
        memset(table, 0, sizeof(struct cuckoo_table32));
        status = cuckoo_table32_init(table, expected, allocator);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, table);
            sixel_helper_set_additional_message(
                "unable to initialize cuckoo cache.");
            return status;
        }
        *cachetable = (unsigned short *)table;
    }

    if (cachetable_size != NULL) {
        *cachetable_size = table->bucket_count * CUCKOO_BUCKET_SIZE;
    }

    return SIXEL_OK;
}

void
sixel_quant_cache_clear(unsigned short *cachetable,
                        int lut_policy)
{
    struct cuckoo_table32 *table;

    (void)lut_policy;
    if (cachetable == NULL) {
        return;
    }

    table = (struct cuckoo_table32 *)cachetable;
    cuckoo_table32_clear(table);
}

void
sixel_quant_cache_release(unsigned short *cachetable,
                          int lut_policy,
                          sixel_allocator_t *allocator)
{
    struct cuckoo_table32 *table;

    (void)lut_policy;
    if (cachetable == NULL || allocator == NULL) {
        return;
    }

    table = (struct cuckoo_table32 *)cachetable;
    cuckoo_table32_fini(table);
    sixel_allocator_free(allocator, table);
}


int
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
                              &colorfreqtable, qualityMode,
                              use_reversible, allocator);
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
            if (use_reversible) {
                sixel_quant_reversible_tuple(colormapP->table[i]->tuple,
                                             depth);
            }
        }
    } else {
        quant_trace(stderr, "choosing %d colors...\n", reqColors);
        status = mediancut(colorfreqtable, depth, reqColors,
                           methodForLargest, methodForRep,
                           use_reversible, final_merge_mode,
                           colormapP, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        quant_trace(stderr, "%d colors are choosed.\n", colorfreqtable.size);
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
 * Shared fast lookup flow:
 *
 *   pixel --> quantize --> cuckoo cache --> palette index
 */
static int
lookup_fast_common(unsigned char const *pixel,
                   unsigned char const *palette,
                   int reqcolor,
                   unsigned short *cachetable,
                   int complexion,
                   struct histogram_control control)
{
    int result;
    unsigned int hash;
    int diff;
    int i;
    int distant;
    struct cuckoo_table32 *table;
    uint32_t *slot;
    SIXELSTATUS status;
    unsigned char const *entry;
    unsigned char const *end;
    int pixel0;
    int pixel1;
    int pixel2;
    int delta;

    result = (-1);
    diff = INT_MAX;
    hash = computeHash(pixel, 3U, &control);

    table = (struct cuckoo_table32 *)cachetable;
    if (table != NULL) {
        slot = cuckoo_table32_lookup(table, hash);
        if (slot != NULL && *slot != 0U) {
            return (int)(*slot - 1U);
        }
    }

    entry = palette;
    end = palette + (size_t)reqcolor * 3;
    pixel0 = (int)pixel[0];
    pixel1 = (int)pixel[1];
    pixel2 = (int)pixel[2];
    /*
     * Palette traversal as RGB triplets keeps the stride linear:
     *
     *   i -> [R][G][B]
     *        |  |  |
     *        `--+--'
     *           v
     *         entry
     */
    for (i = 0; entry < end; ++i, entry += 3) {
        delta = pixel0 - (int)entry[0];
        distant = delta * delta * complexion;
        delta = pixel1 - (int)entry[1];
        distant += delta * delta;
        delta = pixel2 - (int)entry[2];
        distant += delta * delta;
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    if (table != NULL && result >= 0) {
        status = cuckoo_table32_insert(table,
                                       hash,
                                       (uint32_t)(result + 1));
        if (SIXEL_FAILED(status)) {
            /* ignore cache update failure */
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
    struct histogram_control control;

    (void)depth;

    control = histogram_control_make(3U);

    return lookup_fast_common(pixel,
                              palette,
                              reqcolor,
                              cachetable,
                              complexion,
                              control);
}

static int
lookup_fast_robinhood(unsigned char const * const pixel,
                      int const depth,
                      unsigned char const * const palette,
                      int const reqcolor,
                      unsigned short * const cachetable,
                      int const complexion)
{
    struct histogram_control control;

    (void)depth;

    control = histogram_control_make_for_policy(3U,
                                                SIXEL_LUT_POLICY_ROBINHOOD);

    return lookup_fast_common(pixel,
                              palette,
                              reqcolor,
                              cachetable,
                              complexion,
                              control);
}

static int
lookup_fast_hopscotch(unsigned char const * const pixel,
                      int const depth,
                      unsigned char const * const palette,
                      int const reqcolor,
                      unsigned short * const cachetable,
                      int const complexion)
{
    struct histogram_control control;

    (void)depth;

    control = histogram_control_make_for_policy(3U,
                                                SIXEL_LUT_POLICY_HOPSCOTCH);

    return lookup_fast_common(pixel,
                              palette,
                              reqcolor,
                              cachetable,
                              complexion,
                              control);
}

static int
lookup_fast_certlut(unsigned char const * const pixel,
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

    if (certlut_context == NULL) {
        return 0;
    }

    return (int)sixel_certlut_lookup(certlut_context,
                                     pixel[0],
                                     pixel[1],
                                     pixel[2]);
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


SIXELSTATUS
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
        quant_trace(stderr, "overshoot: %d\n", overshoot);
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
    int                    /* in */  force_palette,
    int                    /* in */  use_reversible,
    int                    /* in */  quantize_model,
    int                    /* in */  final_merge_mode,
    sixel_allocator_t      /* in */  *allocator)
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

    palette->requested_colors = reqcolors;
    palette->method_for_largest = methodForLargest;
    palette->method_for_rep = methodForRep;
    palette->quality_mode = qualityMode;
    palette->force_palette = force_palette;
    palette->use_reversible = use_reversible;
    palette->quantize_model = quantize_model;
    palette->final_merge_mode = final_merge_mode;
    palette->lut_policy = histogram_lut_policy;

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
            "sixel_quant_make_palette: allocation failed.");
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
    int allocated_cache;
    int use_cache;
    int cache_policy;
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
    sixel_certlut_t certlut;
    int certlut_ready;
    int wR;
    int wG;
    int wB;

    certlut_ready = 0;
    memset(&certlut, 0, sizeof(certlut));

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
                  && methodForDiffuse == SIXEL_DIFFUSE_LSO2);
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

    if (f_lookup == lookup_fast) {
        if (histogram_lut_policy == SIXEL_LUT_POLICY_ROBINHOOD) {
            f_lookup = lookup_fast_robinhood;
        } else if (histogram_lut_policy == SIXEL_LUT_POLICY_HOPSCOTCH) {
            f_lookup = lookup_fast_hopscotch;
        } else if (histogram_lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
            f_lookup = lookup_fast_certlut;
        }
    }

    indextable = cachetable;
    allocated_cache = 0;
    cache_policy = SIXEL_LUT_POLICY_AUTO;
    use_cache = 0;
    if (f_lookup == lookup_fast) {
        cache_policy = histogram_lut_policy;
        use_cache = 1;
    } else if (f_lookup == lookup_fast_robinhood) {
        cache_policy = SIXEL_LUT_POLICY_ROBINHOOD;
        use_cache = 1;
    } else if (f_lookup == lookup_fast_hopscotch) {
        cache_policy = SIXEL_LUT_POLICY_HOPSCOTCH;
        use_cache = 1;
    }
    if (cache_policy == SIXEL_LUT_POLICY_AUTO) {
        cache_policy = SIXEL_LUT_POLICY_6BIT;
    }
    if (use_cache) {
        if (cachetable == NULL) {
            status = sixel_quant_cache_prepare(&indextable,
                                               &cache_size,
                                               cache_policy,
                                               reqcolor,
                                               allocator);
            if (SIXEL_FAILED(status)) {
                quant_trace(stderr,
                            "Unable to allocate lookup cache.\n");
                goto end;
            }
            allocated_cache = 1;
        } else {
            sixel_quant_cache_clear(indextable, cache_policy);
        }
    }

    if (f_lookup == lookup_fast_certlut) {
        if (depth != 3) {
            status = SIXEL_BAD_ARGUMENT;
            sixel_helper_set_additional_message(
                "sixel_quant_apply_palette: "
                "certlut requires RGB pixels.");
            goto end;
        }
        if (quant_method_for_largest == SIXEL_LARGE_LUM) {
            wR = complexion * 299;
            wG = 587;
            wB = 114;
        } else {
            wR = complexion;
            wG = 1;
            wB = 1;
        }
        status = sixel_certlut_build(&certlut,
                                     (sixel_color_t const *)palette,
                                     reqcolor,
                                     wR,
                                     wG,
                                     wB);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        certlut_context = &certlut;
        certlut_ready = 1;
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

    if (certlut_ready) {
        certlut_context = NULL;
        sixel_certlut_free(&certlut);
        certlut_ready = 0;
    }

    if (allocated_cache) {
        sixel_quant_cache_release(indextable,
                                  cache_policy,
                                  allocator);
    }

    status = SIXEL_OK;

end:
    if (certlut_ready) {
        certlut_context = NULL;
        sixel_certlut_free(&certlut);
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
