/* SPDX-License-Identifier: MIT */
#ifndef GAMUT_FILTER_COMMON_H
#define GAMUT_FILTER_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "src/filter-factory.h"
#include "src/filter-colors.h"
#include "src/filter-gamut.h"
#include "src/filter-pipeline.h"
#include "src/pixelformat.h"
#include "src/threading.h"
#include "src/options.h"

typedef struct gamut_filter_fixture {
    sixel_allocator_t *allocator;
    sixel_gamut_t *mapper;
    sixel_frame_t *frame;
    sixel_filter_t *color;
    sixel_filter_t *gamut;
} gamut_filter_fixture_t;

static inline void
gamut_filter_free(gamut_filter_fixture_t *f)
{
    sixel_filter_free(f->color);
    sixel_filter_free(f->gamut);
    sixel_frame_unref(f->frame);
    sixel_gamut_free(f->mapper);
    sixel_allocator_unref(f->allocator);
    memset(f, 0, sizeof(*f));
}

static inline int
gamut_filter_init(gamut_filter_fixture_t *f, float *pixels, size_t count,
    int source, int target, int policy)
{
    sixel_filter_colors_config_t color;
    sixel_filter_gamut_config_t gamut;
    int space;

    color.target_pixelformat = target;
    if (SIXEL_FAILED(sixel_allocator_new(&f->allocator,
            NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&f->frame, f->allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(f->frame, pixels,
                (int)count, 1, source, NULL, 0)) ||
            SIXEL_FAILED(sixel_gamut_new(&f->mapper, NULL,
                SIXEL_COLORSPACE_GAMMA, policy, f->allocator))) {
        return 0;
    }
    gamut.mapper = f->mapper;
    if (SIXEL_FAILED(sixel_filter_factory_create_by_name("colorspace",
            &color, &f->color)) ||
            SIXEL_FAILED(sixel_filter_factory_create_by_name("gamut",
                &gamut, &f->gamut))) {
        return 0;
    }
    space = sixel_pixelformat_get_colorspace(target);
    sixel_filter_bind_input(f->color, &f->frame, source,
        sixel_pixelformat_get_colorspace(source));
    sixel_filter_bind_output(f->color, &f->frame, target, space);
    sixel_filter_bind_input(f->gamut, &f->frame, target, space);
    sixel_filter_bind_output(f->gamut, &f->frame, target, space);
    return 1;
}

static inline SIXELSTATUS
gamut_filter_serial(gamut_filter_fixture_t *f)
{
    SIXELSTATUS status;

    status = sixel_filter_run(f->color, f->allocator, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_filter_run(f->gamut, f->allocator, NULL);
    }
    return status;
}

static inline int
gamut_filter_scope(sixel_thread_budget_scope_t *scope, char const *text)
{
    sixel_threading_policy_options_t options;

    memset(&options, 0, sizeof(options));
    if (SIXEL_FAILED(sixel_option_apply_threading_policy_argument(&options,
            text, SIXEL_OPTION_SCOPE_ENCODER, NULL, 0u))) {
        return 0;
    }
    sixel_threading_object_enter(scope, &options, 0);
    return 1;
}
#endif
