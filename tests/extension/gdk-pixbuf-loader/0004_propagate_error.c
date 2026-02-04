/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_GDK_PIXBUF2)

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <sixel.h>
#include <string.h>

G_MODULE_EXPORT gboolean sixel_pixbuf_testing_propagate_error(GError **error,
                                                              SIXELSTATUS status,
                                                              char const *message);

static void
propagate_error_test(void)
{
    struct {
        SIXELSTATUS status;
        int expected_code;
    } cases[] = {
        { SIXEL_BAD_INPUT, GDK_PIXBUF_ERROR_CORRUPT_IMAGE },
        { SIXEL_BAD_ALLOCATION, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY },
        { SIXEL_BAD_ARGUMENT, GDK_PIXBUF_ERROR_FAILED },
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(cases); ++i) {
        GError *error;
        gboolean ok;

        error = NULL;
        ok = sixel_pixbuf_testing_propagate_error(&error,
                                                  cases[i].status,
                                                  "sixel loader: test message");
        g_assert_false(ok);
        g_assert_error(error, GDK_PIXBUF_ERROR, cases[i].expected_code);
        g_assert_nonnull(error->message);
        g_assert_nonnull(strstr(error->message, "sixel loader"));

        g_clear_error(&error);
    }
}

int
test_gdk_pixbuf_loader_0004_propagate_error(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/propagate/error", propagate_error_test);

    return g_test_run();
}
#else

int
test_gdk_pixbuf_loader_0004_propagate_error(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    /* Skip when gdk-pixbuf loader support is unavailable. */
    return 77;
}

#endif
