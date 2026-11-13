/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

/* A byte-only injected capability must reject a fractional consumer. All
 * production lookup policies support fractional queries, so this boundary
 * is represented by a test provider, not a dormant production adapter. */
typedef struct byte_only_lookup {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_gpu_lookup_preparer_interface_t *owner;
    int rejected;
} byte_only_lookup_t;

static void
byte_only_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    byte_only_lookup_t *lookup;

    lookup = (byte_only_lookup_t *)self;
    lookup->owner->vtbl->ref(lookup->owner);
}

static void
byte_only_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    byte_only_lookup_t *lookup;

    lookup = (byte_only_lookup_t *)self;
    lookup->owner->vtbl->unref(lookup->owner);
}

static SIXELSTATUS
byte_only_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    byte_only_lookup_t *lookup;

    lookup = (byte_only_lookup_t *)self;
    *result = NULL;
    if (request->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        lookup->rejected = 1;
        return SIXEL_FEATURE_ERROR;
    }
    return lookup->owner->vtbl->prepare(lookup->owner, request, device, result);
}

static sixel_gpu_lookup_preparer_vtbl_t const byte_only_vtbl = {
    byte_only_ref, byte_only_unref, byte_only_prepare
};

int
test_lso2_gpu_byte_lookup_rejected(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    byte_only_lookup_t lookup;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    lookup.iface.vtbl = &byte_only_vtbl;
    lookup.owner = f.lookup;
    lookup.rejected = 0;
    f.prepare.gpu_lookup = &lookup.iface;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_FEATURE_ERROR);
    LSO2_GPU_CHECK(lookup.rejected);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
