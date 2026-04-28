/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify librsvg loader output policy:
 * - preserve RGBA when background is not provided
 * - composite to RGB when background is provided
 *
 * This test also supports a dedicated setopt mode so TAP wrappers can
 * validate option-compatibility behavior separately from plain pixelformat
 * policy.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static SIXELSTATUS
new_librsvg_component(sixel_allocator_t *allocator,
                      void **ppcomponent)
{
    return create_loader_component_by_name("librsvg", allocator, ppcomponent);
}

static int
run_librsvg_pixelformat_test(void)
{
    int result;
    unsigned char white_bg[3];

    result = 0;
    white_bg[0] = 255u;
    white_bg[1] = 255u;
    white_bg[2] = 255u;

    result = run_loader_component_case_with_options_ex(
        "librsvg keeps rgba without bgcolor",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGBA8888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        0,
        256,
        NULL,
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "librsvg composites rgb with bgcolor",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGB888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        0,
        256,
        white_bg,
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    return 0;
}

static int
run_librsvg_setopt_compat_test(void)
{
    int result;
    unsigned char white_bg[3];

    result = 0;
    white_bg[0] = 255u;
    white_bg[1] = 255u;
    white_bg[2] = 255u;

    result = run_loader_component_case_with_options_ex(
        "librsvg use_palette/reqcolors are accepted as no-op",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGBA8888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        1,
        2,
        NULL,
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "librsvg bgcolor option is applied",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGB888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        0,
        256,
        white_bg,
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    return run_loader_component_case_with_options_ex(
        "librsvg bgcolor works even when palette options are set",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGB888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        1,
        2,
        white_bg,
        new_librsvg_component);
}

static int
run_librsvg_setopt_ignored_diag_test(void)
{
#if !HAVE_DEBUG
    return SIXEL_TEST_SKIP;
#else
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    int use_palette;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    component = NULL;
    use_palette = 1;
    message = NULL;
    result = 1;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }

    status = new_librsvg_component(allocator, (void **)&component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "librsvg component initialization failed\n");
        goto cleanup;
    }

    sixel_helper_set_additional_message(NULL);
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "librsvg setopt use_palette failed\n");
        goto cleanup;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
            strstr(message, "USE_PALETTE") == NULL ||
            strstr(message, "ignored") == NULL) {
        fprintf(stderr, "missing setopt ignored diagnostics\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_allocator_unref(allocator);
    return result;
#endif
}

static int
run_librsvg_loader_test_mode(char const *mode)
{
    if (mode == NULL || strcmp(mode, "pixelformat") == 0) {
        return run_librsvg_pixelformat_test();
    }
    if (strcmp(mode, "setopt") == 0) {
        return run_librsvg_setopt_compat_test();
    }
    if (strcmp(mode, "setopt_diag") == 0) {
        return run_librsvg_setopt_ignored_diag_test();
    }

    fprintf(stderr, "unknown librsvg pixelformat test mode: %s\n", mode);
    return 1;
}
#endif

int
test_loader_0023_loader_librsvg_pixelformat(int argc, char **argv)
{
#if HAVE_LIBRSVG
    if (argc > 1 && argv != NULL) {
        return run_librsvg_loader_test_mode(argv[1]);
    }
    return run_librsvg_loader_test_mode(NULL);
#else
    (void)argc;
    (void)argv;
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
