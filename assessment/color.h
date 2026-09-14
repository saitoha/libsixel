/* SPDX-License-Identifier: MIT */
/* Measurement primitives: physical XYZ D50 and conventional CIELAB units. */
#ifndef LIBSIXEL_QA_COLOR_H
#define LIBSIXEL_QA_COLOR_H
#include "cms.h"
#include "colorspace.h"

SIXEL_INTERNAL_API int
sixel_qa_xyz(sixel_color_def_t const *srgb, int coordinate,
             double const sample[3], double xyz[3]);
SIXEL_INTERNAL_API void
sixel_qa_lab(double const xyz[3], double lab[3]);
SIXEL_INTERNAL_API double
sixel_qa_de00(double const lab1[3], double const lab2[3]);
/* Fixed top-left tiles include partial edges; mean is weighted by area. */
SIXEL_INTERNAL_API void
sixel_qa_regions(double const *a, double const *b, int width, int height,
                 int window, double *mean, double *maximum, size_t *count);
#endif
