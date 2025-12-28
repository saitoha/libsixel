/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>

G_MODULE_EXPORT void sixel_pixbuf_testing_context_free(gpointer context_ptr);
G_MODULE_EXPORT void fill_vtable(GdkPixbufModule *module);
G_MODULE_EXPORT void fill_info(GdkPixbufFormat *info);

static void
context_free_test(void)
{
    GdkPixbufModule module;
    GdkPixbufFormat format;
    gpointer context;
    GError *error;

    memset(&module, 0, sizeof(module));
    memset(&format, 0, sizeof(format));
    error = NULL;

    fill_info(&format);
    fill_vtable(&module);

    context = module.begin_load(NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull(context);
    g_assert_no_error(error);

    sixel_pixbuf_testing_context_free(context);

    context = NULL;
    sixel_pixbuf_testing_context_free(context);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/context/free", context_free_test);

    return g_test_run();
}
