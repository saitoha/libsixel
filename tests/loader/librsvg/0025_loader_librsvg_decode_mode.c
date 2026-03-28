/*
 * Verify librsvg decode mode selection for file/data/stdin svgz flows.
 */

#include <stdio.h>
#include <string.h>

#include "src/loader-librsvg-test.h"
#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static int
expect_decode_mode(char const *label,
                   sixel_librsvg_decode_mode_t actual,
                   sixel_librsvg_decode_mode_t expected)
{
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr,
            "%s: expected mode=%d but got mode=%d\n",
            label,
            (int)expected,
            (int)actual);
    return 1;
}
#endif

int
test_loader_0025_loader_librsvg_decode_mode(int argc, char **argv)
{
    sixel_chunk_t chunk;
    unsigned char const svg_data[] =
        "<svg xmlns='http://www.w3.org/2000/svg'/>";
    unsigned char const svgz_data[] = { 0x1f, 0x8b, 0x08, 0x00 };
    int result;
    sixel_librsvg_decode_mode_t mode;

    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    result = 0;
    mode = SIXEL_LIBRSVG_DECODE_MODE_DATA;
    memset(&chunk, 0, sizeof(chunk));

    chunk.source_path = "sample.svg";
    chunk.buffer = (unsigned char *)svg_data;
    chunk.size = sizeof(svg_data) - 1u;
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 1, 0);
    result = expect_decode_mode("local svg with relative opt-in",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_FILE);
    if (result != 0) {
        return result;
    }

    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 0, 0);
    result = expect_decode_mode("local svg default data mode",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_DATA);
    if (result != 0) {
        return result;
    }

    chunk.source_path = "file:///tmp/sample.svg";
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 1, 0);
    result = expect_decode_mode("file URI svg remains data mode",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_DATA);
    if (result != 0) {
        return result;
    }

    chunk.source_path = "";
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 1, 0);
    result = expect_decode_mode("empty source path remains data mode",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_DATA);
    if (result != 0) {
        return result;
    }

    chunk.source_path = "sample.svgz";
    chunk.buffer = (unsigned char *)svgz_data;
    chunk.size = sizeof(svgz_data);
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 0, 0);
    result = expect_decode_mode("local .svgz path decode",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_FILE);
    if (result != 0) {
        return result;
    }

    chunk.source_path = "-";
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 0, 0);
    result = expect_decode_mode("stdin .svgz default reject",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED);
    if (result != 0) {
        return result;
    }

    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 0, 1);
    result = expect_decode_mode(
        "stdin .svgz opt-in tempfile",
        mode,
        SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE);
    if (result != 0) {
        return result;
    }

    chunk.source_path = "https://example.com/image.svgz";
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 1, 1);
    result = expect_decode_mode(
        "non-local .svgz with stdin opt-in",
        mode,
        SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE);
    if (result != 0) {
        return result;
    }

    chunk.source_path = NULL;
    chunk.buffer = (unsigned char *)svg_data;
    chunk.size = sizeof(svg_data) - 1u;
    mode = sixel_loader_librsvg_pick_decode_mode_for_test(&chunk, 1, 0);
    result = expect_decode_mode("memory svg falls back to data",
                                mode,
                                SIXEL_LIBRSVG_DECODE_MODE_DATA);
    if (result != 0) {
        return result;
    }

    mode = sixel_loader_librsvg_pick_decode_mode_for_test(NULL, 1, 1);
    return expect_decode_mode("null chunk defensive fallback",
                              mode,
                              SIXEL_LIBRSVG_DECODE_MODE_DATA);
#else
    (void)chunk;
    (void)svg_data;
    (void)svgz_data;
    (void)result;
    (void)mode;
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
