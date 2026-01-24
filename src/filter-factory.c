/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-factory.h"
#include "filter-colors.h"
#include "filter-clip.h"
#include "filter-final-merge.h"
#include "filter-dither.h"
#include "filter-load.h"
#include "filter-lookup.h"
#include "filter-palette.h"
#include "filter-vpte.h"
#include "filter-eytzinger.h"
#include "filter-encode.h"
#include "filter-resize.h"
#include "filter-sample.h"
#include "filter.h"

typedef SIXELSTATUS (*sixel_filter_initializer_fn)(sixel_filter_t *filter,
                                                   const void *config);

typedef struct sixel_filter_factory_entry {
    const char *name;
    sixel_filter_kind_t kind;
    sixel_filter_initializer_fn initializer;
} sixel_filter_factory_entry_t;

static SIXELSTATUS
sixel_filter_factory_load_init(sixel_filter_t *filter, const void *config)
{
    const sixel_filter_load_config_t *load_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    load_config = (const sixel_filter_load_config_t *)config;

    return sixel_filter_load_init(filter, load_config);
}

static SIXELSTATUS
sixel_filter_factory_clip_init(sixel_filter_t *filter,
                               const void *config)
{
    const sixel_filter_clip_config_t *clip_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    clip_config = (const sixel_filter_clip_config_t *)config;

    return sixel_filter_clip_init(filter, clip_config);
}

static SIXELSTATUS
sixel_filter_factory_sample_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_sample_config_t *sample_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sample_config = (const sixel_filter_sample_config_t *)config;

    return sixel_filter_sample_init(filter, sample_config);
}

static SIXELSTATUS
sixel_filter_factory_resize_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_resize_config_t *resize_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    resize_config = (const sixel_filter_resize_config_t *)config;

    return sixel_filter_resize_init(filter, resize_config);
}

static SIXELSTATUS
sixel_filter_factory_colors_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_colors_config_t *colors_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    colors_config = (const sixel_filter_colors_config_t *)config;

    return sixel_filter_colors_init(filter, colors_config);
}

static SIXELSTATUS
sixel_filter_factory_lookup_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_lookup_config_t *lookup_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lookup_config = (const sixel_filter_lookup_config_t *)config;

    return sixel_filter_lookup_init(filter, lookup_config);
}

static SIXELSTATUS
sixel_filter_factory_palette_init(sixel_filter_t *filter,
                                  const void *config)
{
    const sixel_filter_palette_config_t *palette_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette_config = (const sixel_filter_palette_config_t *)config;

    return sixel_filter_palette_init(filter, palette_config);
}

static SIXELSTATUS
sixel_filter_factory_vpte_init(sixel_filter_t *filter, const void *config)
{
    const sixel_filter_vpte_config_t *vpte_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    vpte_config = (const sixel_filter_vpte_config_t *)config;

    return sixel_filter_vpte_init(filter, vpte_config);
}

static SIXELSTATUS
sixel_filter_factory_1d_eytzinger_init(sixel_filter_t *filter,
                                       const void *config)
{
    const sixel_filter_1d_eytzinger_config_t *eytzinger_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    eytzinger_config = (const sixel_filter_1d_eytzinger_config_t *)config;

    return sixel_filter_1d_eytzinger_init(filter, eytzinger_config);
}

static SIXELSTATUS
sixel_filter_factory_final_merge_init(sixel_filter_t *filter,
                                      const void *config)
{
    const sixel_filter_final_merge_config_t *merge_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    merge_config = (const sixel_filter_final_merge_config_t *)config;

    return sixel_filter_final_merge_init(filter, merge_config);
}

static SIXELSTATUS
sixel_filter_factory_dither_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_dither_config_t *dither_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    dither_config = (const sixel_filter_dither_config_t *)config;

    return sixel_filter_dither_init(filter, dither_config);
}

static SIXELSTATUS
sixel_filter_factory_encode_init(sixel_filter_t *filter,
                                 const void *config)
{
    const sixel_filter_encode_config_t *encode_config;

    if (config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    encode_config = (const sixel_filter_encode_config_t *)config;

    return sixel_filter_encode_init(filter, encode_config);
}

static const sixel_filter_factory_entry_t
        sixel_filter_factory_entries[] = {
    {"load", SIXEL_FILTER_KIND_LOAD, sixel_filter_factory_load_init},
    {"clip", SIXEL_FILTER_KIND_CLIP, sixel_filter_factory_clip_init},
    {"colorspace",
     SIXEL_FILTER_KIND_COLORS,
     sixel_filter_factory_colors_init},
    {"dither", SIXEL_FILTER_KIND_DITHER, sixel_filter_factory_dither_init},
    {"final-merge",
     SIXEL_FILTER_KIND_FINAL_MERGE,
     sixel_filter_factory_final_merge_init},
    {"encode", SIXEL_FILTER_KIND_ENCODE, sixel_filter_factory_encode_init},
    {"lookup", SIXEL_FILTER_KIND_LOOKUP, sixel_filter_factory_lookup_init},
    {"palette", SIXEL_FILTER_KIND_PALETTE, sixel_filter_factory_palette_init},
    {"vpte", SIXEL_FILTER_KIND_VPTE, sixel_filter_factory_vpte_init},
    {"1d-eytzinger",
     SIXEL_FILTER_KIND_EYTZINGER,
     sixel_filter_factory_1d_eytzinger_init},
    {"resize", SIXEL_FILTER_KIND_RESIZE, sixel_filter_factory_resize_init},
    {"sample", SIXEL_FILTER_KIND_SAMPLE, sixel_filter_factory_sample_init},
};

static SIXELSTATUS
sixel_filter_factory_create_entry(const sixel_filter_factory_entry_t *entry,
                                  const void *config,
                                  sixel_filter_t **filter_out)
{
    SIXELSTATUS status;
    sixel_filter_t *filter;

    if (filter_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *filter_out = NULL;

    status = sixel_filter_alloc(&filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = entry->initializer(filter, config);
    if (SIXEL_FAILED(status)) {
        sixel_filter_free(filter);
        return status;
    }

    *filter_out = filter;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_filter_factory_create_by_name(const char *name,
                                    const void *config,
                                    sixel_filter_t **filter_out)
{
    SIXELSTATUS status;
    size_t index;

    if (name == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0u; index < sizeof(sixel_filter_factory_entries)
            / sizeof(sixel_filter_factory_entries[0]); ++index) {
        if (strcmp(sixel_filter_factory_entries[index].name, name) == 0) {
            status = sixel_filter_factory_create_entry(
                    &sixel_filter_factory_entries[index],
                    config,
                    filter_out);

            return status;
        }
    }

    return SIXEL_BAD_ARGUMENT;
}

SIXELAPI SIXELSTATUS
sixel_filter_factory_create_by_kind(sixel_filter_kind_t kind,
                                    const void *config,
                                    sixel_filter_t **filter_out)
{
    SIXELSTATUS status;
    size_t index;

    for (index = 0u; index < sizeof(sixel_filter_factory_entries)
            / sizeof(sixel_filter_factory_entries[0]); ++index) {
        if (sixel_filter_factory_entries[index].kind == kind) {
            status = sixel_filter_factory_create_entry(
                    &sixel_filter_factory_entries[index],
                    config,
                    filter_out);

            return status;
        }
    }

    return SIXEL_BAD_ARGUMENT;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
