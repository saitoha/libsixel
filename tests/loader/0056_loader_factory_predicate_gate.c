/*
 * Verify loader factory honors registry predicates when selecting entries.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "src/chunk.h"
#include "src/loader-factory.h"
#include "src/loader-registry.h"
#include "tests/loader/pixelformat_test_common.h"

static sixel_loader_entry_t const *
loader_factory_find_entry(sixel_loader_entry_t const *entries,
                          size_t entry_count,
                          char const *name)
{
    size_t index;

    index = 0u;
    if (entries == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0u; index < entry_count; ++index) {
        if (entries[index].name == NULL) {
            continue;
        }
        if (strcmp(entries[index].name, name) == 0) {
            return &entries[index];
        }
    }

    return NULL;
}

static int
loader_factory_expect_match(sixel_loader_factory_t *factory,
                            sixel_loader_entry_t const *entry,
                            sixel_chunk_t const *chunk,
                            int expected,
                            char const *label)
{
    int matched;

    matched = 0;
    if (factory == NULL || entry == NULL || chunk == NULL || label == NULL) {
        fprintf(stderr, "invalid test setup: %s\n",
                label != NULL ? label : "unknown");
        return 1;
    }

    matched = loader_factory_entry_matches_chunk(factory, entry, chunk);
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
    /* GIF signature only: enough for loader sniff/predicate checks. */
    static unsigned char const gif_data[] = {
        'G', 'I', 'F', '8', '9', 'a'
    };
    /*
     * PNG signature + IHDR fields up to interlace method.
     * Interlace method is 1 (Adam7) for the delegated path check.
     */
    static unsigned char const png_interlaced_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    /* Same synthetic IHDR with interlace method set to 0. */
    static unsigned char const png_plain_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    sixel_loader_factory_t *factory;
    sixel_loader_entry_t const *entries;
    size_t entry_count;
    sixel_loader_entry_t const *gd_entry;
    sixel_loader_entry_t const *builtin_entry;
    sixel_chunk_t gif_chunk;
    sixel_chunk_t png_interlaced_chunk;
    sixel_chunk_t png_plain_chunk;
    SIXELSTATUS status;
    int exit_status;

    (void)argc;
    (void)argv;

    factory = NULL;
    entries = NULL;
    entry_count = 0u;
    gd_entry = NULL;
    builtin_entry = NULL;
    memset(&gif_chunk, 0, sizeof(gif_chunk));
    memset(&png_interlaced_chunk, 0, sizeof(png_interlaced_chunk));
    memset(&png_plain_chunk, 0, sizeof(png_plain_chunk));
    status = SIXEL_FALSE;
    exit_status = 0;

    status = loader_factory_get_default(&factory);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "loader_factory_get_default failed: %d\n", status);
        return EXIT_FAILURE;
    }

    entry_count = loader_factory_get_entries(factory, &entries);
    gd_entry = loader_factory_find_entry(entries, entry_count, "gd");
    builtin_entry = loader_factory_find_entry(entries, entry_count, "builtin");
    if (gd_entry == NULL || builtin_entry == NULL) {
        loader_factory_unref(factory);
        return SIXEL_TEST_SKIP;
    }

    gif_chunk.buffer = (unsigned char *)gif_data;
    gif_chunk.size = sizeof(gif_data);
    png_interlaced_chunk.buffer = (unsigned char *)png_interlaced_data;
    png_interlaced_chunk.size = sizeof(png_interlaced_data);
    png_plain_chunk.buffer = (unsigned char *)png_plain_data;
    png_plain_chunk.size = sizeof(png_plain_data);

    if (loader_factory_expect_match(factory,
                                    gd_entry,
                                    &gif_chunk,
                                    0,
                                    "gd-gif") != 0) {
        exit_status = 1;
    }
    if (loader_factory_expect_match(factory,
                                    gd_entry,
                                    &png_interlaced_chunk,
                                    0,
                                    "gd-png-interlaced") != 0) {
        exit_status = 1;
    }
    if (loader_factory_expect_match(factory,
                                    gd_entry,
                                    &png_plain_chunk,
                                    1,
                                    "gd-png-plain") != 0) {
        exit_status = 1;
    }
    if (loader_factory_expect_match(factory,
                                    builtin_entry,
                                    &gif_chunk,
                                    1,
                                    "builtin-gif") != 0) {
        exit_status = 1;
    }

    loader_factory_unref(factory);
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
