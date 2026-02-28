/*
 * Unified test runner for C-based tests under tests/.
 *
 * The runner dispatches to per-test functions so multiple test sources can be
 * linked into a single executable while keeping per-test exit status.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

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

static int
test_runner_setenv_portable(char const *name, char const *value);
static int
test_runner_apply_env_assignment(char const *assignment);

static int
test_runner_apply_env_options(int argc, char **argv, int *out_first_index)
{
    int index;
    char const *token;
    char const *assignment;
    int status;

    index = 1;
    token = NULL;
    assignment = NULL;

    while (index < argc) {
        token = argv[index];
        if (strcmp(token, "-%") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr,
                        "test_runner: short env option requires KEY=VALUE"
                        " argument\n");
                return -1;
            }
            assignment = argv[index + 1];
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 2;
            continue;
        }
        if (strncmp(token, "--env=", 6) == 0) {
            assignment = token + 6;
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 1;
            continue;
        }
        if (strcmp(token, "--env") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr,
                        "test_runner: --env requires KEY=VALUE argument\n");
                return -1;
            }
            assignment = argv[index + 1];
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 2;
            continue;
        }
        break;
    }

    *out_first_index = index;
    return 0;
}

static int
test_runner_apply_env_assignment(char const *assignment)
{
    char const *separator;
    size_t name_length;
    size_t value_length;
    char *name;
    char *value;
    int status;

    separator = NULL;
    name_length = 0u;
    value_length = 0u;
    name = NULL;
    value = NULL;
    status = -1;

    separator = strchr(assignment, '=');
    if (separator == NULL || separator == assignment) {
        fprintf(stderr, "test_runner: invalid env assignment\n");
        return -1;
    }

    name_length = (size_t)(separator - assignment);
    value_length = strlen(separator + 1);

    /*
     * Keep test-runner option parsing consistent with img2sixel --env
     * behavior even when hosted environments expose a very long PATH.
     */
    name = (char *)malloc(name_length + 1u);
    if (name == NULL) {
        fprintf(stderr, "test_runner: failed to allocate env key\n");
        return -1;
    }
    value = (char *)malloc(value_length + 1u);
    if (value == NULL) {
        free(name);
        fprintf(stderr, "test_runner: failed to allocate env value\n");
        return -1;
    }

    memcpy(name, assignment, name_length);
    name[name_length] = '\0';
    memcpy(value, separator + 1, value_length + 1u);

    status = test_runner_setenv_portable(name, value);
    free(value);
    free(name);
    return status;
}

static int
test_runner_setenv_portable(char const *name, char const *value)
{
#if defined(_MSC_VER)
    if (name == NULL || value == NULL) {
        return -1;
    }
    if (SetEnvironmentVariableA(name, value) == 0) {
        return -1;
    }
    return 0;
#elif defined(_WIN32) && defined(HAVE__PUTENV_S) && HAVE__PUTENV_S
    if (name == NULL || value == NULL) {
        return -1;
    }
    if (_putenv_s(name, value) != 0) {
        return -1;
    }
    return 0;
#else
    if (name == NULL || value == NULL) {
        return -1;
    }
    return setenv(name, value, 1);
#endif
}

int
main(int argc, char **argv)
{
    size_t index;
    char const *requested;
    int first_index;

    index = 0u;
    requested = NULL;
    first_index = 1;

    if (test_runner_apply_env_options(argc, argv, &first_index) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc <= first_index) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[first_index], "--list") == 0) {
        for (index = 0u; test_entries[index].name != NULL; index++) {
            printf("%s\n", test_entries[index].name);
        }
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[first_index], "--is-running-under-wine") == 0) {
        return test_runner_is_running_under_wine() ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
    }

    requested = argv[first_index];
    for (index = 0u; test_entries[index].name != NULL; index++) {
        if (strcmp(requested, test_entries[index].name) == 0) {
            return test_entries[index].run(argc - first_index,
                                           argv + first_index);
        }
    }

    fprintf(stderr, "unknown test: %s\n", requested);
    print_usage(argv[0]);
    return EXIT_FAILURE;
}
