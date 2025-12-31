/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the SIXEL loader through the gdk-pixbuf module mechanism.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

static unsigned char const embedded_sixel[] = {
    0x1b, 0x50, 0x71, 0x22, 0x31, 0x3b, 0x31, 0x3b, 0x32, 0x3b,
    0x33, 0x23, 0x30, 0x3b, 0x32, 0x3b, 0x31, 0x3b, 0x31, 0x3b,
    0x31, 0x23, 0x31, 0x3b, 0x32, 0x3b, 0x34, 0x3b, 0x34, 0x3b,
    0x34, 0x23, 0x32, 0x3b, 0x32, 0x3b, 0x32, 0x34, 0x3b, 0x33,
    0x30, 0x3b, 0x33, 0x32, 0x23, 0x33, 0x3b, 0x32, 0x3b, 0x33,
    0x33, 0x3b, 0x33, 0x37, 0x23, 0x34, 0x3b, 0x32, 0x3b, 0x34,
    0x31, 0x3b, 0x34, 0x35, 0x3b, 0x34, 0x38, 0x23, 0x35, 0x3b,
    0x32, 0x3b, 0x35, 0x33, 0x3b, 0x35, 0x36, 0x3b, 0x36, 0x30,
    0x23, 0x36, 0x3b, 0x32, 0x3b, 0x36, 0x34, 0x3b, 0x36, 0x38,
    0x3b, 0x37, 0x31, 0x23, 0x37, 0x3b, 0x32, 0x3b, 0x37, 0x36,
    0x3b, 0x37, 0x39, 0x3b, 0x38, 0x34, 0x23, 0x38, 0x3b, 0x32,
    0x3b, 0x38, 0x36, 0x3b, 0x39, 0x32, 0x3b, 0x39, 0x35, 0x23,
    0x39, 0x3b, 0x32, 0x3b, 0x31, 0x30, 0x30, 0x3b, 0x31, 0x3b,
    0x32, 0x23, 0x31, 0x30, 0x3b, 0x32, 0x3b, 0x32, 0x3b, 0x31,
    0x30, 0x30, 0x3b, 0x32, 0x23, 0x31, 0x31, 0x3b, 0x32, 0x3b,
    0x32, 0x3b, 0x32, 0x3b, 0x31, 0x30, 0x30, 0x23, 0x31, 0x32,
    0x3b, 0x32, 0x3b, 0x31, 0x30, 0x30, 0x3b, 0x31, 0x30, 0x30,
    0x3b, 0x32, 0x23, 0x31, 0x33, 0x3b, 0x32, 0x3b, 0x31, 0x3b,
    0x32, 0x3b, 0x32, 0x23, 0x31, 0x34, 0x3b, 0x32, 0x3b, 0x32,
    0x3b, 0x32, 0x3b, 0x34, 0x23, 0x30, 0x40, 0x40, 0x24, 0x23,
    0x39, 0x5f, 0x23, 0x31, 0x30, 0x5f, 0x24, 0x23, 0x37, 0x4f,
    0x23, 0x38, 0x4f, 0x24, 0x23, 0x35, 0x47, 0x23, 0x36, 0x47,
    0x24, 0x23, 0x33, 0x43, 0x23, 0x34, 0x43, 0x24, 0x23, 0x31,
    0x41, 0x23, 0x32, 0x41, 0x2d, 0x23, 0x30, 0x41, 0x23, 0x31,
    0x33, 0x41, 0x24, 0x23, 0x31, 0x31, 0x40, 0x23, 0x31, 0x32,
    0x40, 0x1b, 0x5c
};

static gchar *
find_query_loaders_path(GError **error)
{
    gboolean spawned;
    gchar *stdout_data;
    gchar *stderr_data;
    gchar *query_path;
    gchar *stripped;
    gchar **argv;
    int status;

    query_path = g_strdup(g_getenv("GDK_PIXBUF_QUERY_LOADERS"));
    if (query_path != NULL && query_path[0] != '\0') {
        return query_path;
    }

    query_path = g_find_program_in_path("gdk-pixbuf-query-loaders");
    if (query_path != NULL) {
        return query_path;
    }

    argv = g_new0(gchar *, 4);
    argv[0] = g_strdup("pkg-config");
    argv[1] = g_strdup("--variable=gdk_pixbuf_query_loaders");
    argv[2] = g_strdup("gdk-pixbuf-2.0");

    stdout_data = NULL;
    stderr_data = NULL;
    status = 0;
    spawned = g_spawn_sync(NULL,
                           argv,
                           NULL,
                           G_SPAWN_SEARCH_PATH,
                           NULL,
                           NULL,
                           &stdout_data,
                           &stderr_data,
                           &status,
                           error);

    g_strfreev(argv);

    if (!spawned) {
        g_free(stdout_data);
        g_free(stderr_data);
        return NULL;
    }

    if (!g_spawn_check_wait_status(status, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return NULL;
    }

    stripped = g_strstrip(stdout_data);
    if (stripped[0] == '\0') {
        g_free(stdout_data);
        g_free(stderr_data);
        return NULL;
    }

    query_path = g_strdup(stripped);
    g_free(stdout_data);
    g_free(stderr_data);

    return query_path;
}

static gchar *
resolve_build_path(char const *suffix)
{
    gchar const *top_builddir;
    gchar *resolved;

    top_builddir = g_getenv("TOP_BUILDDIR");
    if (top_builddir == NULL || top_builddir[0] == '\0') {
        top_builddir = ".";
    }

    resolved = g_build_filename(top_builddir, suffix, NULL);

    return resolved;
}

static gchar *
prepend_path_env(char const *name, gchar const *first)
{
    gchar const *current;
    gchar *joined;
    gchar *parts[3];
    gchar **strings;

    current = g_getenv(name);
    parts[0] = g_strdup(first);
    parts[1] = current != NULL ? g_strdup(current) : NULL;
    parts[2] = NULL;

    strings = parts;
    joined = g_strjoinv(":", strings);

    g_free(parts[0]);
    g_free(parts[1]);

    return joined;
}

static gboolean
write_loader_cache(char const *query_path,
                   char const *cache_file,
                   GError **error)
{
    gboolean spawned;
    gchar *stdout_data;
    gchar *stderr_data;
    gchar *argv[2];
    int status;

    stdout_data = NULL;
    stderr_data = NULL;
    status = 0;
    argv[0] = (gchar *)query_path;
    argv[1] = NULL;

    spawned = g_spawn_sync(NULL,
                           argv,
                           NULL,
                           G_SPAWN_SEARCH_PATH,
                           NULL,
                           NULL,
                           &stdout_data,
                           &stderr_data,
                           &status,
                           error);
    if (!spawned) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    if (!g_spawn_check_wait_status(status, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    if (!g_file_set_contents(cache_file, stdout_data, -1, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    g_free(stdout_data);
    g_free(stderr_data);

    return TRUE;
}

static gchar *
find_module_dir(gboolean *has_dylib)
{
    gchar *module_dir;
    gchar *module_path;
    gchar *module_alt_path;

    *has_dylib = FALSE;

    module_dir = resolve_build_path("gdk-pixbuf-loader");
    module_path = g_build_filename(module_dir,
                                   "libpixbufloader-sixel.so",
                                   NULL);
    module_alt_path = g_build_filename(module_dir,
                                       "libpixbufloader-sixel.dylib",
                                       NULL);

    if (g_file_test(module_path, G_FILE_TEST_IS_REGULAR)) {
        g_test_message("found module: %s", module_path);
        g_free(module_path);
        g_free(module_alt_path);
        return module_dir;
    }

    *has_dylib = g_file_test(module_alt_path, G_FILE_TEST_IS_REGULAR);

    g_free(module_dir);
    module_dir = resolve_build_path("gdk-pixbuf-loader/.libs");
    g_free(module_path);
    module_path = g_build_filename(module_dir,
                                   "libpixbufloader-sixel.so",
                                   NULL);
    g_free(module_alt_path);
    module_alt_path = g_build_filename(module_dir,
                                       "libpixbufloader-sixel.dylib",
                                       NULL);

    if (g_file_test(module_path, G_FILE_TEST_IS_REGULAR)) {
        g_test_message("found module: %s", module_path);
        g_free(module_path);
        g_free(module_alt_path);
        return module_dir;
    }

    *has_dylib = *has_dylib ||
                  g_file_test(module_alt_path, G_FILE_TEST_IS_REGULAR);

    g_free(module_dir);
    g_free(module_path);
    g_free(module_alt_path);

    return NULL;
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

    g_test_message("tail rows: r5c0=(%d,%d,%d) r5c1=(%d,%d,%d) "
                   "r6c0=(%d,%d,%d) r6c1=(%d,%d,%d)",
                   pixels[5 * rowstride + 0],
                   pixels[5 * rowstride + 1],
                   pixels[5 * rowstride + 2],
                   pixels[5 * rowstride + channels + 0],
                   pixels[5 * rowstride + channels + 1],
                   pixels[5 * rowstride + channels + 2],
                   pixels[6 * rowstride + 0],
                   pixels[6 * rowstride + 1],
                   pixels[6 * rowstride + 2],
                   pixels[6 * rowstride + channels + 0],
                   pixels[6 * rowstride + channels + 1],
                   pixels[6 * rowstride + channels + 2]);

    /* #13;2;1;2;2 scales to (255,3,5) once normalized to 0-255. */
    idx = 5 * rowstride;
    g_assert_cmpint(pixels[idx + 0], ==, 255);
    g_assert_cmpint(pixels[idx + 1], ==, 3);
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
run_loader_module_test(void)
{
    gchar *module_dir;
    gchar *module_cache_dir;
    gchar *module_cache_file;
    gchar *query_path;
    gchar *ld_path;
    gchar *dyld_path;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf;
    GError *error;
    gboolean ok;
    gboolean has_dylib;

    error = NULL;
    has_dylib = FALSE;
    module_dir = find_module_dir(&has_dylib);
    if (module_dir == NULL) {
        if (has_dylib) {
            g_test_skip("only .dylib module found; gdk-pixbuf expects .so");
        } else {
            g_test_skip("loader module directory is missing");
        }
        return;
    }

    query_path = find_query_loaders_path(&error);
    if (query_path == NULL) {
        g_test_skip("gdk-pixbuf-query-loaders is unavailable");
        g_clear_error(&error);
        g_free(module_dir);
        return;
    }

    module_cache_dir = g_dir_make_tmp("sixel-gdkpixbuf-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(module_cache_dir);

    module_cache_file = g_build_filename(module_cache_dir,
                                         "loaders.cache",
                                         NULL);

    ld_path = prepend_path_env("LD_LIBRARY_PATH", "src/.libs");
    dyld_path = prepend_path_env("DYLD_LIBRARY_PATH", "src/.libs");

    g_setenv("GDK_PIXBUF_MODULEDIR", module_dir, TRUE);
    g_setenv("GDK_PIXBUF_MODULE_FILE", module_cache_file, TRUE);
    g_setenv("LD_LIBRARY_PATH", ld_path, TRUE);
    g_setenv("DYLD_LIBRARY_PATH", dyld_path, TRUE);

    ok = write_loader_cache(query_path, module_cache_file, &error);
    g_assert_true(ok);
    g_assert_no_error(error);

    loader = gdk_pixbuf_loader_new_with_type("sixel", &error);
    g_assert_no_error(error);
    g_assert_nonnull(loader);

    ok = gdk_pixbuf_loader_write(loader,
                                 embedded_sixel,
                                 sizeof(embedded_sixel),
                                 &error);
    g_assert_true(ok);
    g_assert_no_error(error);

    ok = gdk_pixbuf_loader_close(loader, &error);
    g_assert_true(ok);
    g_assert_no_error(error);

    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    g_assert_nonnull(pixbuf);
    g_assert_cmpint(gdk_pixbuf_get_width(pixbuf), ==, 2);
    g_assert_cmpint(gdk_pixbuf_get_height(pixbuf), ==, 8);
    g_assert_cmpint(gdk_pixbuf_get_n_channels(pixbuf), ==, 3);

    assert_tail_pixels(pixbuf);

    g_object_unref(loader);

    g_unlink(module_cache_file);
    g_rmdir(module_cache_dir);
    g_free(query_path);
    g_free(module_cache_file);
    g_free(module_cache_dir);
    g_free(module_dir);
    g_free(ld_path);
    g_free(dyld_path);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/loader/module", run_loader_module_test);

    return g_test_run();
}
