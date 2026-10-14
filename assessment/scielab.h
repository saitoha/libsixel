/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ASSESSMENT_SCIELAB_H
#define SIXEL_ASSESSMENT_SCIELAB_H

#include <sixel.h>

#define SIXEL_QA_SCIELAB_PPD_DEFAULT 60.0
#define SIXEL_QA_SCIELAB_PPD_MIN 1.0
#define SIXEL_QA_SCIELAB_PPD_MAX 1024.0

/* Interleaved physical XYZ D50, white Y=1. Samples stay unclipped.
 * Return pixel-weighted mean and maximum spatial CIE76 difference.
 * A non-NULL map receives width * height differences, in raster order.
 * Outputs are written only on success. Inputs and outputs must not alias. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_qa_scielab(double const *reference, double const *output,
                            int width, int height, double ppd,
                            double *mean, double *maximum, double *map);

#endif
