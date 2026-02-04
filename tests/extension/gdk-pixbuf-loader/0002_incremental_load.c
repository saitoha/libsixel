/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_GDK_PIXBUF2)

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <string.h>

G_MODULE_EXPORT void fill_info(GdkPixbufFormat *info);
G_MODULE_EXPORT void fill_vtable(GdkPixbufModule *module);

static const unsigned char incremental_sixel_data[] = {
    0x1b, 0x50, 0x71, 0x22, 0x31, 0x3b, 0x31, 0x3b, 0x32, 0x3b,
    0x38, 0x23, 0x30, 0x3b, 0x32, 0x3b, 0x31, 0x3b, 0x31, 0x3b,
    0x31, 0x23, 0x31, 0x3b, 0x32, 0x3b, 0x35, 0x3b, 0x39, 0x3b,
    0x31, 0x33, 0x23, 0x32, 0x3b, 0x32, 0x3b, 0x31, 0x36, 0x3b,
    0x32, 0x31, 0x3b, 0x32, 0x34, 0x23, 0x33, 0x3b, 0x32, 0x3b,
    0x32, 0x39, 0x3b, 0x33, 0x32, 0x3b, 0x33, 0x37, 0x23, 0x34,
    0x3b, 0x32, 0x3b, 0x34, 0x30, 0x3b, 0x34, 0x35, 0x3b, 0x34,
    0x38, 0x23, 0x35, 0x3b, 0x32, 0x3b, 0x35, 0x33, 0x3b, 0x35,
    0x36, 0x3b, 0x36, 0x30, 0x23, 0x36, 0x3b, 0x32, 0x3b, 0x36,
    0x34, 0x3b, 0x36, 0x38, 0x3b, 0x37, 0x31, 0x23, 0x37, 0x3b,
    0x32, 0x3b, 0x37, 0x36, 0x3b, 0x37, 0x39, 0x3b, 0x38, 0x34,
    0x23, 0x38, 0x3b, 0x32, 0x3b, 0x38, 0x37, 0x3b, 0x39, 0x32,
    0x3b, 0x39, 0x35, 0x23, 0x39, 0x3b, 0x32, 0x3b, 0x31, 0x30,
    0x30, 0x3b, 0x32, 0x3b, 0x32, 0x23, 0x31, 0x30, 0x3b, 0x32,
    0x3b, 0x32, 0x3b, 0x31, 0x30, 0x30, 0x3b, 0x32, 0x23, 0x31,
    0x31, 0x3b, 0x32, 0x3b, 0x32, 0x3b, 0x32, 0x3b, 0x31, 0x30,
    0x30, 0x23, 0x31, 0x32, 0x3b, 0x32, 0x3b, 0x31, 0x30, 0x30,
    0x3b, 0x31, 0x30, 0x30, 0x3b, 0x32, 0x23, 0x31, 0x33, 0x3b,
    0x32, 0x3b, 0x31, 0x3b, 0x32, 0x3b, 0x32, 0x23, 0x31, 0x34,
    0x3b, 0x32, 0x3b, 0x32, 0x3b, 0x32, 0x3b, 0x34, 0x23, 0x30,
    0x40, 0x40, 0x24, 0x23, 0x39, 0x5f, 0x23, 0x31, 0x30, 0x5f,
    0x24, 0x23, 0x37, 0x4f, 0x23, 0x38, 0x4f, 0x24, 0x23, 0x35,
    0x47, 0x23, 0x36, 0x47, 0x24, 0x23, 0x33, 0x43, 0x23, 0x34,
    0x43, 0x24, 0x23, 0x31, 0x41, 0x23, 0x32, 0x41, 0x2d, 0x23,
    0x30, 0x41, 0x23, 0x31, 0x33, 0x41, 0x24, 0x23, 0x31, 0x31,
    0x40, 0x23, 0x31, 0x32, 0x40, 0x1b, 0x5c
};

static const gsize incremental_sixel_size = sizeof(incremental_sixel_data);

typedef struct IncrementalPixbufCapture {
    GdkPixbuf *pixbuf;
    gboolean prepared_called;
    gboolean updated_called;
} IncrementalPixbufCapture;

static void
incremental_prepared_cb(GdkPixbuf *pixbuf,
                        GdkPixbufAnimation *animation,
                        gpointer user_data)
{
    IncrementalPixbufCapture *capture;

    (void) animation;

    capture = (IncrementalPixbufCapture *)user_data;
    capture->pixbuf = g_object_ref(pixbuf);
    capture->prepared_called = TRUE;
}

static void
incremental_updated_cb(GdkPixbuf *pixbuf,
                       gint x,
                       gint y,
                       gint width,
                       gint height,
                       gpointer user_data)
{
    IncrementalPixbufCapture *capture;

    (void) pixbuf;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    capture = (IncrementalPixbufCapture *)user_data;
    capture->updated_called = TRUE;
}

static void
assert_incremental_load(void)
{
    GdkPixbufModule module;
    GdkPixbufFormat format;
    gpointer context;
    IncrementalPixbufCapture capture;
    GError *error;
    gsize offset;
    gsize chunk;
    gboolean ok;

    memset(&module, 0, sizeof(module));
    memset(&format, 0, sizeof(format));
    memset(&capture, 0, sizeof(capture));
    error = NULL;

    fill_info(&format);
    fill_vtable(&module);

    context = module.begin_load(NULL,
                                incremental_prepared_cb,
                                incremental_updated_cb,
                                &capture,
                                &error);
    g_assert_nonnull(context);
    g_assert_no_error(error);

    offset = 0U;
    while (offset < incremental_sixel_size) {
        chunk = 32U;
        if (offset + chunk > incremental_sixel_size) {
            chunk = incremental_sixel_size - offset;
        }

        ok = module.load_increment(context,
                                   incremental_sixel_data + offset,
                                   (guint)chunk,
                                   &error);
        g_assert_true(ok);
        g_assert_no_error(error);
        offset += chunk;
    }

    ok = module.stop_load(context, &error);
    g_assert_true(ok);
    g_assert_no_error(error);

    g_assert_true(capture.prepared_called);
    g_assert_true(capture.updated_called);
    g_assert_nonnull(capture.pixbuf);
    g_assert_cmpint(gdk_pixbuf_get_width(capture.pixbuf), ==, 2);
    g_assert_cmpint(gdk_pixbuf_get_height(capture.pixbuf), ==, 8);

    g_object_unref(capture.pixbuf);
}

static void
incremental_load_test(void)
{
    assert_incremental_load();
}

int
test_gdk_pixbuf_loader_0002_incremental_load(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/incremental/load", incremental_load_test);

    return g_test_run();
}
#else

int
test_gdk_pixbuf_loader_0002_incremental_load(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    /* Skip when gdk-pixbuf loader support is unavailable. */
    return 77;
}

#endif
