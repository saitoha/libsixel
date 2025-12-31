/*
 * SPDX-License-Identifier: MIT
 *
 * Validate that the gdk-pixbuf loader module and the gdk-pixbuf2 plugin
 * cooperate when invoked through img2sixel.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

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
resolve_source_path(char const *suffix)
{
    gchar const *top_srcdir;
    gchar *resolved;

    top_srcdir = g_getenv("TOP_SRCDIR");
    if (top_srcdir == NULL || top_srcdir[0] == '\0') {
        top_srcdir = ".";
    }

    resolved = g_build_filename(top_srcdir, suffix, NULL);

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
find_module_dir(void)
{
    gchar *module_dir;
    gchar *module_path;
    gchar *module_alt_path;

    module_dir = resolve_build_path("gdk-pixbuf-loader");
    module_path = g_build_filename(module_dir,
                                   "libpixbufloader-sixel.so",
                                   NULL);
    module_alt_path = g_build_filename(module_dir,
                                       "libpixbufloader-sixel.dylib",
                                       NULL);

    if (g_file_test(module_path, G_FILE_TEST_IS_REGULAR) ||
        g_file_test(module_alt_path, G_FILE_TEST_IS_REGULAR)) {
        g_free(module_path);
        g_free(module_alt_path);
        return module_dir;
    }

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

    if (g_file_test(module_path, G_FILE_TEST_IS_REGULAR) ||
        g_file_test(module_alt_path, G_FILE_TEST_IS_REGULAR)) {
        g_free(module_path);
        g_free(module_alt_path);
        return module_dir;
    }

    g_free(module_dir);
    g_free(module_path);
    g_free(module_alt_path);

    return NULL;
}

static void
run_loader_plugin_integration(void)
{
#if !defined(HAVE_GDK_PIXBUF2)
    g_test_skip("gdk-pixbuf2 support is disabled");
    return;
#else
    GError *error;
    gchar *module_dir;
    gchar *module_cache_dir;
    gchar *module_cache_file;
    gchar *query_path;
    gchar *ld_path;
    gchar *dyld_path;
    gchar *img2sixel_path;
    gchar *snake_path;
    gchar *stdout_data;
    gchar *stderr_data;
    gchar **argv;
    gboolean ok;
    int status;

    error = NULL;
    module_dir = find_module_dir();
    if (module_dir == NULL) {
        g_test_skip("loader module directory is missing");
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

    img2sixel_path = resolve_build_path("converters/img2sixel");
    snake_path = resolve_source_path("images/snake.six");

    argv = g_new0(gchar *, 6);
    argv[0] = img2sixel_path;
    argv[1] = snake_path;
    argv[2] = g_strdup("-Lgdk-pixbuf2");
    argv[3] = g_strdup("-o/dev/null");
    argv[4] = g_strdup("-v");
    argv[5] = NULL;

    stdout_data = NULL;
    stderr_data = NULL;
    status = 0;

    ok = g_spawn_sync(NULL,
                      argv,
                      NULL,
                      G_SPAWN_SEARCH_PATH,
                      NULL,
                      NULL,
                      &stdout_data,
                      &stderr_data,
                      &status,
                      &error);
    g_assert_true(ok);
    g_assert_no_error(error);
    g_assert_true(g_spawn_check_wait_status(status, &error));
    g_assert_no_error(error);

    g_test_message("img2sixel stderr: %s",
                   stderr_data != NULL ? stderr_data : "<none>");

    if (stderr_data == NULL ||
        g_strstr_len(stderr_data, -1, "loader gdk-pixbuf2 succeeded") ==
            NULL) {
        g_error("missing loader success marker");
    }

    g_strfreev(argv);
    g_free(stdout_data);
    g_free(stderr_data);

    g_unlink(module_cache_file);
    g_rmdir(module_cache_dir);
    g_free(query_path);
    g_free(module_cache_file);
    g_free(module_cache_dir);
    g_free(module_dir);
    g_free(ld_path);
    g_free(dyld_path);
#endif
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdk-pixbuf/loader/plugin_integration",
                   run_loader_plugin_integration);

    return g_test_run();
}
