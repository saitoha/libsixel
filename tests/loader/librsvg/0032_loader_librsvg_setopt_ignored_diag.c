/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the debug diagnostic emitted for an accepted but ignored librsvg
 * palette option.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG && HAVE_DEBUG
static SIXELSTATUS
new_librsvg_diag_component(sixel_allocator_t *allocator,
                           void **component_out)
{
    return create_loader_component_by_name("librsvg",
                                           allocator,
                                           component_out);
}

static int
run_librsvg_setopt_ignored_diag_test(void)
{
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
    status = new_librsvg_diag_component(allocator, (void **)&component);
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
}
#endif

int
test_loader_0032_loader_librsvg_setopt_ignored_diag(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if HAVE_LIBRSVG && HAVE_DEBUG
    return run_librsvg_setopt_ignored_diag_test();
#else
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
