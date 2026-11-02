/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ASSESSMENT_SCIELAB_H
#define SIXEL_ASSESSMENT_SCIELAB_H

#include <sixel.h>

#define SIXEL_QA_PPD_DEFAULT 60.0
#define SIXEL_QA_PPD_MIN 1.0
#define SIXEL_QA_PPD_MAX 1024.0

/* Interleaved physical XYZ D50, white Y=1. Samples stay unclipped.
 * Return spatial CIE76 and/or CIEDE2000 (kL=kC=kH=1) mean and maximum.
 * Each summary pair must be both NULL or both non-NULL; at least one pair
 * is required. The expensive filtering is shared by both differences.
 * A non-NULL map receives width * height CIE76 differences in raster order
 * and requires the CIE76 summary pair. It never changes units implicitly.
 * Outputs are written only on success. Inputs and outputs must not alias. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_qa_scielab(double const *reference, double const *output,
                            int width, int height, double ppd,
                            double *mean, double *maximum, double *map,
                            double *mean00, double *maximum00);

#endif
