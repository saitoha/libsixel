/*
 * Verify GD can_try policy for delegated and accepted formats.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/chunk.h"
#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static int
new_gd_component(sixel_allocator_t *allocator, void **ppcomponent)
{
    return create_loader_component_by_name("gd", allocator, ppcomponent);
}

static int
expect_can_try(sixel_loader_component_t *loader,
               unsigned char const *buffer,
               size_t size,
               int expected,
               char const *label)
{
    sixel_chunk_t chunk;
    int actual;

    memset(&chunk, 0, sizeof(chunk));
    chunk.buffer = (unsigned char *)buffer;
    chunk.size = size;
    actual = sixel_loader_component_predicate(loader, &chunk);
    if (actual != expected) {
        fprintf(stderr,
                "gd loader predicate mismatch for %s: got=%d expected=%d\n",
                label,
                actual,
                expected);
        return 1;
    }

    return 0;
}

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
run_gd_component_status(sixel_loader_component_t *loader,
                        sixel_chunk_t const *chunk)
{
    loader_probe_context_t context;

    memset(&context, 0, sizeof(context));
    return sixel_loader_component_load(loader,
                                       chunk,
                                       capture_frame,
                                       &context);
}

static int
expect_optional_can_try_consistency(sixel_loader_component_t *loader,
                                    sixel_allocator_t *allocator,
                                    char const *label,
                                    char const *relative_path)
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

    can_try = sixel_loader_component_predicate(loader, chunk);
    status = run_gd_component_status(loader, chunk);
    sixel_chunk_destroy(chunk);

    if (can_try == 0 && status != SIXEL_FALSE) {
        fprintf(stderr,
                "%s: can_try=0 but status=%d\n",
                label,
                (int)status);
        return 1;
    }
    if (can_try != 0 && status == SIXEL_FALSE) {
        fprintf(stderr, "%s: can_try=1 but status=SIXEL_FALSE\n", label);
        return 1;
    }

    return 0;
}

static int
run_can_try_policy_mode(char const *mode)
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
    static unsigned char const png_signature_only_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const png_bad_ihdr_len_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0cu, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x06u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const png_highdepth_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x10u, 0x00u, 0x00u, 0x00u, 0x00u,
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
    static unsigned char const jpeg_data[] = {
        0xffu, 0xd8u, 0xffu, 0xe0u, 0x00u, 0x10u, 'J', 'F', 'I', 'F'
    };
    static unsigned char const unknown_data[] = {
        'N', 'O', 'T', '_', 'I', 'M', 'G'
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    int run_all;
    int matched;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    loader = NULL;
    run_all = 0;
    matched = 0;
    result = 0;
    run_all = mode == NULL || strcmp(mode, "all") == 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }
    status = new_gd_component(allocator, (void **)&loader);
    if (SIXEL_FAILED(status) || loader == NULL) {
        sixel_allocator_unref(allocator);
        fprintf(stderr, "GD loader unavailable\n");
        return SIXEL_TEST_SKIP;
    }

    if (run_all || strcmp(mode, "null_chunk_rejected") == 0) {
        matched = 1;
        if (sixel_loader_component_predicate(loader, NULL) != 0) {
            fprintf(stderr, "gd loader predicate should reject null chunk\n");
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "gif_delegate_false") == 0) {
        matched = 1;
        if (expect_can_try(loader, gif_data, sizeof(gif_data), 0, "gif") !=
                0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "apng_delegate_false") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_apng_data,
                           sizeof(png_apng_data),
                           0,
                           "png-apng") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "interlaced_png_delegate_false") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_interlaced_data,
                           sizeof(png_interlaced_data),
                           0,
                           "png-interlaced") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "png_plain_true") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_plain_data,
                           sizeof(png_plain_data),
                           1,
                           "png-plain") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "png_signature_only_true") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_signature_only_data,
                           sizeof(png_signature_only_data),
                           1,
                           "png-signature-only") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "png_bad_ihdr_len_can_try_true") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_bad_ihdr_len_data,
                           sizeof(png_bad_ihdr_len_data),
                           1,
                           "png-bad-ihdr-len") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "png_highdepth_true") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           png_highdepth_data,
                           sizeof(png_highdepth_data),
                           1,
                           "png-highdepth") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "jpeg_true") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           jpeg_data,
                           sizeof(jpeg_data),
                           1,
                           "jpeg") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "unknown_false") == 0) {
        matched = 1;
        if (expect_can_try(loader,
                           unknown_data,
                           sizeof(unknown_data),
                           0,
                           "unknown") != 0) {
            result = 1;
        }
    }

    if (run_all || strcmp(mode, "optional_tiff_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-tiff",
                "/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_tga_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-tga",
                "/tests/data/inputs/formats/snake-tga-type2-rgb.tga") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_wbmp_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-wbmp",
                "/tests/data/inputs/formats/snake-wbmp-bilevel.wbmp") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_gd2_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-gd2",
                "/tests/data/inputs/formats/sample-gd2-conv_test.gd2") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_webp_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-webp",
                "/tests/data/inputs/snake_64.webp") != 0) {
            result = 1;
        }
    }
    if (run_all || strcmp(mode, "optional_matrix_consistency") == 0) {
        matched = 1;
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-matrix-tiff",
                "/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-matrix-tga",
                "/tests/data/inputs/formats/snake-tga-type2-rgb.tga") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-matrix-wbmp",
                "/tests/data/inputs/formats/snake-wbmp-bilevel.wbmp") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-matrix-gd2",
                "/tests/data/inputs/formats/sample-gd2-conv_test.gd2") != 0) {
            result = 1;
        }
        if (expect_optional_can_try_consistency(
                loader,
                allocator,
                "optional-matrix-webp",
                "/tests/data/inputs/snake_64.webp") != 0) {
            result = 1;
        }
    }

    sixel_loader_component_unref(loader);
    sixel_allocator_unref(allocator);
    if (matched == 0) {
        fprintf(stderr, "unknown gd can_try mode: %s\n", mode);
        return 1;
    }

    return result;
}
#endif

int
test_loader_0057_loader_gd_can_try_policy(int argc, char **argv)
{
#if HAVE_GD
    if (argc > 1 && argv != NULL) {
        return run_can_try_policy_mode(argv[1]);
    }
    return run_can_try_policy_mode("all");
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
