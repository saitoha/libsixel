/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_SNAP_HOOK_TEST_COMMON_H
#define SIXEL_SNAP_HOOK_TEST_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette-common-snap.h"
void snap_hook_settings(int policy, int timing, double rate);
int snap_hook_targets(int policy, int final);
int snap_hook_quantizer(int timing);
#endif
