/*
 * SPDX-License-Identifier: MIT
 *
 * GLib-based tests for the SIXEL gdk-pixbuf loader.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_GDK_PIXBUF2)

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <string.h>

/* Loader entry points provided by sixel-pixbuf-loader.c. */
G_MODULE_EXPORT void fill_info(GdkPixbufFormat *info);
G_MODULE_EXPORT void fill_vtable(GdkPixbufModule *module);

static const unsigned char tiny_sixel_data[] = {
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

static const gsize tiny_sixel_size = sizeof(tiny_sixel_data);
static const gsize tiny_sixel_truncated_size = sizeof(tiny_sixel_data) - 2U;

static const unsigned char oversized_sixel_data[] =
    "\x1bPq\"1;1;6001;1#0;2;0;0;0"
    "#0!1?\x1b\\";

typedef struct PixbufCapture {
    GdkPixbuf *pixbuf;
    gboolean updated_called;
} PixbufCapture;

static void
prepared_cb(GdkPixbuf *pixbuf,
            GdkPixbufAnimation *animation,
            gpointer user_data)
{
    PixbufCapture *capture;

    (void) animation;

    capture = (PixbufCapture *)user_data;
    capture->pixbuf = g_object_ref(pixbuf);
}

static void
updated_cb(GdkPixbuf *pixbuf,
           gint x,
           gint y,
           gint width,
           gint height,
           gpointer user_data)
{
    PixbufCapture *capture;

    (void) pixbuf;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    capture = (PixbufCapture *)user_data;
    capture->updated_called = TRUE;
}

static GdkPixbuf *
load_pixbuf_from_bytes(unsigned char const *data, gsize size, GError **error)
{
    GdkPixbufModule module;
    GdkPixbufFormat format;
    gpointer context;
    GdkPixbuf *pixbuf;
    GError *local_error;
    PixbufCapture capture;

    memset(&module, 0, sizeof(module));
    memset(&format, 0, sizeof(format));
    pixbuf = NULL;
    local_error = NULL;
    memset(&capture, 0, sizeof(capture));

    fill_info(&format);
    fill_vtable(&module);

    context = module.begin_load(NULL,
                                prepared_cb,
                                updated_cb,
                                &capture,
                                &local_error);
    if (context == NULL) {
        g_propagate_error(error, local_error);
        return NULL;
    }

    if (!module.load_increment(context, data, (guint)size, &local_error)) {
        module.stop_load(context, &local_error);
        g_propagate_error(error, local_error);
        return NULL;
    }

    if (!module.stop_load(context, &local_error)) {
        g_propagate_error(error, local_error);
        return NULL;
    }

    /* The loader must deliver the pixbuf through prepared_cb(). */
    g_assert_nonnull(capture.pixbuf);
    g_assert_true(capture.updated_called);
    pixbuf = capture.pixbuf;

    return pixbuf;
}

static void
assert_tail_pixels(GdkPixbuf *pixbuf)
{
    guchar const *pixels;
    int rowstride;
    int channels;
    int idx;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    channels = gdk_pixbuf_get_n_channels(pixbuf);

    /* Last four pixels should match the embedded sample colors. */
    idx = 5 * rowstride;
    g_assert_cmpint(pixels[idx + 0], ==, 255);
    g_assert_cmpint(pixels[idx + 1], ==, 5);
    g_assert_cmpint(pixels[idx + 2], ==, 5);

    idx = 5 * rowstride + channels;
    g_assert_cmpint(pixels[idx + 0], ==, 5);
    g_assert_cmpint(pixels[idx + 1], ==, 255);
    g_assert_cmpint(pixels[idx + 2], ==, 5);

    idx = 6 * rowstride;
    g_assert_cmpint(pixels[idx + 0], ==, 5);
    g_assert_cmpint(pixels[idx + 1], ==, 5);
    g_assert_cmpint(pixels[idx + 2], ==, 255);

    idx = 6 * rowstride + channels;
    g_assert_cmpint(pixels[idx + 0], ==, 255);
    g_assert_cmpint(pixels[idx + 1], ==, 255);
    g_assert_cmpint(pixels[idx + 2], ==, 5);
}

static void
test_basic_load(void)
{
    GdkPixbuf *pixbuf;
    GError *error;

    error = NULL;
    pixbuf = load_pixbuf_from_bytes(tiny_sixel_data, tiny_sixel_size, &error);

    g_assert_no_error(error);
    g_assert_nonnull(pixbuf);
    g_assert_cmpint(gdk_pixbuf_get_width(pixbuf), ==, 2);
    g_assert_cmpint(gdk_pixbuf_get_height(pixbuf), ==, 8);
    g_assert_cmpint(gdk_pixbuf_get_n_channels(pixbuf), ==, 3);

    assert_tail_pixels(pixbuf);

    g_object_unref(pixbuf);
}

static void
test_truncated_load(void)
{
    GdkPixbuf *pixbuf;
    GError *error;

    error = NULL;
    pixbuf = load_pixbuf_from_bytes(tiny_sixel_data,
                                    tiny_sixel_truncated_size,
                                    &error);

    g_assert_no_error(error);
    g_assert_nonnull(pixbuf);
    g_assert_cmpint(gdk_pixbuf_get_width(pixbuf), ==, 2);
    g_assert_cmpint(gdk_pixbuf_get_height(pixbuf), ==, 8);

    assert_tail_pixels(pixbuf);

    g_object_unref(pixbuf);
}

static void
test_oversized_load(void)
{
    GdkPixbuf *pixbuf;
    GError *error;

    pixbuf = NULL;
    error = NULL;

    pixbuf = load_pixbuf_from_bytes(oversized_sixel_data,
                                    strlen((char const *)oversized_sixel_data),
                                    &error);

    g_assert_null(pixbuf);
    g_assert_error(error, GDK_PIXBUF_ERROR, error->code);
    g_assert_true(error->code == GDK_PIXBUF_ERROR_FAILED ||
                  error->code == GDK_PIXBUF_ERROR_CORRUPT_IMAGE);

    g_clear_error(&error);
}

static gpointer
thread_entry(gpointer user_data)
{
    GError *error;
    GdkPixbuf *pixbuf;

    (void) user_data;

    error = NULL;
    pixbuf = load_pixbuf_from_bytes(tiny_sixel_data, tiny_sixel_size, &error);
    g_assert_no_error(error);
    g_assert_nonnull(pixbuf);
    g_object_unref(pixbuf);

    return NULL;
}

static void
test_threaded_load(void)
{
    GThread *threads[4];
    guint i;

    for (i = 0; i < G_N_ELEMENTS(threads); ++i) {
        threads[i] = g_thread_new("sixel-thread", thread_entry, NULL);
        g_assert_nonnull(threads[i]);
    }

    for (i = 0; i < G_N_ELEMENTS(threads); ++i) {
        g_thread_join(threads[i]);
    }
}

int
test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/loader/basic", test_basic_load);
    g_test_add_func("/gdk-pixbuf/loader/truncated", test_truncated_load);
    g_test_add_func("/gdk-pixbuf/loader/oversized", test_oversized_load);
    g_test_add_func("/gdk-pixbuf/loader/threaded", test_threaded_load);

    return g_test_run();
}
#else

int
test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    /* Skip when gdk-pixbuf loader support is unavailable. */
    return 77;
}

#endif
