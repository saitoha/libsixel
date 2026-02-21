/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include <sixel.h>

#include "loader-component-legacy.h"

typedef struct sixel_loader_component_legacy {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    sixel_loader_entry_t const *entry;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_component_legacy_t;

static void
sixel_loader_component_legacy_ref(sixel_loader_component_t *component)
{
    sixel_loader_component_legacy_t *legacy;

    legacy = NULL;
    if (component == NULL) {
        return;
    }

    legacy = (sixel_loader_component_legacy_t *)component;
    ++legacy->ref;
}

static void
sixel_loader_component_legacy_unref(sixel_loader_component_t *component)
{
    sixel_loader_component_legacy_t *legacy;
    sixel_allocator_t *allocator;

    legacy = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    legacy = (sixel_loader_component_legacy_t *)component;
    if (legacy->ref == 0u) {
        return;
    }

    --legacy->ref;
    if (legacy->ref > 0u) {
        return;
    }

    allocator = legacy->allocator;
    sixel_allocator_free(allocator, legacy);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_component_legacy_setopt(sixel_loader_component_t *component,
                                     int option,
                                     void const *value)
{
    sixel_loader_component_legacy_t *legacy;
    int const *flag;
    unsigned char const *color;

    legacy = NULL;
    flag = NULL;
    color = NULL;

    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    legacy = (sixel_loader_component_legacy_t *)component;

    /*
     * Common options are normalized by sixel_loader_setopt() before they are
     * copied into each component instance.  This legacy bridge keeps setopt()
     * lightweight and stores values without revalidating the same constraints.
     */
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        legacy->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        legacy->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        legacy->reqcolors = flag != NULL ? *flag : legacy->reqcolors;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            legacy->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        legacy->bgcolor[0] = color[0];
        legacy->bgcolor[1] = color[1];
        legacy->bgcolor[2] = color[2];
        legacy->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        legacy->loop_control = flag != NULL ? *flag : legacy->loop_control;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            legacy->has_start_frame_no = 0;
            legacy->start_frame_no = INT_MIN;
        } else {
            flag = (int const *)value;
            legacy->start_frame_no = *flag;
            legacy->has_start_frame_no = 1;
        }
        return SIXEL_OK;
    default:
        /*
         * Accept unknown options so component-specific options can be routed
         * through the same call path without coupling all implementations.
         */
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_component_legacy_load(sixel_loader_component_t *component,
                                   sixel_chunk_t const *chunk,
                                   sixel_load_image_function fn_load,
                                   void *context)
{
    sixel_loader_component_legacy_t *legacy;
    unsigned char *bgcolor;

    legacy = NULL;
    bgcolor = NULL;

    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    legacy = (sixel_loader_component_legacy_t *)component;
    if (legacy->entry == NULL || legacy->entry->backend == NULL) {
        return SIXEL_LOGIC_ERROR;
    }

    if (legacy->has_bgcolor) {
        bgcolor = legacy->bgcolor;
    }

    return legacy->entry->backend(chunk,
                                  legacy->fstatic,
                                  legacy->fuse_palette,
                                  legacy->reqcolors,
                                  bgcolor,
                                  legacy->loop_control,
                                  legacy->has_start_frame_no,
                                  legacy->start_frame_no,
                                  fn_load,
                                  context);
}

static char const *
sixel_loader_component_legacy_name(sixel_loader_component_t const *component)
{
    sixel_loader_component_legacy_t const *legacy;

    legacy = NULL;
    if (component == NULL) {
        return NULL;
    }

    legacy = (sixel_loader_component_legacy_t const *)component;
    if (legacy->entry == NULL) {
        return NULL;
    }

    return legacy->entry->name;
}

static sixel_loader_component_vtbl_t const g_sixel_loader_component_legacy_vtbl
    = {
        sixel_loader_component_legacy_ref,
        sixel_loader_component_legacy_unref,
        sixel_loader_component_legacy_setopt,
        sixel_loader_component_legacy_load,
        sixel_loader_component_legacy_name
    };

sixel_loader_component_t *
sixel_loader_component_legacy_new(sixel_loader_entry_t const *entry,
                                  sixel_allocator_t *allocator)
{
    sixel_loader_component_legacy_t *legacy;

    legacy = NULL;
    if (entry == NULL || allocator == NULL) {
        return NULL;
    }

    legacy = (sixel_loader_component_legacy_t *)
             sixel_allocator_malloc(allocator, sizeof(*legacy));
    if (legacy == NULL) {
        return NULL;
    }

    memset(legacy, 0, sizeof(*legacy));
    legacy->base.vtbl = &g_sixel_loader_component_legacy_vtbl;
    legacy->allocator = allocator;
    legacy->entry = entry;
    legacy->ref = 1u;
    legacy->reqcolors = SIXEL_PALETTE_MAX;
    legacy->loop_control = SIXEL_LOOP_AUTO;
    legacy->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);

    return &legacy->base;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
