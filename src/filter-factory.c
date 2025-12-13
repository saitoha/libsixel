/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-factory.h"
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

static const sixel_filter_factory_entry_t
        sixel_filter_factory_entries[] = {
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

SIXELSTATUS
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

SIXELSTATUS
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
