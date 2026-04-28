/*
 * Verify loader predicate gates through factory-created
 * ILoaderComponent objects.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "src/chunk.h"
#include "src/factory.h"
#include "src/loader.h"
#include "tests/loader/pixelformat_test_common.h"

static int
loader_expect_predicate(sixel_loader_component_interface_t *loader,
                        sixel_chunk_t const *chunk,
                        int expected,
                        char const *label)
{
    int matched;

    matched = 0;
    if (loader == NULL || chunk == NULL || label == NULL) {
        fprintf(stderr, "invalid test setup: %s\n",
                label != NULL ? label : "unknown");
        return 1;
    }

    matched = sixel_loader_component_predicate(loader, chunk);
    if (matched != expected) {
        fprintf(stderr,
                "unexpected predicate result for %s: got=%d expected=%d\n",
                label,
                matched,
                expected);
        return 1;
    }

    return 0;
}

int
test_loader_0056_loader_factory_predicate_gate(int argc, char **argv)
{
    static unsigned char const gif_data[] = {
        'G', 'I', 'F', '8', '9', 'a'
    };
    static unsigned char const png_interlaced_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const png_plain_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    sixel_factory_t *factory;
    sixel_allocator_t *allocator;
    sixel_loader_component_interface_t *gd_loader;
    sixel_loader_component_interface_t *builtin_loader;
    sixel_chunk_t gif_chunk;
    sixel_chunk_t png_interlaced_chunk;
    sixel_chunk_t png_plain_chunk;
    SIXELSTATUS status;
    int exit_status;

    (void)argc;
    (void)argv;

    factory = NULL;
    allocator = NULL;
    gd_loader = NULL;
    builtin_loader = NULL;
    memset(&gif_chunk, 0, sizeof(gif_chunk));
    memset(&png_interlaced_chunk, 0, sizeof(png_interlaced_chunk));
    memset(&png_plain_chunk, 0, sizeof(png_plain_chunk));
    status = SIXEL_FALSE;
    exit_status = 0;

    status = sixel_factory_get_default(&factory);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_factory_get_default failed: %d\n", status);
        return EXIT_FAILURE;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }

    status = factory->vtbl->create(factory,
                                   "loader/gd",
                                   allocator,
                                   (void **)&gd_loader);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return SIXEL_TEST_SKIP;
    }

    status = factory->vtbl->create(factory,
                                   "loader/builtin",
                                   allocator,
                                   (void **)&builtin_loader);
    if (SIXEL_FAILED(status)) {
        sixel_loader_component_unref(gd_loader);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    gif_chunk.buffer = (unsigned char *)gif_data;
    gif_chunk.size = sizeof(gif_data);
    png_interlaced_chunk.buffer = (unsigned char *)png_interlaced_data;
    png_interlaced_chunk.size = sizeof(png_interlaced_data);
    png_plain_chunk.buffer = (unsigned char *)png_plain_data;
    png_plain_chunk.size = sizeof(png_plain_data);

    if (loader_expect_predicate(gd_loader,
                                &gif_chunk,
                                0,
                                "gd-gif") != 0) {
        exit_status = 1;
    }
    if (loader_expect_predicate(gd_loader,
                                &png_interlaced_chunk,
                                0,
                                "gd-png-interlaced") != 0) {
        exit_status = 1;
    }
    if (loader_expect_predicate(gd_loader,
                                &png_plain_chunk,
                                1,
                                "gd-png-plain") != 0) {
        exit_status = 1;
    }
    if (loader_expect_predicate(builtin_loader,
                                &gif_chunk,
                                1,
                                "builtin-gif") != 0) {
        exit_status = 1;
    }

    sixel_loader_component_unref(builtin_loader);
    sixel_loader_component_unref(gd_loader);
    sixel_allocator_unref(allocator);
    return exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
