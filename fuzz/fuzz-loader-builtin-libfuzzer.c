/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * libFuzzer target for the builtin loader component.
 *
 * This harness intentionally goes through the loader factory/component API:
 *   factory -> create("builtin") -> setopt(...) -> load(...)
 * so it exercises the same wiring used by the production loader pipeline.
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

static sixel_allocator_t *g_allocator = NULL;
static sixel_loader_factory_t *g_factory = NULL;
static sixel_loader_component_t *g_component = NULL;
static int g_fuzz_ready = 0;
static unsigned char g_empty_input[1];

static unsigned char
fuzz_data_byte(uint8_t const *data, size_t size, size_t index)
{
    if (data == NULL || index >= size) {
        return 0;
    }
    return (unsigned char)data[index];
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

static SIXELSTATUS
fuzz_frame_callback(sixel_frame_t *frame, void *context)
{
    (void)context;
    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

static void
fuzz_builtin_loader_dispose(void)
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

    g_fuzz_ready = 0;
}

static int
fuzz_builtin_loader_init(void)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (g_fuzz_ready) {
        return 1;
    }

    status = sixel_allocator_new(&g_allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    status = loader_factory_get_default(&g_factory);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    status = loader_factory_create_component(g_factory,
                                             "builtin",
                                             g_allocator,
                                             &g_component);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    g_fuzz_ready = 1;
    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;

    if (fuzz_builtin_loader_init()) {
        (void)atexit(fuzz_builtin_loader_dispose);
    }

    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
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

    if (size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (data == NULL) {
        return 0;
    }

    if (!g_fuzz_ready && !fuzz_builtin_loader_init()) {
        return 0;
    }
    if (g_component == NULL || g_allocator == NULL) {
        return 0;
    }

    fstatic = (int)(fuzz_data_byte(data, size, 0) & 1u);
    fuse_palette = (int)(fuzz_data_byte(data, size, 1) & 1u);
    reqcolors = fuzz_pick_reqcolors(fuzz_data_byte(data, size, 2));
    loop_control = fuzz_pick_loop_control(fuzz_data_byte(data, size, 3));
    start_frame_no = fuzz_pick_start_frame(fuzz_data_byte(data, size, 4));
    enable_cms = (int)(fuzz_data_byte(data, size, 5) & 1u);
    use_bgcolor = (int)(fuzz_data_byte(data, size, 6) & 1u);
    bgcolor[0] = fuzz_data_byte(data, size, 7);
    bgcolor[1] = fuzz_data_byte(data, size, 8);
    bgcolor[2] = fuzz_data_byte(data, size, 9);

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

    if (size == 0u) {
        chunk.buffer = g_empty_input;
    } else {
        chunk.buffer = (unsigned char *)(uintptr_t)data;
    }
    chunk.size = size;
    chunk.max_size = size;
    chunk.source_path = NULL;
    chunk.allocator = g_allocator;

    (void)sixel_loader_component_load(g_component,
                                      &chunk,
                                      fuzz_frame_callback,
                                      NULL);

    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
