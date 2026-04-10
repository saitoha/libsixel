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

static SIXELSTATUS
run_load_with_gd_status_with_lut(sixel_chunk_t const *chunk,
                                 unsigned char const *bgcolor,
                                 int *lut_ready,
                                 double lut[256])
{
    loader_probe_context_t context;

    memset(&context, 0, sizeof(context));
    return load_with_gd(chunk,
                        0,
                        SIXEL_PALETTE_MAX,
                        (unsigned char *)bgcolor,
                        lut_ready,
                        lut,
                        capture_frame,
                        &context);
}

static SIXELSTATUS
run_load_with_gd_status_and_lut_state(sixel_chunk_t const *chunk,
                                      unsigned char const *bgcolor,
                                      int *lut_ready_after)
{
    int lut_ready;
    double lut[256];
    SIXELSTATUS status;

    lut_ready = 0;
    memset(lut, 0, sizeof(lut));
    status = run_load_with_gd_status_with_lut(chunk,
                                              bgcolor,
                                              &lut_ready,
                                              lut);
    if (lut_ready_after != NULL) {
        *lut_ready_after = lut_ready;
    }
    return status;
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

static int
expect_transfer_cache_status_for_file(sixel_allocator_t *allocator,
                                      char const *relative_path,
                                      unsigned char const *bgcolor,
                                      int expected_lut_ready,
                                      char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int lut_ready;

    chunk = NULL;
    status = SIXEL_FALSE;
    lut_ready = 0;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }

    status = run_load_with_gd_status_and_lut_state(chunk,
                                                    bgcolor,
                                                    &lut_ready);
    sixel_chunk_destroy(chunk);
    if (status != SIXEL_OK) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)SIXEL_OK);
        return 1;
    }
    if (lut_ready != expected_lut_ready) {
        fprintf(stderr,
                "%s: lut-ready mismatch (got=%d expected=%d)\n",
                label,
                lut_ready,
                expected_lut_ready);
        return 1;
    }

    return 0;
}

static int
expect_transfer_cache_reused_for_file(sixel_allocator_t *allocator,
                                      char const *relative_path,
                                      char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int lut_ready;
    double lut[256];

    chunk = NULL;
    status = SIXEL_FALSE;
    lut_ready = 0;
    memset(lut, 0, sizeof(lut));
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }

    status = run_load_with_gd_status_with_lut(chunk, NULL, &lut_ready, lut);
    if (status != SIXEL_OK || lut_ready != 1) {
        fprintf(stderr,
                "%s: first decode mismatch (status=%d lut=%d)\n",
                label,
                (int)status,
                lut_ready);
        sixel_chunk_destroy(chunk);
        return 1;
    }

    status = run_load_with_gd_status_with_lut(chunk, NULL, &lut_ready, lut);
    sixel_chunk_destroy(chunk);
    if (status != SIXEL_OK || lut_ready != 1) {
        fprintf(stderr,
                "%s: second decode mismatch (status=%d lut=%d)\n",
                label,
                (int)status,
                lut_ready);
        return 1;
    }

    return 0;
}

static int
expect_can_try_status_for_buffer(sixel_allocator_t *allocator,
                                 unsigned char const *buffer,
                                 size_t size,
                                 int expected_can_try,
                                 SIXELSTATUS expected_status,
                                 char const *label)
{
    sixel_chunk_t chunk;
    SIXELSTATUS status;
    int can_try;

    memset(&chunk, 0, sizeof(chunk));
    chunk.buffer = (unsigned char *)buffer;
    chunk.size = size;
    chunk.allocator = allocator;
    can_try = loader_can_try_gd(&chunk);
    status = run_load_with_gd_status(&chunk);
    if (can_try != expected_can_try) {
        fprintf(stderr,
                "%s: can_try mismatch (got=%d expected=%d)\n",
                label,
                can_try,
                expected_can_try);
        return 1;
    }
    if (status != expected_status) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)expected_status);
        return 1;
    }

    return 0;
}

static int
expect_can_try_status_for_file(sixel_allocator_t *allocator,
                               char const *relative_path,
                               int expected_can_try,
                               SIXELSTATUS expected_status,
                               char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int can_try;

    chunk = NULL;
    status = SIXEL_FALSE;
    can_try = 0;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }

    can_try = loader_can_try_gd(chunk);
    status = run_load_with_gd_status(chunk);
    sixel_chunk_destroy(chunk);
    if (can_try != expected_can_try) {
        fprintf(stderr,
                "%s: can_try mismatch (got=%d expected=%d)\n",
                label,
                can_try,
                expected_can_try);
        return 1;
    }
    if (status != expected_status) {
        fprintf(stderr,
                "%s: status mismatch (got=%d expected=%d)\n",
                label,
                (int)status,
                (int)expected_status);
        return 1;
    }

    return 0;
}

static int
expect_optional_can_try_status_for_file(sixel_allocator_t *allocator,
                                        char const *relative_path,
                                        char const *label)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int can_try;

    chunk = NULL;
    status = SIXEL_FALSE;
    can_try = 0;
    if (load_chunk_from_relative(allocator, relative_path, &chunk) != 0) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        return 1;
    }

    can_try = loader_can_try_gd(chunk);
    status = run_load_with_gd_status(chunk);
    sixel_chunk_destroy(chunk);
    if (can_try == 0 && status != SIXEL_FALSE) {
        fprintf(stderr,
                "%s: can_try=0 but status=%d\n",
                label,
                (int)status);
        return 1;
    }
    if (can_try != 0 && status == SIXEL_FALSE) {
        fprintf(stderr,
                "%s: can_try=1 but status=SIXEL_FALSE\n",
                label);
        return 1;
    }

    return 0;
}

static int
expect_optional_wbmp_status(sixel_allocator_t *allocator)
{
    static unsigned char const wbmp_oversize_data[] = {
        0x00u, 0x00u, 0xffu, 0xffu, 0x7fu, 0xffu, 0x7fu, 0x00u
    };

    if (expect_status_for_file(allocator,
                               "/tests/data/inputs/formats/"
                               "snake-wbmp-bilevel.wbmp",
                               SIXEL_OK,
                               "optional-wbmp-valid") != 0) {
        return 0;
    }
    if (expect_status_for_buffer(allocator,
                                 wbmp_oversize_data,
                                 sizeof(wbmp_oversize_data),
                                 SIXEL_GD_ERROR,
                                 "optional-wbmp-oversize") != 0) {
        return 1;
    }

    return 0;
}

static int
run_status_policy_mode(char const *mode)
{
    static unsigned char const white_bg[] = { 0xffu, 0xffu, 0xffu };
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
    static unsigned char const png_signature_only_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const png_bad_ihdr_len_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0cu, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x06u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    int run_all;
    int matched;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    run_all = 0;
    matched = 0;
    result = 0;
    run_all = mode == NULL || strcmp(mode, "all") == 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }

    if (run_all || strcmp(mode, "delegate_gif_false") == 0) {
        matched = 1;
        if (expect_status_for_buffer(allocator,
                                     gif_data,
                                     sizeof(gif_data),
                                     SIXEL_FALSE,
                                     "delegate-gif") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "delegate_apng_false") == 0) {
        matched = 1;
        if (expect_status_for_buffer(allocator,
                                     png_apng_data,
                                     sizeof(png_apng_data),
                                     SIXEL_FALSE,
                                     "delegate-apng") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "delegate_interlaced_png_false") == 0) {
        matched = 1;
        if (expect_status_for_buffer(allocator,
                                     png_interlaced_data,
                                     sizeof(png_interlaced_data),
                                     SIXEL_FALSE,
                                     "delegate-interlaced-png") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "delegate_unknown_false") == 0) {
        matched = 1;
        if (expect_status_for_buffer(allocator,
                                     unknown_data,
                                     sizeof(unknown_data),
                                     SIXEL_FALSE,
                                     "delegate-unknown") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "can_try_status_core_matrix") == 0) {
        matched = 1;
        if (expect_can_try_status_for_buffer(allocator,
                                             gif_data,
                                             sizeof(gif_data),
                                             0,
                                             SIXEL_FALSE,
                                             "matrix-gif") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_buffer(allocator,
                                             png_apng_data,
                                             sizeof(png_apng_data),
                                             0,
                                             SIXEL_FALSE,
                                             "matrix-apng") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_buffer(allocator,
                                             png_interlaced_data,
                                             sizeof(png_interlaced_data),
                                             0,
                                             SIXEL_FALSE,
                                             "matrix-interlaced-png") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_buffer(allocator,
                                             unknown_data,
                                             sizeof(unknown_data),
                                             0,
                                             SIXEL_FALSE,
                                             "matrix-unknown") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_buffer(allocator,
                                             png_signature_only_data,
                                             sizeof(png_signature_only_data),
                                             1,
                                             SIXEL_GD_ERROR,
                                             "matrix-png-signature-only") !=
                0) {
            result = 1;
        }
        if (expect_can_try_status_for_buffer(allocator,
                                             png_bad_ihdr_len_data,
                                             sizeof(png_bad_ihdr_len_data),
                                             1,
                                             SIXEL_GD_ERROR,
                                             "matrix-png-bad-ihdr-len") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_file(allocator,
                                           "/tests/data/inputs/formats/rgb.png",
                                           1,
                                           SIXEL_OK,
                                           "matrix-decode-png") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-jpeg-444.jpg",
                1,
                SIXEL_OK,
                "matrix-decode-jpeg") != 0) {
            result = 1;
        }
        if (expect_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-bmp3-rgb.bmp",
                1,
                SIXEL_OK,
                "matrix-decode-bmp") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "can_try_status_optional_matrix") == 0) {
        matched = 1;
        if (expect_optional_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff",
                "optional-matrix-tiff") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-tga-type2-rgb.tga",
                "optional-matrix-tga") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-wbmp-bilevel.wbmp",
                "optional-matrix-wbmp") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/formats/sample-gd2-conv_test.gd2",
                "optional-matrix-gd2") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_status_for_file(
                allocator,
                "/tests/data/inputs/snake_64.webp",
                "optional-matrix-webp") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "png_bad_ihdr_len_status_gd_error") == 0) {
        matched = 1;
        if (expect_status_for_buffer(allocator,
                                     png_bad_ihdr_len_data,
                                     sizeof(png_bad_ihdr_len_data),
                                     SIXEL_GD_ERROR,
                                     "png-bad-ihdr-len") != 0) {
            result = 1;
        }
    }

    if (run_all || strcmp(mode, "decode_png_ok") == 0) {
        matched = 1;
        if (expect_status_for_file(allocator,
                                   "/tests/data/inputs/formats/rgb.png",
                                   SIXEL_OK,
                                   "decode-png") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "decode_jpeg_ok") == 0) {
        matched = 1;
        if (expect_status_for_file(allocator,
                                   "/tests/data/inputs/formats/"
                                   "snake-jpeg-444.jpg",
                                   SIXEL_OK,
                                   "decode-jpeg") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "decode_bmp_ok") == 0) {
        matched = 1;
        if (expect_status_for_file(allocator,
                                   "/tests/data/inputs/formats/"
                                   "snake-bmp3-rgb.bmp",
                                   SIXEL_OK,
                                   "decode-bmp") != 0) {
            result = 1;
        }
    }

    if (run_all || strcmp(mode, "error_truncated_png") == 0) {
        matched = 1;
        if (expect_truncated_status_for_file(
                allocator,
                "/tests/data/inputs/formats/rgb.png",
                64u,
                SIXEL_GD_ERROR,
                "error-truncated-png") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "error_corrupt_jpeg") == 0) {
        matched = 1;
        if (expect_status_for_file(allocator,
                                   "/tests/data/corrupted/"
                                   "invalid_marker.jpg",
                                   SIXEL_GD_ERROR,
                                   "error-corrupt-jpeg") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "error_truncated_bmp") == 0) {
        matched = 1;
        if (expect_truncated_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake-bmp3-rgb.bmp",
                64u,
                SIXEL_GD_ERROR,
                "error-truncated-bmp") != 0) {
            result = 1;
        }
    }

    if (run_all || strcmp(mode, "optional_tiff_policy") == 0) {
        matched = 1;
        if (expect_optional_format_status(
                allocator,
                "optional-tiff",
                "/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff",
                64u) != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_tga_policy") == 0) {
        matched = 1;
        if (expect_optional_format_status(
                allocator,
                "optional-tga",
                "/tests/data/inputs/formats/snake-tga-type2-rgb.tga",
                64u) != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_wbmp_policy") == 0) {
        matched = 1;
        if (expect_optional_wbmp_status(allocator) != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_gd2_policy") == 0) {
        matched = 1;
        if (expect_optional_format_status(
                allocator,
                "optional-gd2",
                "/tests/data/inputs/formats/sample-gd2-conv_test.gd2",
                64u) != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_webp_policy") == 0) {
        matched = 1;
        if (expect_optional_format_status(
                allocator,
                "optional-webp",
                "/tests/data/inputs/snake_64.webp",
                64u) != 0) {
            result = 1;
        }
    }

    if (run_all || strcmp(mode, "transfer_gama_only_no_srgb_cache") == 0) {
        matched = 1;
        if (expect_transfer_cache_status_for_file(
                allocator,
                "/tests/data/inputs/formats/pal8-trns-key0-gama-only.png",
                white_bg,
                0,
                "transfer-gama-only-no-srgb-cache") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "transfer_iccp_prefers_srgb_cache") == 0) {
        matched = 1;
        if (expect_transfer_cache_status_for_file(
                allocator,
                "/tests/data/inputs/formats/pal8-trns-key0-gama-icc.png",
                white_bg,
                1,
                "transfer-iccp-prioritizes-srgb-cache") != 0) {
            result = 1;
        }
    }
    if (run_all ||
            strcmp(mode, "transfer_srgb_only_initializes_cache") == 0) {
        matched = 1;
        if (expect_transfer_cache_status_for_file(
                allocator,
                "/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png",
                NULL,
                1,
                "transfer-srgb-only-initializes-cache") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "lut_cache_srgb_reused_across_calls") == 0) {
        matched = 1;
        if (expect_transfer_cache_reused_for_file(
                allocator,
                "/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png",
                "transfer-srgb-cache-reuse") != 0) {
            result = 1;
        }
    }
    if (run_all ||
            strcmp(mode, "lut_cache_gama_path_does_not_mark_srgb_ready") ==
            0) {
        matched = 1;
        if (expect_transfer_cache_status_for_file(
                allocator,
                "/tests/data/inputs/formats/pal8-trns-key0-gama-only.png",
                white_bg,
                0,
                "transfer-gama-path-keeps-srgb-cache-off") != 0) {
            result = 1;
        }
    }

    sixel_allocator_unref(allocator);
    if (matched == 0) {
        fprintf(stderr, "unknown gd status mode: %s\n", mode);
        return 1;
    }

    return result;
}
#endif

int
test_loader_0058_loader_gd_status_policy(int argc, char **argv)
{
#if HAVE_GD
    if (argc > 1 && argv != NULL) {
        return run_status_policy_mode(argv[1]);
    }
    return run_status_policy_mode("all");
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
