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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "compat_stub.h"
#include "frompsd-internal.h"
#include "loader-common.h"

#if defined(_MSC_VER)
# define SIXEL_PSD_TRACE_TLS __declspec(thread)
# define SIXEL_PSD_TRACE_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_PSD_TRACE_TLS _Thread_local
# define SIXEL_PSD_TRACE_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_PSD_TRACE_TLS __thread
# define SIXEL_PSD_TRACE_TLS_AVAILABLE 1
#else
# define SIXEL_PSD_TRACE_TLS
# define SIXEL_PSD_TRACE_TLS_AVAILABLE 0
#endif

#define SIXEL_PSD_TRACE_SEEN_MAX 256u
#define SIXEL_PSD_TRACE_MESSAGE_MAX 384u
#define SIXEL_PSD_TRACE_CODE_MAX 64u

static SIXEL_PSD_TRACE_TLS uint64_t
sixel_builtin_psd_trace_seen_hashes[SIXEL_PSD_TRACE_SEEN_MAX];
static SIXEL_PSD_TRACE_TLS unsigned int
sixel_builtin_psd_trace_seen_count;
static SIXEL_PSD_TRACE_TLS char
sixel_builtin_psd_trace_messages[SIXEL_PSD_TRACE_SEEN_MAX]
                                [SIXEL_PSD_TRACE_MESSAGE_MAX];
static SIXEL_PSD_TRACE_TLS unsigned int
sixel_builtin_psd_trace_message_count;
static SIXEL_PSD_TRACE_TLS char const *
sixel_builtin_psd_trace_codes[SIXEL_PSD_TRACE_CODE_MAX];
static SIXEL_PSD_TRACE_TLS unsigned int
sixel_builtin_psd_trace_code_count;

static int
sixel_builtin_psd_trace_header_only_enabled(void)
{
    char const *value;

    value = sixel_compat_getenv("SIXEL_PSD_TRACE_HEADER_ONLY");
    if (value == NULL) {
        return 0;
    }
    if (value[0] == '1' && value[1] == '\0') {
        return 1;
    }

    return 0;
}

void
sixel_builtin_psd_trace_reset(void)
{
    sixel_builtin_psd_trace_seen_count = 0u;
    sixel_builtin_psd_trace_message_count = 0u;
    sixel_builtin_psd_trace_code_count = 0u;
}

static int
sixel_builtin_psd_trace_seen(char const *message)
{
    uint64_t hash;
    unsigned int i;

    hash = 0u;
    i = 0u;
    if (message == NULL) {
        return 0;
    }
    hash = 1469598103934665603ull;
    for (; message[i] != '\0'; ++i) {
        hash ^= (uint64_t)(unsigned char)message[i];
        hash *= 1099511628211ull;
    }
    i = 0u;
    for (i = 0u; i < sixel_builtin_psd_trace_seen_count; ++i) {
        if (sixel_builtin_psd_trace_seen_hashes[i] == hash) {
            return 1;
        }
    }
    if (sixel_builtin_psd_trace_seen_count < SIXEL_PSD_TRACE_SEEN_MAX) {
        sixel_builtin_psd_trace_seen_hashes[
            sixel_builtin_psd_trace_seen_count] = hash;
        ++sixel_builtin_psd_trace_seen_count;
    }
    return 0;
}

static void
sixel_builtin_psd_trace_add_code(char const *code)
{
    unsigned int i;

    i = 0u;
    if (code == NULL || code[0] == '\0') {
        return;
    }
    for (i = 0u; i < sixel_builtin_psd_trace_code_count; ++i) {
        if (strcmp(sixel_builtin_psd_trace_codes[i], code) == 0) {
            return;
        }
    }
    if (sixel_builtin_psd_trace_code_count >= SIXEL_PSD_TRACE_CODE_MAX) {
        return;
    }
    sixel_builtin_psd_trace_codes[sixel_builtin_psd_trace_code_count] = code;
    ++sixel_builtin_psd_trace_code_count;
}

static char const *
sixel_builtin_psd_trace_code_from_message(char const *message)
{
    if (message == NULL || message[0] == '\0') {
        return NULL;
    }
    if (strstr(message, "parsed OrGl glow source/choke/range semantics") !=
            NULL) {
        return "FX_ORGL_SEM";
    }
    if (strstr(message,
               "parsed OrGl effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_ORGL_INACTIVE_PARSE";
    }
    if (strstr(message,
               "parsed OrGl effect object in layer effects") != NULL) {
        return "FX_ORGL_PARSE";
    }
    if (strstr(message,
               "parsed ChFX effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_CHFX_INACTIVE_PARSE";
    }
    if (strstr(message,
               "parsed ChFX effect object in layer effects") != NULL) {
        return "FX_CHFX_PARSE";
    }
    if (strstr(message,
               "parsed GrFl effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_GRFL_INACTIVE_PARSE";
    }
    if (strstr(message,
               "parsed GrFl effect object in layer effects") != NULL) {
        return "FX_GRFL_PARSE";
    }
    if (strstr(message, "parsed IrGl glow source/choke/range semantics") !=
            NULL) {
        return "FX_IRGL_SEM";
    }
    if (strstr(message,
               "parsed IrGl effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_IRGL_INACTIVE_PARSE";
    }
    if (strstr(message, "parsed bevel lighting semantics") != NULL) {
        return "FX_BEVEL_LIGHT_SEM";
    }
    if (strstr(message,
               "parsed ebbl bevel object in layer effects (inactive)") !=
            NULL) {
        return "FX_EBBL_INACTIVE_PARSE";
    }
    if (strstr(message,
               "parsed ebbl bevel object in layer effects") != NULL) {
        return "FX_EBBL_PARSE";
    }
    if (strstr(message,
               "separating deferred solid coverage source and clip gate "
               "in layer fallback") != NULL) {
        return "FX_DEFERRED_SOLID_CLIP_SPLIT";
    }
    if (strstr(message,
               "applying vector stroke and layer effect stroke in layer "
               "fallback") != NULL) {
        return "FX_DUAL_SOURCE_BASE";
    }
    if (strstr(message,
               "recording base dual-stroke source code under deferred "
               "ownership") != NULL) {
        return "FX_DUAL_SOURCE_BASE";
    }
    if (strstr(message,
               "applying deferred vector stroke and layer effect stroke on "
               "clipped group") != NULL) {
        return "FX_DUAL_SOURCE_DEFER";
    }
    if (strstr(message,
               "sharing clipped source alpha for deferred inside dual-stroke "
               "coverage") != NULL) {
        return "FX_DUAL_CLIP_SHARED_DEFER";
    }
    if (strstr(message,
               "applying effect-priority dual-stroke overlap on "
               "clipped group") != NULL) {
        return "FX_DUAL_FXPRI_OVL_DEFER";
    }
    if (strstr(message,
               "applying mode-aware dual-stroke blend in layer fallback") !=
            NULL) {
        return "FX_DUAL_MODE_BASE";
    }
    if (strstr(message,
               "recording base mode-aware dual-stroke code under deferred "
               "ownership") != NULL) {
        return "FX_DUAL_MODE_BASE";
    }
    if (strstr(message,
               "resolving dual-stroke alpha with max coverage in layer "
               "fallback") != NULL) {
        return "FX_DUAL_MAX_ALPHA_BASE";
    }
    if (strstr(message,
               "applying dual-stroke union coverage in layer fallback") !=
            NULL) {
        return "FX_DUAL_OVERLAP_BASE";
    }
    if (strstr(message,
               "recording base dual-stroke overlap code under deferred "
               "ownership") != NULL) {
        return "FX_DUAL_OVERLAP_BASE";
    }
    if (strstr(message,
               "applying dual-stroke overlap decomposition in layer fallback")
            != NULL) {
        return "FX_DUAL_OVERLAP_BASE";
    }
    if (strstr(message,
               "applying mode-aware dual-stroke blend on clipped group") !=
            NULL) {
        return "FX_DUAL_MODE_DEFER";
    }
    if (strstr(message,
               "resolving deferred dual-stroke alpha with max coverage on "
               "clipped group") != NULL) {
        return "FX_DUAL_MAX_ALPHA_DEFER";
    }
    if (strstr(message,
               "applying deferred dual-stroke overlap decomposition "
               "on clipped group") != NULL) {
        return "FX_DUAL_OVERLAP_DEFER";
    }
    if (strstr(message,
               "applying deferred dual-stroke union on clipped group") !=
            NULL) {
        return "FX_DUAL_OVERLAP_DEFER";
    }
    if (strstr(message, "applying solid overlay effect in layer fallback") !=
            NULL) {
        return "FX_SOLID_OVERLAY_BASE";
    }
    if (strstr(message,
               "applying clip-weighted deferred solid overlay in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_SOLID_OVERLAY_CLIP";
    }
    if (strstr(message, "parsed clbl=0") != NULL) {
        return "FX_CLBL0_PARSE";
    }
    if (strstr(message, "parsed clbl=1") != NULL) {
        return "FX_CLBL1_PARSE";
    }
    if (strstr(message, "parsed infx=0") != NULL) {
        return "FX_INFX0_PARSE";
    }
    if (strstr(message, "parsed infx=1") != NULL) {
        return "FX_INFX1_PARSE";
    }
    if (strstr(message,
               "clbl=0; deferring interior effects to clipped group "
               "composite") != NULL) {
        return "FX_CLBL0_DEFER_INTERIOR";
    }
    if (strstr(message,
               "clbl=1; deferring interior overlays to clipped group "
               "composite") != NULL) {
        return "FX_CLBL1_DEFER_INTERIOR";
    }
    if (strstr(message,
               "suppressing clbl=1 deferred base solid/gradient overlays") !=
            NULL) {
        return "FX_CLBL1_BASE_OVERLAY_SUPPRESS";
    }
    if (strstr(message,
               "replaying deferred clbl=1 overlay entry in layer fallback") !=
            NULL) {
        return "FX_CLBL1_DEFERRED_OVERLAY_REPLAY_ENTRY";
    }
    if (strstr(message,
               "replacing deferred clbl=1 overlay replay entry in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_OVERLAY_REPLAY_REPLACE";
    }
    if (strstr(message,
               "skipping deferred overlay replay enqueue while group replay "
               "is locked") != NULL) {
        return "FX_DEFERRED_OVERLAY_REPLAY_SKIP_LOCKED";
    }
    if (strstr(message,
               "suppressing clbl=1 deferred base interior glow/choke/"
               "bevel-shadow") != NULL) {
        return "FX_CLBL1_BASE_INTERIOR_SUPPRESS";
    }
    if (strstr(message,
               "applying clip-weighted deferred interior effects in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_INTERIOR_CLIP";
    }
    if (strstr(message,
               "applying clip-weighted deferred gradient overlay in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_GRADIENT_CLIP";
    }
    if (strstr(message,
               "applying gradient overlay effect in layer fallback") != NULL) {
        return "FX_GRFL_BASE_APPLY";
    }
    if (strstr(message,
               "applying clip-weighted deferred effect stroke in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_EFFECT_STROKE_CLIP";
    }
    if (strstr(message,
               "keeping inside stroke alpha write inside source silhouette")
            != NULL) {
        return "FX_STROKE_ALPHA_INSIDE_BASE";
    }
    if (strstr(message,
               "keeping deferred inside stroke alpha write inside source "
               "silhouette") != NULL) {
        return "FX_STROKE_ALPHA_INSIDE_DEFER";
    }
    if (strstr(message,
               "writing outside stroke alpha from outside component") !=
            NULL) {
        return "FX_STROKE_ALPHA_OUTSIDE_BASE";
    }
    if (strstr(message,
               "splitting deferred center stroke alpha write by outside "
               "component") != NULL) {
        return "FX_STROKE_ALPHA_CENTER_DEFER_SPLIT";
    }
    if (strstr(message,
               "infx=0; skipping interior effects in layer fallback") !=
            NULL) {
        return "FX_INFX0_SKIP_INTERIOR";
    }
    if (strstr(message, "applying bevel shadow in layer fallback") != NULL) {
        return "FX_BEVEL_SHADOW_APPLY";
    }
    if (strstr(message,
               "applying inner glow effect in layer fallback") != NULL) {
        return "FX_IRGL_APPLY";
    }
    if (strstr(message,
               "applying bevel highlight in layer fallback") != NULL) {
        return "FX_BEVEL_HIGHLIGHT_APPLY";
    }
    if (strstr(message, "ignoring knockout in layer fallback") != NULL) {
        return "FX_KNOCKOUT_IGNORE";
    }
    if (strstr(message,
               "applying deferred stroke on clipped group") != NULL) {
        return "FX_DEFERRED_STROKE_CLIPPED";
    }
    if (strstr(message,
               "applying deferred stroke with base silhouette coverage in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_STROKE_BASE_SILHOUETTE";
    }
    if (strstr(message,
               "applying fractional silhouette coverage for deferred effect "
               "stroke in layer fallback") != NULL) {
        return "FX_DEFERRED_STROKE_FRACTIONAL_SIL";
    }
    if (strstr(message,
               "separating deferred stroke coverage source and clip gate in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_STROKE_CLIP_SPLIT";
    }
    if (strstr(message,
               "applying clip-weighted deferred outer effects in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_OUTER_CLIP";
    }
    if (strstr(message,
               "applying deferred outer alpha accumulation in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_OUTER_ALPHA_ACCUM";
    }
    if (strstr(message,
               "applying deferred outer distance-band coverage in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_OUTER_DISTANCE_BAND";
    }
    if (strstr(message,
               "gating deferred outer effects with exterior background in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_OUTER_GATE_BG";
    }
    if (strstr(message,
               "applying distance-map deferred effect stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_DISTANCE_MAP_STROKE";
    }
    if (strstr(message,
               "applying distance-map effect stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DISTANCE_MAP_STROKE_BASE";
    }
    if (strstr(message,
               "using distance-map inside stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DISTANCE_MAP_INSIDE_BASE";
    }
    if (strstr(message,
               "using distance-map center stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DISTANCE_MAP_CENTER_BASE";
    }
    if (strstr(message,
               "using distance-map outside stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DISTANCE_MAP_OUTSIDE_BASE";
    }
    if (strstr(message,
               "using distance-map deferred inside stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_DISTANCE_MAP_INSIDE";
    }
    if (strstr(message,
               "using distance-map deferred center stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_DISTANCE_MAP_CENTER";
    }
    if (strstr(message,
               "using distance-map deferred outside stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_DISTANCE_MAP_OUTSIDE";
    }
    if (strstr(message,
               "applying GrFl alignment semantics in layer effects") !=
            NULL) {
        return "FX_GRFL_ALIGN_SEM";
    }
    if (strstr(message,
               "normalizing GrFl percent scale for effect gradient overlay "
               "in layer fallback") != NULL) {
        return "FX_GRFL_SCALE_NORMALIZE";
    }
    if (strstr(message, "applying effect stroke in layer fallback") != NULL) {
        return "FX_EFFECT_STROKE_BASE";
    }
    if (strstr(message,
               "applying round-join vector stroke coverage in layer "
               "fallback") != NULL) {
        return "FX_VECTOR_ROUND_BASE";
    }
    if (strstr(message,
               "applying miter-limit constrained vector stroke coverage in "
               "layer fallback") != NULL) {
        return "FX_VECTOR_MITER_LIMIT_BASE";
    }
    if (strstr(message,
               "applying deferred round-join vector stroke on clipped group")
            != NULL) {
        return "FX_VECTOR_ROUND_DEFER";
    }
    if (strstr(message,
               "applying stroke-adjusted vector stroke coverage in layer "
               "fallback") != NULL) {
        return "FX_STROKE_ADJUST_BASE";
    }
    if (strstr(message,
               "applying deferred stroke-adjusted vector stroke on clipped "
               "group") != NULL) {
        return "FX_STROKE_ADJUST_DEFER";
    }
    if (strstr(message,
               "skipping vector stroke cap on closed silhouette in layer "
               "fallback") != NULL) {
        return "FX_VECTOR_CAP_SKIP";
    }
    if (strstr(message,
               "preferring vector stroke style over layer effect stroke in "
               "layer fallback") != NULL) {
        return "FX_VECTOR_ONLY_PREFER";
    }
    if (strstr(message,
               "separating deferred gradient coverage source and clip gate in "
               "layer fallback") != NULL) {
        return "FX_DEFERRED_GRADIENT_CLIP_SPLIT";
    }
    if (strstr(message,
               "suppressing synthesized vector stroke on clipping-group "
               "base layer") != NULL) {
        return "FX_VECTOR_STROKE_BASE_SUPPRESS";
    }
    if (strstr(message,
               "parsed bevel highlight channel in layer effects") != NULL) {
        return "FX_BEVEL_HIGHLIGHT_PARSE";
    }
    if (strstr(message,
               "parsed bevel shadow channel in layer effects") != NULL) {
        return "FX_BEVEL_SHADOW_PARSE";
    }
    if (strstr(message,
               "merging legacy lrFX effects missing from lfx2") != NULL) {
        return "FX_LRFX_MERGE";
    }
    if (strstr(message,
               "legacy lrFX contains glow/bevel/sofi records") != NULL) {
        return "FX_LRFX_RECORDS_PRESENT";
    }
    if (strstr(message,
               "ignoring legacy lrFX when lfx2 is present") != NULL) {
        return "FX_LRFX_IGNORE";
    }
    if (strstr(message,
               "parsed DrSh shadow offset semantics in layer effects") !=
            NULL) {
        return "FX_DRSH_OFFSET_SEM";
    }
    if (strstr(message,
               "parsed IrSh shadow offset semantics in layer effects") !=
            NULL) {
        return "FX_IRSH_OFFSET_SEM";
    }
    if (strstr(message,
               "parsed DrSh effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_DRSH_INACTIVE_PARSE";
    }
    if (strstr(message,
               "parsed IrSh effect object in layer effects (inactive)") !=
            NULL) {
        return "FX_IRSH_INACTIVE_PARSE";
    }
    if (strstr(message,
               "applying deferred bevel lighting semantics in layer "
               "fallback") != NULL) {
        return "FX_DEFERRED_BEVEL_LIGHT_SEM";
    }
    return NULL;
}

static void
sixel_builtin_psd_trace_buffer_message(char const *message)
{
    size_t length;
    unsigned int slot;

    length = 0u;
    slot = 0u;
    if (message == NULL || message[0] == '\0') {
        return;
    }
    if (sixel_builtin_psd_trace_message_count >= SIXEL_PSD_TRACE_SEEN_MAX) {
        return;
    }
    slot = sixel_builtin_psd_trace_message_count;
    length = strlen(message);
    if (length >= SIXEL_PSD_TRACE_MESSAGE_MAX) {
        length = SIXEL_PSD_TRACE_MESSAGE_MAX - 1u;
    }
    memcpy(sixel_builtin_psd_trace_messages[slot], message, length);
    sixel_builtin_psd_trace_messages[slot][length] = '\0';
    ++sixel_builtin_psd_trace_message_count;
}

void
sixel_builtin_psd_trace_contract_add_error_code(char const *code)
{
    sixel_builtin_psd_trace_add_code(code);
}

void
sixel_builtin_psd_trace_contract_flush(int rc)
{
    unsigned int i;
    char const *kind;

    i = 0u;
    kind = rc == 0 ? "OK" : "ERR";
    if (!sixel_trace_topic_is_enabled("psd_decode")) {
        sixel_builtin_psd_trace_reset();
        return;
    }

    if (sixel_builtin_psd_trace_code_count == 0u) {
        if (rc == 0) {
            sixel_builtin_psd_trace_add_code("PSD_OK");
        } else {
            sixel_builtin_psd_trace_add_code("PSD_ERR");
        }
    }

    fprintf(stderr, "LSXPSD1|rc=%d|kind=%s|codes=", rc, kind);
    for (i = 0u; i < sixel_builtin_psd_trace_code_count; ++i) {
        if (i != 0u) {
            fprintf(stderr, ",");
        }
        fprintf(stderr, "%s", sixel_builtin_psd_trace_codes[i]);
    }
    fprintf(stderr, "\n");

    if (sixel_builtin_psd_trace_header_only_enabled() != 0) {
        sixel_builtin_psd_trace_reset();
        return;
    }

    for (i = 0u; i < sixel_builtin_psd_trace_message_count; ++i) {
        (sixel_trace_topic_message)(
            "psd_decode",
            "%s",
            sixel_builtin_psd_trace_messages[i]);
    }
    sixel_builtin_psd_trace_reset();
}

void
sixel_builtin_psd_trace_message(char const *topic,
                                char const *message)
{
    char const *code;
    int header_only_enabled;

    code = NULL;
    header_only_enabled = 0;
    if (message == NULL) {
        return;
    }
    if (topic != NULL && strcmp(topic, "psd_decode") == 0) {
        header_only_enabled = sixel_builtin_psd_trace_header_only_enabled();
        if (header_only_enabled == 0 &&
                sixel_builtin_psd_trace_seen(message) != 0) {
            return;
        }
        code = sixel_builtin_psd_trace_code_from_message(message);
        sixel_builtin_psd_trace_add_code(code);
        if (header_only_enabled == 0) {
            sixel_builtin_psd_trace_buffer_message(message);
        }
        return;
    }
    (sixel_trace_topic_message)(topic, "%s", message);
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
