/* SPDX-License-Identifier: MIT */
/* The ormode factory publishes the indexed body interface. */
#include <stdlib.h>
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_core_ormode_factory(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_encoder_core_t *core;
    void *object;
    SIXELSTATUS status;
    int valid;

    (void)argc;
    (void)argv;
    allocator = NULL;
    object = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_components_getservice("services/factory", &object);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    factory = object;
    object = NULL;
    status = factory->vtbl->create(factory, "codec/encoder-core.ormode",
        allocator, &object);
    factory->vtbl->unref(factory);
    core = object;
    valid = status == SIXEL_OK && core != NULL && core->vtbl != NULL &&
        core->vtbl->prepare != NULL && core->vtbl->consume != NULL &&
        core->vtbl->finish != NULL && core->vtbl->abort != NULL;
    if (core != NULL) {
        core->vtbl->unref(core);
    }
    sixel_allocator_unref(allocator);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
