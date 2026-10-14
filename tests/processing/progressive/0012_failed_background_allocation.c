/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

static int background_fail_allocation;

static void *
background_allocate(size_t size)
{
    return background_fail_allocation ? NULL : malloc(size);
}

static void *
background_reallocate(void *data, size_t size)
{
    return background_fail_allocation ? NULL : realloc(data, size);
}

static void *
background_callocate(size_t count, size_t size)
{
    return background_fail_allocation ? NULL : calloc(count, size);
}

int
test_progressive_0012(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    unsigned char *previous;
    uint64_t revision;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    encoder = NULL;
    background_fail_allocation = 0;
    if (sixel_allocator_new(&allocator, background_allocate,
            background_callocate, background_reallocate, free) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    if (sixel_encoder_new(&encoder, allocator) != SIXEL_OK) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    ok = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_BGCOLOR,
                              "#112233") == SIXEL_OK;
    previous = encoder->bgcolor;
    revision = encoder->configuration_revision;
    background_fail_allocation = 1;
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_BGCOLOR, "#abcdef");
    background_fail_allocation = 0;
    ok = ok && SIXEL_FAILED(status) && encoder->bgcolor == previous &&
        previous != NULL && previous[0] == 0x11 && previous[1] == 0x22 &&
        previous[2] == 0x33 && encoder->configuration_revision == revision;
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
