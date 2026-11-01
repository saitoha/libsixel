/* SPDX-License-Identifier: MIT */
/* The body encoder refuses an incomplete producer at finish. */
#include <stdlib.h>
#include "../../_support/codec/indexed_component_common.h"

static SIXELSTATUS
indexed_probe_sink_0042(char const *bytes, size_t size, void *context)
{
    (void)bytes;
    (void)size;
    (void)context;
    return SIXEL_OK;
}

int
test_indexed_core_early_finish(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_encoder_core_t *core;
    sixel_encoder_core_prepare_request_t request;
    void *object;
    int ok;

    (void)argc;
    (void)argv;
    core = NULL;
    object = NULL;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (sixel_encoder_core_factory_new(f.allocator, &object) != SIXEL_OK) {
        goto end;
    }
    core = object;
    memset(&request, 0, sizeof(request));
    request.image = f.image;
    request.threads = request.queue_bands = 1;
    request.sink.write = indexed_probe_sink_0042;
    if (core->vtbl->prepare(core, &request) != SIXEL_OK) goto end;
    ok = core->vtbl->finish(core) == SIXEL_LOGIC_ERROR;
end:
    if (core != NULL) core->vtbl->unref(core);
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
