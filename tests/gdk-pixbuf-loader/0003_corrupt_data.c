/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <string.h>

G_MODULE_EXPORT void fill_info(GdkPixbufFormat *info);
G_MODULE_EXPORT void fill_vtable(GdkPixbufModule *module);

static unsigned char const invalid_sixel_data[] = "not a sixel";

static void
corrupt_data_test(void)
{
    GdkPixbufModule module;
    GdkPixbufFormat format;
    gpointer context;
    GError *error;
    gboolean ok;

    memset(&module, 0, sizeof(module));
    memset(&format, 0, sizeof(format));
    error = NULL;

    fill_info(&format);
    fill_vtable(&module);

    context = module.begin_load(NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull(context);
    g_assert_no_error(error);

    ok = module.load_increment(context,
                               invalid_sixel_data,
                               (guint)strlen((char const *)invalid_sixel_data),
                               &error);
    g_assert_true(ok);
    g_assert_no_error(error);

    ok = module.stop_load(context, &error);
    g_assert_false(ok);
    g_assert_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE);
    g_assert_nonnull(error->message);

    g_clear_error(&error);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/corrupt/data", corrupt_data_test);

    return g_test_run();
}
