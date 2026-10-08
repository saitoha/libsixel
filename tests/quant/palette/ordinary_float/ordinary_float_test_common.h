/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ORDINARY_FLOAT_TEST_COMMON_H
#define SIXEL_ORDINARY_FLOAT_TEST_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/palette-common-merge.h"
#include "src/palette-common-snap.h"
#include "src/palette-kcenter.h"
#include "src/palette-kmedoids.h"
#include "src/palette-kmeans.h"
int ordinary_float_sources(int model, int bin, int prefer);
int ordinary_float_reduce(int model);
int ordinary_float_ward(int model);
int ordinary_float_snap(int model);
int ordinary_float_failure(int model, int nonfinite, int reuse);
int ordinary_float_byte(int model);
int ordinary_kmeans_mean(int polish);
int ordinary_kmeans_empty(void);
int ordinary_kmeans_feedback(int snap, int interval);
int ordinary_kmeans_unique(void);
#endif
