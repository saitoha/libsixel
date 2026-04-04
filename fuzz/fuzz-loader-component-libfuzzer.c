/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <sixel.h>

#include "chunk.h"
#include "loader-common.h"
#include "loader-component.h"
#include "loader-factory.h"

#ifndef FUZZ_LOADER_COMPONENT_NAME
# define FUZZ_LOADER_COMPONENT_NAME "builtin"
#endif

#define FUZZ_MAX_INPUT_BYTES (4u * 1024u * 1024u)

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

static SIXELSTATUS
fuzz_frame_callback(sixel_frame_t *frame, void *context)
{
    (void)context;
    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    (void)sixel_frame_set_delay(frame, 0);
    return SIXEL_OK;
}

static void
fuzz_loader_component_shutdown(void)
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
fuzz_loader_component_init(void)
{
    SIXELSTATUS status;

    if (g_runtime_ready) {
        return 1;
    }

    status = sixel_allocator_new(&g_allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_component_shutdown();
        return 0;
    }

    status = loader_factory_get_default(&g_factory);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_component_shutdown();
        return 0;
    }

    status = loader_factory_create_component(g_factory,
                                             FUZZ_LOADER_COMPONENT_NAME,
                                             g_allocator,
                                             &g_component);
    if (SIXEL_FAILED(status)) {
        fuzz_loader_component_shutdown();
        return 0;
    }

    g_runtime_ready = 1;
    return 1;
}

static int
fuzz_loader_component_bootstrap(void)
{
    if (!fuzz_loader_component_init()) {
        return 0;
    }
    if (!g_atexit_registered) {
        (void)atexit(fuzz_loader_component_shutdown);
        g_atexit_registered = 1;
    }
    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    (void)fuzz_loader_component_bootstrap();
    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    uint64_t h;
    sixel_chunk_t chunk;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int start_frame_no;
    int enable_cms;
    int use_bgcolor;
    unsigned char bgcolor[3];

    if (data == NULL || size > FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (!fuzz_loader_component_bootstrap()) {
        return 0;
    }
    if (g_component == NULL || g_allocator == NULL) {
        return 0;
    }

    h = fuzz_data_mix64(data, size);
    fstatic = (int)(h & UINT64_C(1));
    fuse_palette = (int)((h >> 1) & UINT64_C(1));
    reqcolors = 2 + ((int)((h >> 8) & UINT64_C(0xff)) % (SIXEL_PALETTE_MAX - 1));
    loop_control = SIXEL_LOOP_DISABLE;
    start_frame_no = ((int)((h >> 16) & UINT64_C(0x07))) - 3;
    enable_cms = (int)((h >> 24) & UINT64_C(1));
    use_bgcolor = (int)((h >> 25) & UINT64_C(1));
    bgcolor[0] = (unsigned char)(h >> 32);
    bgcolor[1] = (unsigned char)(h >> 40);
    bgcolor[2] = (unsigned char)(h >> 48);

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
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_CMS,
                                        &enable_cms);

    chunk.buffer = size == 0u ? g_empty_input : (unsigned char *)(uintptr_t)data;
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
