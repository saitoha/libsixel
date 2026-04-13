/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_LOADER_MANAGER_H
#define LIBSIXEL_LOADER_MANAGER_H

#include <sixel.h>

#include "chunk.h"
#include "loader-chain.h"
#include "loader-factory.h"
#include "logger.h"
#include "options.h"

typedef struct sixel_loader_manager sixel_loader_manager_t;

typedef struct sixel_loader_suboptions {
    int wic_ico_minsize;
    int libjpeg_enable_cms;
    int libjpeg_cms_engine;
    int libjpeg_enable_orientation;
    int libpng_enable_cms;
    int libpng_cms_engine;
    int libpng_enable_orientation;
    int libwebp_enable_cms;
    int libwebp_cms_engine;
    int libwebp_enable_orientation;
    int coregraphics_enable_orientation;
    int libtiff_enable_cms;
    int libtiff_cms_engine;
    int builtin_enable_cms;
    int builtin_cms_engine;
    int builtin_bmp_info40_mode;
} sixel_loader_suboptions_t;

typedef SIXELSTATUS (*sixel_loader_manager_configure_component_fn)(
    sixel_loader_component_t *component,
    void *context);

typedef void (*sixel_loader_manager_trace_try_fn)(
    char const *name,
    void *context);

typedef void (*sixel_loader_manager_trace_result_fn)(
    char const *name,
    SIXELSTATUS status,
    void *context);

/*
 * Manager API boundary
 *
 * 1) parse_loader_order(): parse order text into a validated resolution.
 * 2) resolve_loader_suboptions(): resolve defaults + overrides into an
 *    execution-local suboption context.
 * 3) build_plan_from_resolution(): map resolution entries to factory entries.
 * 4) build_chain_from_plan(): apply coarse eligibility (magic signature) and
 *    materialize components in plan order.
 * 5) execute_chain(): evaluate predicate eligibility lazily for reached
 *    components, then run until one succeeds.
 */

SIXELSTATUS
loader_manager_get_default(sixel_loader_manager_t **ppmanager);

void
loader_manager_ref(sixel_loader_manager_t *manager);

void
loader_manager_unref(sixel_loader_manager_t *manager);

SIXELSTATUS
loader_manager_parse_loader_order(
    char const *order,
    sixel_option_argument_list_resolution_t *resolution);

size_t
loader_manager_build_plan_from_resolution(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity);

void
loader_manager_init_loader_suboptions(
    sixel_loader_suboptions_t *suboptions);

void
loader_manager_resolve_loader_suboptions(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_suboptions_t *suboptions);

SIXELSTATUS
loader_manager_build_chain_from_plan(
    sixel_loader_manager_t *manager,
    sixel_loader_entry_t const **plan,
    size_t plan_length,
    sixel_chunk_t const *chunk,
    int skip_predicate_gate,
    sixel_allocator_t *allocator,
    sixel_loader_chain_t **ppchain);

SIXELSTATUS
loader_manager_execute_chain(
    sixel_loader_manager_t *manager,
    sixel_loader_chain_t const *chain,
    sixel_chunk_t const *chunk,
    int skip_predicate_gate,
    sixel_load_image_function fn_load,
    void *load_context,
    sixel_logger_t *timeline_logger,
    int *timeline_job_seq,
    sixel_loader_manager_configure_component_fn fn_configure,
    void *configure_context,
    sixel_loader_manager_trace_try_fn fn_try,
    sixel_loader_manager_trace_result_fn fn_result,
    void *trace_context,
    char const **selected_name);

#endif /* LIBSIXEL_LOADER_MANAGER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
