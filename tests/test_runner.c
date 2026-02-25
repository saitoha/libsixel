/*
 * Unified test runner for C-based tests under tests/.
 *
 * The runner dispatches to per-test functions so multiple test sources can be
 * linked into a single executable while keeping per-test exit status.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
# include <windows.h>
#endif

int test_filter_0001_filter_clip(int argc, char **argv);
int test_filter_0002_filter_sample(int argc, char **argv);
int test_filter_0003_filter_resize(int argc, char **argv);
int test_filter_0004_filter_colors(int argc, char **argv);
int test_filter_0005_filter_lookup(int argc, char **argv);
int test_filter_0006_filter_final_merge(int argc, char **argv);
int test_filter_0007_filter_fhedt(int argc, char **argv);
int test_filter_0008_filter_load(int argc, char **argv);
int test_filter_0009_filter_dither(int argc, char **argv);
int test_filter_0010_filter_encode(int argc, char **argv);

int test_probe_0001_probe_parse(int argc, char **argv);

int test_palette_0001_kmeans_init(int argc, char **argv);

int test_cli_0029_cli_token_is_known_option(int argc, char **argv);
int test_cli_0030_cli_option_requires_argument(int argc, char **argv);
int test_cli_0031_cli_guard_missing_argument(int argc, char **argv);

int test_loader_0008_loader_coregraphics_pixelformat(int argc, char **argv);
int test_loader_0009_loader_wic_pixelformat(int argc, char **argv);
int test_loader_0010_loader_gdk_pixbuf_pixelformat(int argc, char **argv);
int test_loader_0011_loader_gd_pixelformat(int argc, char **argv);
int test_loader_0012_loader_libpng_pixelformat(int argc, char **argv);
int test_loader_0013_loader_libjpeg_pixelformat(int argc, char **argv);
int test_loader_0014_loader_builtin_pixelformat(int argc, char **argv);
int test_loader_0015_loader_quicklook_pixelformat(int argc, char **argv);
int test_loader_0016_loader_gnome_thumbnailer_pixelformat(int argc,
                                                          char **argv);
int test_aborttrace_0001_img2sixel_aborttrace(int argc, char **argv);
int test_loader_0017_loader_libwebp_pixelformat(int argc, char **argv);
int test_loader_0018_loader_libwebp_animation_frames(int argc, char **argv);
int test_loader_0019_loader_libwebp_palette_promotion_guard(int argc,
                                                             char **argv);
int test_loader_0020_loader_librsvg_detect_svg_like(int argc, char **argv);
int test_loader_0021_loader_librsvg_default_size(int argc, char **argv);
int test_loader_0022_loader_librsvg_viewbox_size(int argc, char **argv);
int test_loader_0023_loader_librsvg_bgcolor_option(int argc, char **argv);

#if defined(SIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS)
int test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader(int argc, char **argv);
int test_gdk_pixbuf_loader_0002_incremental_load(int argc, char **argv);
int test_gdk_pixbuf_loader_0003_corrupt_data(int argc, char **argv);
int test_gdk_pixbuf_loader_0004_propagate_error(int argc, char **argv);
int test_gdk_pixbuf_loader_0005_context_free(int argc, char **argv);
int test_gdk_pixbuf_loader_0006_loader_module(int argc, char **argv);
int test_gdk_pixbuf_loader_0007_loader_plugin_integration(int argc,
                                                          char **argv);
#endif

typedef int (*test_runner_fn)(int argc, char **argv);

typedef struct test_entry {
    char const *name;
    test_runner_fn run;
} test_entry_t;

static test_entry_t const test_entries[] = {
    { "filter/0001_filter_clip", test_filter_0001_filter_clip },
    { "filter/0002_filter_sample", test_filter_0002_filter_sample },
    { "filter/0003_filter_resize", test_filter_0003_filter_resize },
    { "filter/0004_filter_colors", test_filter_0004_filter_colors },
    { "filter/0005_filter_lookup", test_filter_0005_filter_lookup },
    { "filter/0006_filter_final_merge", test_filter_0006_filter_final_merge },
    { "filter/0007_filter_fhedt", test_filter_0007_filter_fhedt },
    { "filter/0008_filter_load", test_filter_0008_filter_load },
    { "filter/0009_filter_dither", test_filter_0009_filter_dither },
    { "filter/0010_filter_encode", test_filter_0010_filter_encode },
    { "probe/0001_probe_parse", test_probe_0001_probe_parse },
    { "palette/0001_kmeans_init", test_palette_0001_kmeans_init },
    { "cli/0029_cli_token_is_known_option",
      test_cli_0029_cli_token_is_known_option },
    { "cli/0030_cli_option_requires_argument",
      test_cli_0030_cli_option_requires_argument },
    { "cli/0031_cli_guard_missing_argument",
      test_cli_0031_cli_guard_missing_argument },
    { "loader/0008_loader_coregraphics_pixelformat",
      test_loader_0008_loader_coregraphics_pixelformat },
    { "loader/0009_loader_wic_pixelformat",
      test_loader_0009_loader_wic_pixelformat },
    { "loader/0010_loader_gdk_pixbuf_pixelformat",
      test_loader_0010_loader_gdk_pixbuf_pixelformat },
    { "loader/0011_loader_gd_pixelformat",
      test_loader_0011_loader_gd_pixelformat },
    { "loader/0012_loader_libpng_pixelformat",
      test_loader_0012_loader_libpng_pixelformat },
    { "loader/0013_loader_libjpeg_pixelformat",
      test_loader_0013_loader_libjpeg_pixelformat },
    { "loader/0014_loader_builtin_pixelformat",
      test_loader_0014_loader_builtin_pixelformat },
    { "loader/0015_loader_quicklook_pixelformat",
      test_loader_0015_loader_quicklook_pixelformat },
    { "loader/0016_loader_gnome_thumbnailer_pixelformat",
      test_loader_0016_loader_gnome_thumbnailer_pixelformat },
    { "aborttrace/0001_img2sixel_aborttrace",
      test_aborttrace_0001_img2sixel_aborttrace },
    { "loader/0017_loader_libwebp_pixelformat",
      test_loader_0017_loader_libwebp_pixelformat },
    { "loader/0018_loader_libwebp_animation_frames",
      test_loader_0018_loader_libwebp_animation_frames },
    { "loader/0019_loader_libwebp_palette_promotion_guard",
      test_loader_0019_loader_libwebp_palette_promotion_guard },
    { "loader/0020_loader_librsvg_detect_svg_like",
      test_loader_0020_loader_librsvg_detect_svg_like },
    { "loader/0021_loader_librsvg_default_size",
      test_loader_0021_loader_librsvg_default_size },
    { "loader/0022_loader_librsvg_viewbox_size",
      test_loader_0022_loader_librsvg_viewbox_size },
    { "loader/0023_loader_librsvg_bgcolor_option",
      test_loader_0023_loader_librsvg_bgcolor_option },
#if defined(SIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS)
    { "gdk-pixbuf-loader/0001_gdk_pixbuf_loader",
      test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader },
    { "gdk-pixbuf-loader/0002_incremental_load",
      test_gdk_pixbuf_loader_0002_incremental_load },
    { "gdk-pixbuf-loader/0003_corrupt_data",
      test_gdk_pixbuf_loader_0003_corrupt_data },
    { "gdk-pixbuf-loader/0004_propagate_error",
      test_gdk_pixbuf_loader_0004_propagate_error },
    { "gdk-pixbuf-loader/0005_context_free",
      test_gdk_pixbuf_loader_0005_context_free },
    { "gdk-pixbuf-loader/0006_loader_module",
      test_gdk_pixbuf_loader_0006_loader_module },
    { "gdk-pixbuf-loader/0007_loader_plugin_integration",
      test_gdk_pixbuf_loader_0007_loader_plugin_integration },
#endif
    { NULL, NULL }
};

static void
print_usage(char const *program)
{
    size_t index;

    fprintf(stderr, "usage: %s <test-name> [args...]\n", program);
    fprintf(stderr, "       %s --list\n", program);
    fprintf(stderr, "       %s --is-running-under-wine\n", program);
    fprintf(stderr, "\n");
    fprintf(stderr, "available tests:\n");
    for (index = 0u; test_entries[index].name != NULL; index++) {
        fprintf(stderr, "  %s\n", test_entries[index].name);
    }
}

static int
test_runner_is_running_under_wine(void)
{
#if defined(_WIN32)
    HMODULE ntdll;
    FARPROC wine_get_version;

    ntdll = NULL;
    wine_get_version = NULL;

    ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) {
        return 0;
    }

    wine_get_version = GetProcAddress(ntdll, "wine_get_version");
    if (wine_get_version == NULL) {
        return 0;
    }

    return 1;
#else
    return 0;
#endif
}

int
main(int argc, char **argv)
{
    size_t index;
    char const *requested;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--list") == 0) {
        for (index = 0u; test_entries[index].name != NULL; index++) {
            printf("%s\n", test_entries[index].name);
        }
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--is-running-under-wine") == 0) {
        return test_runner_is_running_under_wine() ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
    }

    requested = argv[1];
    for (index = 0u; test_entries[index].name != NULL; index++) {
        if (strcmp(requested, test_entries[index].name) == 0) {
            return test_entries[index].run(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "unknown test: %s\n", requested);
    print_usage(argv[0]);
    return EXIT_FAILURE;
}
