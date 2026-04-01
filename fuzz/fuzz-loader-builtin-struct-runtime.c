/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <sixel.h>

#include "chunk.h"
#include "loader-common.h"
#include "loader-component.h"
#include "loader-factory.h"

#include "fuzz-loader-builtin-struct-common.h"

#ifndef FUZZ_LOADER_COMPONENT_NAME
# define FUZZ_LOADER_COMPONENT_NAME "builtin"
#endif

static char const *const g_loader_component_name = FUZZ_LOADER_COMPONENT_NAME;

static sixel_allocator_t *g_allocator = NULL;
static sixel_loader_factory_t *g_factory = NULL;
static sixel_loader_component_t *g_component = NULL;
static int g_runtime_ready = 0;
static int g_atexit_registered = 0;
static unsigned char g_empty_input[1];

static uint64_t
fuzz_data_mix64(uint8_t const *data, size_t size)
{
    uint64_t h;
    size_t i;

    h = UINT64_C(1469598103934665603);
    if (data == NULL) {
        return h;
    }

    for (i = 0u; i < size; ++i) {
        h ^= (uint64_t)data[i];
        h *= UINT64_C(1099511628211);
    }
    h ^= (uint64_t)size;

    return h;
}

static int
fuzz_pick_reqcolors(unsigned char byte)
{
    return 2 + ((int)byte % (SIXEL_PALETTE_MAX - 1));
}

static int
fuzz_pick_loop_control(unsigned char byte)
{
    switch (byte % 3u) {
    case 0u:
        return SIXEL_LOOP_AUTO;
    case 1u:
        return SIXEL_LOOP_FORCE;
    default:
        return SIXEL_LOOP_DISABLE;
    }
}

static int
fuzz_pick_start_frame(unsigned char byte)
{
    switch (byte % 8u) {
    case 0u:
        return -2;
    case 1u:
        return -1;
    case 2u:
        return 0;
    case 3u:
        return 1;
    case 4u:
        return 2;
    case 5u:
        return 3;
    case 6u:
        return 8;
    default:
        return INT_MAX;
    }
}

static void
fuzz_pick_loader_options(uint8_t const *data,
                         size_t size,
                         int *fstatic,
                         int *fuse_palette,
                         int *reqcolors,
                         int *loop_control,
                         int *start_frame_no,
                         int *enable_cms,
                         int *use_bgcolor,
                         unsigned char bgcolor[3])
{
    uint64_t h;

    h = fuzz_data_mix64(data, size);
    *fstatic = (int)(h & UINT64_C(1));
    *fuse_palette = (int)((h >> 1) & UINT64_C(1));
    *reqcolors = fuzz_pick_reqcolors((unsigned char)(h >> 8));
    *loop_control = fuzz_pick_loop_control((unsigned char)(h >> 16));
    *start_frame_no = fuzz_pick_start_frame((unsigned char)(h >> 24));
    *enable_cms = (int)((h >> 32) & UINT64_C(1));
    *use_bgcolor = (int)((h >> 33) & UINT64_C(1));
    bgcolor[0] = (unsigned char)(h >> 40);
    bgcolor[1] = (unsigned char)(h >> 48);
    bgcolor[2] = (unsigned char)(h >> 56);
}

static SIXELSTATUS
fuzz_frame_callback(sixel_frame_t *frame, void *context)
{
    (void)context;
    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

void
fuzz_loader_builtin_runtime_shutdown(void)
{
    if (g_component != NULL) {
        sixel_loader_component_unref(g_component);
        g_component = NULL;
    }
    if (g_factory != NULL) {
        loader_factory_unref(g_factory);
        g_factory = NULL;
    }
    if (g_allocator != NULL) {
        sixel_allocator_unref(g_allocator);
        g_allocator = NULL;
    }
    g_runtime_ready = 0;
}

static int
fuzz_loader_builtin_runtime_init(void)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (g_runtime_ready) {
        return 1;
    }

    status = sixel_allocator_new(&g_allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_builtin_runtime_shutdown();
        return 0;
    }

    status = loader_factory_get_default(&g_factory);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_builtin_runtime_shutdown();
        return 0;
    }

    status = loader_factory_create_component(g_factory,
                                             g_loader_component_name,
                                             g_allocator,
                                             &g_component);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_builtin_runtime_shutdown();
        return 0;
    }

    g_runtime_ready = 1;
    return 1;
}

int
fuzz_loader_builtin_runtime_bootstrap(void)
{
    if (!fuzz_loader_builtin_runtime_init()) {
        return 0;
    }

    if (!g_atexit_registered) {
        (void)atexit(fuzz_loader_builtin_runtime_shutdown);
        g_atexit_registered = 1;
    }

    return 1;
}

int
fuzz_loader_builtin_runtime_run(uint8_t const *option_data,
                                size_t option_size,
                                unsigned char const *payload,
                                size_t payload_size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 4 * 1024 * 1024 };

    sixel_chunk_t chunk;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int start_frame_no;
    int enable_cms;
    int use_bgcolor;
    unsigned char bgcolor[3];

    if (payload_size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (payload == NULL && payload_size > 0u) {
        return 0;
    }

    if (!g_runtime_ready && !fuzz_loader_builtin_runtime_init()) {
        return 0;
    }
    if (g_component == NULL || g_allocator == NULL) {
        return 0;
    }

    if (option_data == NULL) {
        option_data = payload;
        option_size = payload_size;
    }

    fuzz_pick_loader_options(option_data,
                             option_size,
                             &fstatic,
                             &fuse_palette,
                             &reqcolors,
                             &loop_control,
                             &start_frame_no,
                             &enable_cms,
                             &use_bgcolor,
                             bgcolor);

    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                        &fstatic);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_USE_PALETTE,
                                        &fuse_palette);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_REQCOLORS,
                                        &reqcolors);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                        &loop_control);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_START_FRAME_NO,
                                        &start_frame_no);
    if (use_bgcolor) {
        (void)sixel_loader_component_setopt(g_component,
                                            SIXEL_LOADER_OPTION_BGCOLOR,
                                            bgcolor);
    } else {
        (void)sixel_loader_component_setopt(g_component,
                                            SIXEL_LOADER_OPTION_BGCOLOR,
                                            NULL);
    }
    sixel_helper_set_builtin_enable_cms(enable_cms);

    if (payload_size == 0u) {
        chunk.buffer = g_empty_input;
    } else {
        chunk.buffer = (unsigned char *)(uintptr_t)payload;
    }
    chunk.size = payload_size;
    chunk.max_size = payload_size;
    chunk.source_path = NULL;
    chunk.allocator = g_allocator;

    (void)sixel_loader_component_load(g_component,
                                      &chunk,
                                      fuzz_frame_callback,
                                      NULL);

    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
