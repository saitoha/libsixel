/*
 * Verify GD loader status policy for delegate/target/error paths.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "src/chunk.h"
#include "src/loader-gd.h"
#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static char const *
loader_test_source_root(void)
{
    char const *source_root;

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    return source_root;
}

static int
load_chunk_from_relative(sixel_allocator_t *allocator,
                         char const *relative_path,
                         sixel_chunk_t **out_chunk)
{
    SIXELSTATUS status;
    char image_path[PATH_MAX];
    int cancel_flag;

    status = SIXEL_FALSE;
    cancel_flag = 0;
    if (allocator == NULL || relative_path == NULL || out_chunk == NULL) {
        return 1;
    }

    *out_chunk = NULL;
    if (build_image_path(loader_test_source_root(),
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        return 1;
    }

    status = sixel_chunk_new(out_chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);

    return SIXEL_FAILED(status) ? 1 : 0;
}

static SIXELSTATUS
run_load_with_gd_status(sixel_chunk_t const *chunk)
{
    loader_probe_context_t context;
    int lut_ready;
    double lut[256];

    memset(&context, 0, sizeof(context));
    lut_ready = 0;
    memset(lut, 0, sizeof(lut));

    return load_with_gd(chunk,
                        0,
                        SIXEL_PALETTE_MAX,
                        NULL,
                        &lut_ready,
                        lut,
                        capture_frame,
                        &context);
}

static int
expect_status_for_buffer(sixel_allocator_t *allocator,
                         unsigned char const *buffer,
                         size_t size,
                         SIXELSTATUS expected,
                         char const *label)
{
    sixel_chunk_t chunk;
    SIXELSTATUS status;

    memset(&chunk, 0, sizeof(chunk));
    chunk.buffer = (unsigned char *)buffer;
    chunk.size = size;
    chunk.allocator = allocator;
    status = run_load_with_gd_status(&chunk);
    if (status != expected) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)expected);
        return 1;
    }

    return 0;
}

static int
expect_status_for_file(sixel_allocator_t *allocator,
                       char const *relative_path,
                       SIXELSTATUS expected,
                       char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;

    chunk = NULL;
    status = SIXEL_FALSE;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }

    status = run_load_with_gd_status(chunk);
    sixel_chunk_destroy(chunk);
    if (status != expected) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)expected);
        return 1;
    }

    return 0;
}

static int
expect_truncated_status_for_file(sixel_allocator_t *allocator,
                                 char const *relative_path,
                                 size_t truncated_size,
                                 SIXELSTATUS expected,
                                 char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;

    chunk = NULL;
    status = SIXEL_FALSE;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }
    if (chunk->size <= truncated_size) {
        fprintf(stderr, "%s: sample is too small for truncation\n", label);
        sixel_chunk_destroy(chunk);
        return 1;
    }

    chunk->size = truncated_size;
    status = run_load_with_gd_status(chunk);
    sixel_chunk_destroy(chunk);
    if (status != expected) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)expected);
        return 1;
    }

    return 0;
}

static int
expect_optional_format_status(sixel_allocator_t *allocator,
                              char const *format_label,
                              char const *relative_path,
                              size_t truncated_size)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;

    chunk = NULL;
    status = SIXEL_FALSE;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", format_label);
        return 1;
    }

    status = run_load_with_gd_status(chunk);
    if (status != SIXEL_OK) {
        sixel_chunk_destroy(chunk);
        return 0;
    }
    if (chunk->size <= truncated_size) {
        sixel_chunk_destroy(chunk);
        return 0;
    }

    chunk->size = truncated_size;
    status = run_load_with_gd_status(chunk);
    sixel_chunk_destroy(chunk);
    if (status != SIXEL_GD_ERROR) {
        fprintf(stderr,
                "%s: expected SIXEL_GD_ERROR for truncated data (got=%d)\n",
                format_label,
                (int)status);
        return 1;
    }

    return 0;
}
#endif

int
test_loader_0058_loader_gd_status_policy(int argc, char **argv)
{
#if HAVE_GD
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
    static unsigned char const png_apng_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x08u, 'a', 'c', 'T', 'L',
        0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const unknown_data[] = {
        'N', 'O', 'T', '_', 'I', 'M', 'G'
    };
    static unsigned char const wbmp_oversize_data[] = {
        0x00u, 0x00u, 0xffu, 0xffu, 0x7fu, 0xffu, 0x7fu, 0x00u
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    result = 0;
    (void)argc;
    (void)argv;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }

    if (expect_status_for_buffer(allocator,
                                 gif_data,
                                 sizeof(gif_data),
                                 SIXEL_FALSE,
                                 "delegate-gif") != 0) {
        result = 1;
    }
    if (expect_status_for_buffer(allocator,
                                 png_apng_data,
                                 sizeof(png_apng_data),
                                 SIXEL_FALSE,
                                 "delegate-apng") != 0) {
        result = 1;
    }
    if (expect_status_for_buffer(allocator,
                                 png_interlaced_data,
                                 sizeof(png_interlaced_data),
                                 SIXEL_FALSE,
                                 "delegate-interlaced-png") != 0) {
        result = 1;
    }
    if (expect_status_for_buffer(allocator,
                                 unknown_data,
                                 sizeof(unknown_data),
                                 SIXEL_FALSE,
                                 "delegate-unknown") != 0) {
        result = 1;
    }

    if (expect_status_for_file(allocator,
                               "/tests/data/inputs/formats/rgb.png",
                               SIXEL_OK,
                               "decode-png") != 0) {
        result = 1;
    }
    if (expect_status_for_file(allocator,
                               "/tests/data/inputs/formats/snake-jpeg-444.jpg",
                               SIXEL_OK,
                               "decode-jpeg") != 0) {
        result = 1;
    }
    if (expect_status_for_file(allocator,
                               "/tests/data/inputs/formats/snake-bmp3-rgb.bmp",
                               SIXEL_OK,
                               "decode-bmp") != 0) {
        result = 1;
    }

    if (expect_truncated_status_for_file(
            allocator,
            "/tests/data/inputs/formats/rgb.png",
            64u,
            SIXEL_GD_ERROR,
            "error-truncated-png") != 0) {
        result = 1;
    }
    if (expect_status_for_file(allocator,
                               "/tests/data/corrupted/invalid_marker.jpg",
                               SIXEL_GD_ERROR,
                               "error-corrupt-jpeg") != 0) {
        result = 1;
    }
    if (expect_truncated_status_for_file(
            allocator,
            "/tests/data/inputs/formats/snake-bmp3-rgb.bmp",
            64u,
            SIXEL_GD_ERROR,
            "error-truncated-bmp") != 0) {
        result = 1;
    }

    if (expect_optional_format_status(
            allocator,
            "optional-tiff",
            "/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff",
            64u) != 0) {
        result = 1;
    }
    if (expect_optional_format_status(
            allocator,
            "optional-tga",
            "/tests/data/inputs/formats/snake-tga-type2-rgb.tga",
            64u) != 0) {
        result = 1;
    }
    if (expect_status_for_file(allocator,
                               "/tests/data/inputs/formats/"
                               "snake-wbmp-bilevel.wbmp",
                               SIXEL_OK,
                               "optional-wbmp-valid") == 0) {
        if (expect_status_for_buffer(allocator,
                                     wbmp_oversize_data,
                                     sizeof(wbmp_oversize_data),
                                     SIXEL_GD_ERROR,
                                     "optional-wbmp-oversize") != 0) {
            result = 1;
        }
    }
    if (expect_optional_format_status(
            allocator,
            "optional-gd2",
            "/tests/data/inputs/formats/sample-gd2-conv_test.gd2",
            64u) != 0) {
        result = 1;
    }
    if (expect_optional_format_status(
            allocator,
            "optional-webp",
            "/tests/data/inputs/snake_64.webp",
            64u) != 0) {
        result = 1;
    }

    sixel_allocator_unref(allocator);
    return result;
#else
    (void)argc;
    (void)argv;
    fprintf(stderr, "GD loader unavailable\n");
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
