/*
 * Verify librsvg loader probing accepts SVG-like chunks and rejects others.
 */

#include <stdio.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static int
expect_librsvg_predicate(sixel_loader_component_t *loader,
                         sixel_chunk_t const *chunk,
                         int expected,
                         char const *label)
{
    int actual;

    actual = sixel_loader_component_predicate(loader, chunk);
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr,
            "%s: predicate mismatch (got=%d expected=%d)\n",
            label,
            actual,
            expected);
    return 1;
}
#endif

int
test_loader_0020_loader_librsvg_detect_svg_like(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    sixel_chunk_t *chunk;
    sixel_chunk_memory_request_t request;
    unsigned char const svg_with_bom[] =
        "\xef\xbb\xbf<svg xmlns='http://www.w3.org/2000/svg'/>";
    unsigned char const svg_with_xml[] =
        "<?xml version='1.0'?><svg xmlns='http://www.w3.org/2000/svg'/>";
    unsigned char const not_svg[] = "GIF89a";
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    char long_prefix[4106];
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    loader = NULL;
    chunk = NULL;
    result = 0;
    memset(&request, 0, sizeof(request));
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }
    status = create_loader_component_by_name("librsvg",
                                             allocator,
                                             (void **)&loader);
    if (SIXEL_FAILED(status) || loader == NULL) {
        sixel_allocator_unref(allocator);
        fprintf(stderr, "librsvg loader unavailable\n");
        return SIXEL_TEST_SKIP;
    }

    status = sixel_chunk_create_from_memory(&chunk,
                                            svg_with_bom,
                                            sizeof(svg_with_bom) - 1u,
                                            NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        result = 1;
        goto cleanup;
    }
    if (expect_librsvg_predicate(loader,
                                 chunk,
                                 1,
                                 "BOM-prefixed SVG") != 0) {
        result = 1;
    }

    request.bytes = svg_with_xml;
    request.size = sizeof(svg_with_xml) - 1u;
    request.source_path = NULL;
    status = chunk->vtbl->init_memory(chunk, &request);
    if (SIXEL_FAILED(status)) {
        result = 1;
        goto cleanup;
    }
    if (expect_librsvg_predicate(loader,
                                 chunk,
                                 1,
                                 "XML-prefixed SVG") != 0) {
        result = 1;
    }

    request.bytes = not_svg;
    request.size = sizeof(not_svg) - 1u;
    request.source_path = NULL;
    status = chunk->vtbl->init_memory(chunk, &request);
    if (SIXEL_FAILED(status)) {
        result = 1;
        goto cleanup;
    }
    if (expect_librsvg_predicate(loader, chunk, 0, "non-SVG data") != 0) {
        result = 1;
    }

    request.bytes = not_svg;
    request.size = sizeof(not_svg) - 1u;
    request.source_path = "sample.SVGZ";
    status = chunk->vtbl->init_memory(chunk, &request);
    if (SIXEL_FAILED(status)) {
        result = 1;
        goto cleanup;
    }
    if (expect_librsvg_predicate(loader, chunk, 1, ".svgz path hint") != 0) {
        result = 1;
    }

    memset(long_prefix, ' ', sizeof(long_prefix));
    long_prefix[4100] = '<';
    long_prefix[4101] = 's';
    long_prefix[4102] = 'v';
    long_prefix[4103] = 'g';
    long_prefix[4104] = '>';
    long_prefix[4105] = '\0';
    request.bytes = (unsigned char const *)long_prefix;
    request.size = 4105u;
    request.source_path = NULL;
    status = chunk->vtbl->init_memory(chunk, &request);
    if (SIXEL_FAILED(status)) {
        result = 1;
        goto cleanup;
    }
    if (expect_librsvg_predicate(loader,
                                 chunk,
                                 0,
                                 "SVG marker beyond probe limit") != 0) {
        result = 1;
    }

cleanup:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_component_unref(loader);
    sixel_allocator_unref(allocator);
    return result;
#else
    fprintf(stderr, "librsvg loader unavailable\n");
    return SIXEL_TEST_SKIP;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
