/* SPDX-License-Identifier: MIT */
/* An interrupted source must not be published as a successfully read chunk. */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <stdio.h>
#include <sixel.h>
#include "chunk-factory.h"

static int
chunk_cancel_callback(void *context)
{
    int *calls;

    calls = (int *)context;
    ++*calls;
    return 1;
}

int
test_chunk_cancel_callback(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int calls;
    int result;

    allocator = NULL;
    chunk = NULL;
    calls = 0;
    result = 1;
    if (argc != 2) {
        return 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (status != SIXEL_OK) {
        return 1;
    }
    status = sixel_chunk_create_from_source_cancel(&chunk, argv[1],
                 0, NULL, chunk_cancel_callback, &calls, allocator);
    if (status == SIXEL_INTERRUPTED && chunk == NULL && calls == 1) {
        result = 0;
    } else {
        fprintf(stderr, "chunk cancellation: status=%d calls=%d chunk=%p\n",
                status, calls, (void *)chunk);
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}
