/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 */

#ifndef LIBSIXEL_LOADER_MANAGER_H
#define LIBSIXEL_LOADER_MANAGER_H

#include <sixel.h>

#include "chunk.h"
#include "loader-chain.h"
#include "loader-factory.h"

typedef struct sixel_loader_manager sixel_loader_manager_t;

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
 * 1) build_plan(): parse order text into prioritized entry pointers.
 * 2) build_chain_from_plan(): ask factory for per-entry eligibility and
 *    materialize components in plan order.
 * 3) execute_chain(): run configured components until one succeeds.
 */

SIXELSTATUS
loader_manager_get_default(sixel_loader_manager_t **ppmanager);

void
loader_manager_ref(sixel_loader_manager_t *manager);

void
loader_manager_unref(sixel_loader_manager_t *manager);

void
loader_manager_apply_loader_suboptions(char const *order);

size_t
loader_manager_build_plan(
    char const *order,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity);

SIXELSTATUS
loader_manager_build_chain_from_plan(
    sixel_loader_manager_t *manager,
    sixel_loader_entry_t const **plan,
    size_t plan_length,
    sixel_chunk_t const *chunk,
    sixel_allocator_t *allocator,
    sixel_loader_chain_t **ppchain);

SIXELSTATUS
loader_manager_execute_chain(
    sixel_loader_manager_t *manager,
    sixel_loader_chain_t const *chain,
    sixel_chunk_t const *chunk,
    sixel_load_image_function fn_load,
    void *load_context,
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
