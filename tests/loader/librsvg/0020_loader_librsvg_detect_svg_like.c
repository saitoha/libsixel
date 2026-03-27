/*
 * Verify librsvg loader probing accepts SVG-like chunks and rejects others.
 */

#include <stdio.h>
#include <string.h>

#include "src/chunk.h"
#include "src/loader-librsvg.h"
#include "tests/loader/pixelformat_test_common.h"

int
test_loader_0020_loader_librsvg_detect_svg_like(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    sixel_chunk_t chunk;
    unsigned char const svg_with_bom[] =
        "\xef\xbb\xbf<svg xmlns='http://www.w3.org/2000/svg'/>";
    unsigned char const svg_with_xml[] =
        "<?xml version='1.0'?><svg xmlns='http://www.w3.org/2000/svg'/>";
    unsigned char const not_svg[] = "GIF89a";
    char long_prefix[4106];

    memset(&chunk, 0, sizeof(chunk));
    chunk.buffer = (unsigned char *)svg_with_bom;
    chunk.size = sizeof(svg_with_bom) - 1;
    if (!loader_can_try_librsvg(&chunk)) {
        fprintf(stderr, "BOM-prefixed SVG was not detected\n");
        return 1;
    }

    chunk.buffer = (unsigned char *)svg_with_xml;
    chunk.size = sizeof(svg_with_xml) - 1;
    if (!loader_can_try_librsvg(&chunk)) {
        fprintf(stderr, "XML-prefixed SVG was not detected\n");
        return 1;
    }

    chunk.buffer = (unsigned char *)not_svg;
    chunk.size = sizeof(not_svg) - 1;
    if (loader_can_try_librsvg(&chunk)) {
        fprintf(stderr, "non-SVG data was misdetected\n");
        return 1;
    }

    chunk.source_path = "sample.SVGZ";
    if (!loader_can_try_librsvg(&chunk)) {
        fprintf(stderr, ".svgz path hint was not detected\n");
        return 1;
    }
    chunk.source_path = NULL;

    memset(long_prefix, ' ', sizeof(long_prefix));
    long_prefix[4100] = '<';
    long_prefix[4101] = 's';
    long_prefix[4102] = 'v';
    long_prefix[4103] = 'g';
    long_prefix[4104] = '>';
    long_prefix[4105] = '\0';
    chunk.buffer = (unsigned char *)long_prefix;
    chunk.size = 4105;
    if (loader_can_try_librsvg(&chunk)) {
        fprintf(stderr, "SVG marker beyond probe limit was misdetected\n");
        return 1;
    }

    return 0;
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
