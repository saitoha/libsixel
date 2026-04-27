/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMPSD_INTERNAL_H
#define LIBSIXEL_FROMPSD_INTERNAL_H

#include "frompsd.h"

/*
 * pcc warns when internal identifiers exceed the historical external-name
 * limit. Keep descriptive source names by composing identifiers from a
 * prefix and a short suffix; pcc gets a short prefix only.
 */
#define SIXEL_FROMPSD_CAT2_I(a, b) a##b
#define SIXEL_FROMPSD_CAT2(a, b) SIXEL_FROMPSD_CAT2_I(a, b)
#define SIXEL_PSD_FN(prefix, suffix) SIXEL_FROMPSD_CAT2(prefix, suffix)

#if defined(__PCC__)
# define SIXEL_PSD_PREF_TYSH_FILLFLAG_ENGINEDATA pcc_psd_tysh_ff_eng
# define SIXEL_PSD_PREF_TYSH_FILLCOLOR_ENGINEDATA pcc_psd_tysh_fc_eng
# define SIXEL_PSD_PREF_TYSH_OPACITY_ENGINEDATA pcc_psd_tysh_op_eng
# define SIXEL_PSD_PREF_TYSH_STROKEFLAG_ENGINEDATA pcc_psd_tysh_sf_eng
# define SIXEL_PSD_PREF_TYSH_STROKECOLOR_ENGINEDATA pcc_psd_tysh_sc_eng
# define SIXEL_PSD_PREF_TYSH_STYLERUN_STYLESHEETSET pcc_psd_tysh_style_set
# define SIXEL_PSD_PREF_DEC_MISS_COMP_CMYK pcc_psd_dec_miss_comp_c
# define SIXEL_PSD_PREF_DEC_MISS_COMP_LAB pcc_psd_dec_miss_comp_l
#else
# define SIXEL_PSD_PREF_TYSH_FILLFLAG_ENGINEDATA \
    sixel_builtin_psd_parse_tysh_fillflag_enginedata
# define SIXEL_PSD_PREF_TYSH_FILLCOLOR_ENGINEDATA \
    sixel_builtin_psd_parse_tysh_fillcolor_enginedata
# define SIXEL_PSD_PREF_TYSH_OPACITY_ENGINEDATA \
    sixel_builtin_psd_parse_tysh_opacity_enginedata
# define SIXEL_PSD_PREF_TYSH_STROKEFLAG_ENGINEDATA \
    sixel_builtin_psd_parse_tysh_strokeflag_enginedata
# define SIXEL_PSD_PREF_TYSH_STROKECOLOR_ENGINEDATA \
    sixel_builtin_psd_parse_tysh_strokecolor_enginedata
# define SIXEL_PSD_PREF_TYSH_STYLERUN_STYLESHEETSET \
    sixel_builtin_psd_parse_tysh_stylerun_stylesheetset
# define SIXEL_PSD_PREF_DEC_MISS_COMP_CMYK \
    sixel_builtin_decode_psd_single_layer_missing_composite_cmyk
# define SIXEL_PSD_PREF_DEC_MISS_COMP_LAB \
    sixel_builtin_decode_psd_single_layer_missing_composite_lab
#endif

#define SIXEL_FROMPSD_MAX_CHANNELS 56u
#define SIXEL_FROMPSD_MAX_DIMENSION 300000u
#define SIXEL_BUILTIN_PSD_STROKE_APPLY_NONE 0
#define SIXEL_BUILTIN_PSD_STROKE_APPLY_VECTOR_ONLY 1
#define SIXEL_BUILTIN_PSD_STROKE_APPLY_EFFECT_ONLY 2
#define SIXEL_BUILTIN_PSD_STROKE_APPLY_DUAL 3
#define SIXEL_BUILTIN_PSD_DUAL_OVL_DEFAULT 0
#define SIXEL_BUILTIN_PSD_DUAL_OVL_FXPRI_INSIDE 1

void
sixel_builtin_psd_trace_message(char const *topic,
                                char const *message);

void
sixel_builtin_psd_trace_reset(void);

#endif /* LIBSIXEL_FROMPSD_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
