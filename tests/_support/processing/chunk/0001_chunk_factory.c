/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for chunk creation through the component factory.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/compat_stub.h"
#include "src/factory.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("image/chunk", allocator, &chunk)
 * IChunk.init_memory(request)
 * IChunk.init_source(request)
 * IChunk.get_bytes(view)
 * IChunk.source_path()
 * IChunk.ref()
 * IChunk.unref()
 */

static unsigned char const memory_bytes[] = "chunk-memory";
static char const memory_path[] = "memory://chunk-factory-test";
static int tracked_allocator_free_count;

static void *
tracked_malloc(size_t size)
{
    return malloc(size);
}

static void *
tracked_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *
tracked_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
tracked_free(void *ptr)
{
    if (ptr != NULL) {
        ++tracked_allocator_free_count;
    }
    free(ptr);
}

static int
build_source_path(char *path, size_t path_size)
{
    char const *source_root;
    int written;

    source_root = sixel_compat_getenv("TOP_SRCDIR");
    if (source_root == NULL || source_root[0] == '\0') {
        source_root = ".";
    }

    written = snprintf(path,
                       path_size,
                       "%s/tests/data/inputs/small.ppm",
                       source_root);
    if (written < 0 || (size_t)written >= path_size) {
        return 0;
    }

    return 1;
}

static int
test_chunk_factory_component(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_chunk_t *chunk;
    sixel_chunk_bytes_view_t view;
    sixel_chunk_memory_request_t memory_request;
    sixel_chunk_source_request_t source_request;
    void *service;
    void *object;
    char source_path[4096];
    int free_count_before_unref;
    int cancel_flag;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    chunk = NULL;
    service = NULL;
    object = NULL;
    free_count_before_unref = 0;
    cancel_flag = 0;
    memset(&view, 0, sizeof(view));
    memset(&memory_request, 0, sizeof(memory_request));
    memset(&source_request, 0, sizeof(source_request));
    memset(source_path, 0, sizeof(source_path));

    tracked_allocator_free_count = 0;
    status = sixel_allocator_new(&allocator,
                                 tracked_malloc,
                                 tracked_calloc,
                                 tracked_realloc,
                                 tracked_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = factory->vtbl->create(factory, "image/chunk",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    chunk = (sixel_chunk_t *)object;
    object = NULL;
    if (chunk == NULL || chunk->vtbl == NULL ||
        chunk->vtbl->ref == NULL ||
        chunk->vtbl->unref == NULL ||
        chunk->vtbl->init_memory == NULL ||
        chunk->vtbl->init_source == NULL ||
        chunk->vtbl->get_bytes == NULL ||
        chunk->vtbl->source_path == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /*
     * The chunk retains the allocator internally. Dropping the caller's
     * reference here keeps the test honest without exposing an allocator()
     * accessor on the interface.
     */
    sixel_allocator_unref(allocator);
    allocator = NULL;

    memory_request.bytes = memory_bytes;
    memory_request.size = sizeof(memory_bytes) - 1u;
    memory_request.source_path = memory_path;
    status = chunk->vtbl->init_memory(chunk, &memory_request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = chunk->vtbl->get_bytes(chunk, &view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (view.size != sizeof(memory_bytes) - 1u ||
        memcmp(view.bytes, memory_bytes, view.size) != 0 ||
        strcmp(chunk->vtbl->source_path(chunk), memory_path) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    chunk->vtbl->ref(chunk);
    chunk->vtbl->unref(chunk);

    if (!build_source_path(source_path, sizeof(source_path))) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    source_request.filename = source_path;
    source_request.finsecure = 0;
    source_request.cancel_flag = &cancel_flag;
    status = chunk->vtbl->init_source(chunk, &source_request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = chunk->vtbl->get_bytes(chunk, &view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (view.bytes == NULL || view.size == 0u ||
        strcmp(chunk->vtbl->source_path(chunk), source_path) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    free_count_before_unref = tracked_allocator_free_count;
    chunk->vtbl->unref(chunk);
    chunk = NULL;
    if (tracked_allocator_free_count <= free_count_before_unref) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_chunk_t *)object)->vtbl->unref((sixel_chunk_t *)object);
    }
    if (chunk != NULL && chunk->vtbl != NULL && chunk->vtbl->unref != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_chunk_0001_chunk_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_chunk_factory_component()) {
        fprintf(stderr, "chunk factory component contract failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
