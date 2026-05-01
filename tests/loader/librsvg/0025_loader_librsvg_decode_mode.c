/*
 * Verify librsvg source selection through observable load behavior.
 */

#include <stdio.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/compat_stub.h"

#if HAVE_LIBRSVG
typedef struct librsvg_source_selection_case {
    char const *label;
    char const *relative_path;
    int replace_source_path;
    char const *source_path;
    int replace_buffer;
    unsigned char const *buffer;
    size_t buffer_size;
    int allow_relative_resources;
    int allow_stdin_svgz;
    int expect_success;
} librsvg_source_selection_case_t;

static SIXELSTATUS
set_librsvg_test_env(int allow_relative_resources, int allow_stdin_svgz)
{
    char const *relative_value;
    char const *stdin_value;

    relative_value = allow_relative_resources ? "1" : "0";
    stdin_value = allow_stdin_svgz ? "1" : "0";
    if (sixel_compat_setenv(
            "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES",
            relative_value) != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (sixel_compat_setenv("SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ",
                            stdin_value) != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
replace_chunk_source_path(sixel_chunk_t *chunk, char const *path)
{
    SIXELSTATUS status;
    sixel_chunk_bytes_view_t view;
    sixel_chunk_memory_request_t request;

    status = SIXEL_FALSE;
    view.bytes = NULL;
    view.size = 0u;
    if (chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = chunk->vtbl->get_bytes(chunk, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    request.bytes = view.bytes;
    request.size = view.size;
    request.source_path = path;
    return chunk->vtbl->init_memory(chunk, &request);
}

static SIXELSTATUS
replace_chunk_buffer(sixel_chunk_t *chunk,
                     unsigned char const *buffer,
                     size_t buffer_size)
{
    sixel_chunk_memory_request_t request;

    if (chunk == NULL || buffer == NULL || buffer_size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    request.bytes = buffer;
    request.size = buffer_size;
    request.source_path = sixel_chunk_get_source_path(chunk);
    return chunk->vtbl->init_memory(chunk, &request);
}

static int
load_librsvg_chunk(char const *label,
                   sixel_allocator_t *allocator,
                   sixel_chunk_t *chunk,
                   int expect_success)
{
    SIXELSTATUS status;
    sixel_loader_component_t *component;
    loader_probe_context_t context;
    loader_probe_callback_state_t callback_state;
    int result;

    status = SIXEL_FALSE;
    component = NULL;
    result = 1;
    context.callback_count = 0;
    context.pixelformat = 0;
    context.colorspace = 0;
    context.width = 0;
    context.height = 0;
    context.transparent = FRAME_METADATA_ANY;
    context.multiframe = FRAME_METADATA_ANY;
    context.alpha_zero_is_transparent = FRAME_ALPHA_ZERO_ANY;
    context.has_transparent_mask = FRAME_MASK_ANY;
    context.transparent_mask_size = 0u;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

    status = create_loader_component_by_name("librsvg",
                                             allocator,
                                             (void **)&component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: librsvg component init failed\n", label);
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (expect_success) {
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "%s: expected load success but got status=%d (%s)\n",
                    label,
                    (int)status,
                    sixel_helper_get_additional_message());
            goto cleanup;
        }
        if (context.callback_count <= 0) {
            fprintf(stderr, "%s: load did not emit a frame\n", label);
            goto cleanup;
        }
    } else if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "%s: expected load failure\n", label);
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    return result;
}

static int
run_librsvg_source_selection_case(
    librsvg_source_selection_case_t const *test_case)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
#if defined(_MSC_VER)
    char *source_root_dupe;
    size_t source_root_len;
#endif
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
#if defined(_MSC_VER)
    source_root_dupe = NULL;
    source_root_len = 0u;
#endif
    cancel_flag = 0;
    result = 1;
    if (test_case == NULL || test_case->label == NULL ||
        test_case->relative_path == NULL) {
        return 1;
    }

#if defined(_MSC_VER)
    _dupenv_s(&source_root_dupe, &source_root_len, "MESON_SOURCE_ROOT");
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "abs_top_srcdir");
    }
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "TOP_SRCDIR");
    }
    if (source_root_dupe != NULL) {
        source_root = source_root_dupe;
    }
#else
    source_root = sixel_compat_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("TOP_SRCDIR");
    }
#endif
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         test_case->relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n",
                test_case->label);
        goto cleanup;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n",
                test_case->label);
        goto cleanup;
    }

    status = sixel_chunk_create_from_source(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", test_case->label);
        goto cleanup;
    }

    if (test_case->replace_source_path) {
        status = replace_chunk_source_path(chunk, test_case->source_path);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "%s: failed to replace source path\n",
                    test_case->label);
            goto cleanup;
        }
    }
    if (test_case->replace_buffer) {
        status = replace_chunk_buffer(chunk,
                                      test_case->buffer,
                                      test_case->buffer_size);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "%s: failed to replace chunk buffer\n",
                    test_case->label);
            goto cleanup;
        }
    }

    status = set_librsvg_test_env(test_case->allow_relative_resources,
                                  test_case->allow_stdin_svgz);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to set librsvg policy env\n",
                test_case->label);
        goto cleanup;
    }

    result = load_librsvg_chunk(test_case->label,
                                allocator,
                                chunk,
                                test_case->expect_success);

cleanup:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
#if defined(_MSC_VER)
    free(source_root_dupe);
#endif
    return result;
}
#endif

int
test_loader_0025_loader_librsvg_decode_mode(int argc, char **argv)
{
#if HAVE_LIBRSVG
    unsigned char const malformed_svg[] = "not svg";
    unsigned char const malformed_svgz[] = { 0x1f, 0x8b, 0x08, 0xff };
    librsvg_source_selection_case_t const cases[] = {
        {
            "local svg default uses chunk data",
            "/tests/data/inputs/formats/librsvg-default-size.svg",
            0, NULL,
            1, malformed_svg, sizeof(malformed_svg) - 1u,
            0, 0, 0
        },
        {
            "local svg relative opt-in uses source file",
            "/tests/data/inputs/formats/librsvg-default-size.svg",
            0, NULL,
            1, malformed_svg, sizeof(malformed_svg) - 1u,
            1, 0, 1
        },
        {
            "file uri svg remains data backed",
            "/tests/data/inputs/formats/librsvg-default-size.svg",
            1, "file:///tmp/libsixel-librsvg-missing.svg",
            0, NULL, 0u,
            1, 0, 1
        },
        {
            "empty source path remains data backed",
            "/tests/data/inputs/formats/librsvg-default-size.svg",
            1, "",
            0, NULL, 0u,
            1, 0, 1
        },
        {
            "local svgz uses source file",
            "/tests/data/inputs/formats/librsvg-transparent-2color.svgz",
            0, NULL,
            1, malformed_svgz, sizeof(malformed_svgz),
            0, 0, 1
        },
        {
            "stdin svgz default is rejected",
            "/tests/data/inputs/formats/librsvg-transparent-2color.svgz",
            1, "-",
            0, NULL, 0u,
            0, 0, 0
        },
        {
            "stdin svgz opt-in uses tempfile",
            "/tests/data/inputs/formats/librsvg-transparent-2color.svgz",
            1, "-",
            0, NULL, 0u,
            0, 1, 1
        },
        {
            "non-local svgz opt-in uses tempfile",
            "/tests/data/inputs/formats/librsvg-transparent-2color.svgz",
            1, "https://example.com/image.svgz",
            0, NULL, 0u,
            1, 1, 1
        },
        {
            "memory svg uses chunk data",
            "/tests/data/inputs/formats/librsvg-default-size.svg",
            1, NULL,
            0, NULL, 0u,
            1, 0, 1
        }
    };
    size_t index;
    int result;
#endif

    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    result = 0;
    index = 0u;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        result = run_librsvg_source_selection_case(&cases[index]);
        if (result != 0) {
            break;
        }
    }
    (void)set_librsvg_test_env(0, 0);
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
