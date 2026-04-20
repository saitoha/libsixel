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
#include <errno.h>
#if HAVE_TIME_H
# include <time.h>
#endif

#if !defined(_WIN32)
# if HAVE_SIGNAL_H
#  include <signal.h>
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif
# if HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

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
int test_icc_0001_icc_builtin_rgb_gray_v4_paths(int argc, char **argv);
int test_icc_0002_icc_builtin_mab_mba_a2b0_paths(int argc, char **argv);
int test_icc_0003_icc_builtin_a2b_intent_paths(int argc, char **argv);
int test_icc_0004_icc_builtin_b2a_slot_paths(int argc, char **argv);
int test_icc_0005_icc_builtin_device_to_device_intent_paths(int argc,
                                                             char **argv);

int test_palette_0001_kmeans_init(int argc, char **argv);
int test_palette_0002_kmedoids_constraints(int argc, char **argv);
int test_palette_0003_kcenter_constraints(int argc, char **argv);

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
int test_loader_0024_loader_libwebp_fault_demux(int argc, char **argv);
int test_loader_0026_loader_libwebp_fault_options_init(int argc, char **argv);
int test_loader_0027_loader_libwebp_fault_decoder_new(int argc, char **argv);
int test_loader_0028_loader_libwebp_fault_decoder_getinfo(int argc,
                                                          char **argv);
int test_loader_0029_loader_libwebp_fault_decoder_getnext(int argc,
                                                          char **argv);
int test_loader_0032_loader_libwebp_fault_static_rgbinto(int argc,
                                                         char **argv);
int test_loader_0033_loader_libwebp_fault_static_rgbainto(int argc,
                                                          char **argv);
int test_loader_0034_loader_libwebp_fault_lossy_init_config(int argc,
                                                             char **argv);
int test_loader_0035_loader_libwebp_fault_lossy_decode(int argc, char **argv);
int test_loader_0036_loader_libwebp_fault_lossy_yuv_plane_missing(int argc,
                                                                   char **argv);
int test_loader_0037_loader_libwebp_fault_lossy_yuv_stride_invalid(int argc,
                                                                    char **argv);
int test_loader_0038_loader_libwebp_fault_lossy_dimensions_mismatch(int argc,
                                                                     char **argv);
int test_loader_0039_loader_libwebp_fault_no_frames(int argc, char **argv);
int test_loader_0040_loader_libwebp_fault_no_anmf(int argc, char **argv);
int test_loader_0041_loader_libwebp_frame_count_limit_fast(int argc,
                                                           char **argv);
int test_loader_0042_loader_libwebp_frame_count_limit_decoder_guard(int argc,
                                                                     char **argv);
int test_loader_0043_loader_libwebp_fault_get_features_static(int argc,
                                                              char **argv);
int test_loader_0044_loader_libwebp_fault_get_features_animation(int argc,
                                                                 char **argv);
int test_loader_0045_loader_libwebp_fault_static_malloc(int argc,
                                                        char **argv);
int test_loader_0046_loader_libwebp_fault_animation_canvas_malloc(int argc,
                                                                  char **argv);
int test_loader_0047_loader_libwebp_fault_lossy_malloc(int argc,
                                                       char **argv);
int test_loader_0048_loader_libwebp_fault_anmf_payload_too_small(int argc,
                                                                 char **argv);
int test_loader_0049_loader_libwebp_fault_anmf_extract_malloc(int argc,
                                                              char **argv);
int test_loader_0050_loader_osc11_colorspec_parser(int argc, char **argv);
int test_loader_0051_loader_osc11_response_parser(int argc, char **argv);
int test_loader_0052_loader_osc11_query_control(int argc, char **argv);
int test_loader_0053_loader_background_colorspace_override(int argc,
                                                            char **argv);
int test_loader_0054_loader_gif_bgcolor_canvas_fill(int argc, char **argv);
int test_loader_0055_loader_animation_hide_cursor_control(int argc,
                                                           char **argv);
int test_loader_0056_loader_factory_predicate_gate(int argc, char **argv);
int test_loader_0057_loader_gd_can_try_policy(int argc, char **argv);
int test_loader_0058_loader_gd_status_policy(int argc, char **argv);
int test_loader_0059_loader_gd_pixelpolicy_detail(int argc, char **argv);
int test_loader_0020_loader_librsvg_detect_svg_like(int argc, char **argv);
int test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback(int argc,
                                                                    char **argv
                                                                    );
int test_loader_0022_loader_libpng_indexed_png_reqcolors_fallback(int argc,
                                                                   char **argv);
int test_loader_0023_loader_librsvg_pixelformat(int argc, char **argv);
int test_loader_0025_loader_librsvg_decode_mode(int argc, char **argv);

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
    { "icc/0001_icc_builtin_rgb_gray_v4_paths",
      test_icc_0001_icc_builtin_rgb_gray_v4_paths },
    { "icc/0002_icc_builtin_mab_mba_a2b0_paths",
      test_icc_0002_icc_builtin_mab_mba_a2b0_paths },
    { "icc/0003_icc_builtin_a2b_intent_paths",
      test_icc_0003_icc_builtin_a2b_intent_paths },
    { "icc/0004_icc_builtin_b2a_slot_paths",
      test_icc_0004_icc_builtin_b2a_slot_paths },
    { "icc/0005_icc_builtin_device_to_device_intent_paths",
      test_icc_0005_icc_builtin_device_to_device_intent_paths },
    { "palette/0001_kmeans_init", test_palette_0001_kmeans_init },
    { "palette/0002_kmedoids_constraints",
      test_palette_0002_kmedoids_constraints },
    { "palette/0003_kcenter_constraints",
      test_palette_0003_kcenter_constraints },
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
    { "loader/0024_loader_libwebp_fault_demux",
      test_loader_0024_loader_libwebp_fault_demux },
    { "loader/0026_loader_libwebp_fault_options_init",
      test_loader_0026_loader_libwebp_fault_options_init },
    { "loader/0027_loader_libwebp_fault_decoder_new",
      test_loader_0027_loader_libwebp_fault_decoder_new },
    { "loader/0028_loader_libwebp_fault_decoder_getinfo",
      test_loader_0028_loader_libwebp_fault_decoder_getinfo },
    { "loader/0029_loader_libwebp_fault_decoder_getnext",
      test_loader_0029_loader_libwebp_fault_decoder_getnext },
    { "loader/0032_loader_libwebp_fault_static_rgbinto",
      test_loader_0032_loader_libwebp_fault_static_rgbinto },
    { "loader/0033_loader_libwebp_fault_static_rgbainto",
      test_loader_0033_loader_libwebp_fault_static_rgbainto },
    { "loader/0034_loader_libwebp_fault_lossy_init_config",
      test_loader_0034_loader_libwebp_fault_lossy_init_config },
    { "loader/0035_loader_libwebp_fault_lossy_decode",
      test_loader_0035_loader_libwebp_fault_lossy_decode },
    { "loader/0036_loader_libwebp_fault_lossy_yuv_plane_missing",
      test_loader_0036_loader_libwebp_fault_lossy_yuv_plane_missing },
    { "loader/0037_loader_libwebp_fault_lossy_yuv_stride_invalid",
      test_loader_0037_loader_libwebp_fault_lossy_yuv_stride_invalid },
    { "loader/0038_loader_libwebp_fault_lossy_dimensions_mismatch",
      test_loader_0038_loader_libwebp_fault_lossy_dimensions_mismatch },
    { "loader/0039_loader_libwebp_fault_no_frames",
      test_loader_0039_loader_libwebp_fault_no_frames },
    { "loader/0040_loader_libwebp_fault_no_anmf",
      test_loader_0040_loader_libwebp_fault_no_anmf },
    { "loader/0041_loader_libwebp_frame_count_limit_fast",
      test_loader_0041_loader_libwebp_frame_count_limit_fast },
    { "loader/0042_loader_libwebp_frame_count_limit_decoder_guard",
      test_loader_0042_loader_libwebp_frame_count_limit_decoder_guard },
    { "loader/0043_loader_libwebp_fault_get_features_static",
      test_loader_0043_loader_libwebp_fault_get_features_static },
    { "loader/0044_loader_libwebp_fault_get_features_animation",
      test_loader_0044_loader_libwebp_fault_get_features_animation },
    { "loader/0045_loader_libwebp_fault_static_malloc",
      test_loader_0045_loader_libwebp_fault_static_malloc },
    { "loader/0046_loader_libwebp_fault_animation_canvas_malloc",
      test_loader_0046_loader_libwebp_fault_animation_canvas_malloc },
    { "loader/0047_loader_libwebp_fault_lossy_malloc",
      test_loader_0047_loader_libwebp_fault_lossy_malloc },
    { "loader/0048_loader_libwebp_fault_anmf_payload_too_small",
      test_loader_0048_loader_libwebp_fault_anmf_payload_too_small },
    { "loader/0049_loader_libwebp_fault_anmf_extract_malloc",
      test_loader_0049_loader_libwebp_fault_anmf_extract_malloc },
    { "loader/0050_loader_osc11_colorspec_parser",
      test_loader_0050_loader_osc11_colorspec_parser },
    { "loader/0051_loader_osc11_response_parser",
      test_loader_0051_loader_osc11_response_parser },
    { "loader/0052_loader_osc11_query_control",
      test_loader_0052_loader_osc11_query_control },
    { "loader/0053_loader_background_colorspace_override",
      test_loader_0053_loader_background_colorspace_override },
    { "loader/0054_loader_gif_bgcolor_canvas_fill",
      test_loader_0054_loader_gif_bgcolor_canvas_fill },
    { "loader/0055_loader_animation_hide_cursor_control",
      test_loader_0055_loader_animation_hide_cursor_control },
    { "loader/0056_loader_factory_predicate_gate",
      test_loader_0056_loader_factory_predicate_gate },
    { "loader/0057_loader_gd_can_try_policy",
      test_loader_0057_loader_gd_can_try_policy },
    { "loader/0058_loader_gd_status_policy",
      test_loader_0058_loader_gd_status_policy },
    { "loader/0059_loader_gd_pixelpolicy_detail",
      test_loader_0059_loader_gd_pixelpolicy_detail },
    { "loader/0020_loader_librsvg_detect_svg_like",
      test_loader_0020_loader_librsvg_detect_svg_like },
    { "loader/0021_loader_builtin_indexed_png_reqcolors_fallback",
      test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback },
    { "loader/0022_loader_libpng_indexed_png_reqcolors_fallback",
      test_loader_0022_loader_libpng_indexed_png_reqcolors_fallback },
    { "loader/0023_loader_librsvg_pixelformat",
      test_loader_0023_loader_librsvg_pixelformat },
    { "loader/0025_loader_librsvg_decode_mode",
      test_loader_0025_loader_librsvg_decode_mode },
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
    fprintf(stderr,
            "       %s --win32-ctrl-break-run <delay-ms> <timeout-ms> "
            "<program> [args...]\n",
            program);
    fprintf(stderr,
            "       %s --sigint-run <delay-ms> <timeout-ms> "
            "<program> [args...]\n",
            program);
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
test_runner_run_windows_ctrl_break(int argc, char **argv)
{
#if defined(_WIN32)
    char const *delay_token;
    char const *timeout_token;
    unsigned long parsed_delay;
    unsigned long parsed_timeout;
    char *delay_end;
    char *timeout_end;
    char const *program;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    char *command_line;
    size_t command_line_length;
    size_t index;
    size_t token_length;
    char *cursor;
    char const *token_cursor;
    size_t backslash_count;
    DWORD wait_result;
    BOOL create_ok;
    BOOL handler_ignore_set;
    BOOL ctrl_break_sent;
    BOOL child_terminated;
    int exit_status;

    delay_token = NULL;
    timeout_token = NULL;
    parsed_delay = 0ul;
    parsed_timeout = 0ul;
    delay_end = NULL;
    timeout_end = NULL;
    program = NULL;
    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    command_line = NULL;
    command_line_length = 1u;
    index = 0u;
    token_length = 0u;
    cursor = NULL;
    token_cursor = NULL;
    backslash_count = 0u;
    wait_result = WAIT_FAILED;
    create_ok = FALSE;
    handler_ignore_set = FALSE;
    ctrl_break_sent = FALSE;
    child_terminated = FALSE;
    exit_status = EXIT_FAILURE;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <delay-ms> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    delay_token = argv[1];
    timeout_token = argv[2];
    errno = 0;
    parsed_delay = strtoul(delay_token, &delay_end, 10);
    if (errno != 0 || delay_end == delay_token || *delay_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid delay milliseconds: %s\n",
                delay_token);
        return EXIT_FAILURE;
    }

    errno = 0;
    parsed_timeout = strtoul(timeout_token, &timeout_end, 10);
    if (errno != 0 || timeout_end == timeout_token || *timeout_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid timeout milliseconds: %s\n",
                timeout_token);
        return EXIT_FAILURE;
    }

    if (parsed_delay > (unsigned long)MAXDWORD) {
        fprintf(stderr,
                "test_runner: delay milliseconds exceeds DWORD range\n");
        return EXIT_FAILURE;
    }

    if (parsed_timeout > (unsigned long)MAXDWORD) {
        fprintf(stderr,
                "test_runner: timeout milliseconds exceeds DWORD range\n");
        return EXIT_FAILURE;
    }

    program = argv[3];
    for (index = 3u; index < (size_t)argc; index++) {
        token_length = strlen(argv[index]);
        command_line_length += 3u + (token_length * 2u);
    }

    command_line = (char *)malloc(command_line_length);
    if (command_line == NULL) {
        fprintf(stderr, "test_runner: failed to allocate command line\n");
        return EXIT_FAILURE;
    }

    cursor = command_line;
    for (index = 3u; index < (size_t)argc; index++) {
        if (index != 3u) {
            *cursor++ = ' ';
        }
        *cursor++ = '"';
        token_cursor = argv[index];
        backslash_count = 0u;
        while (*token_cursor != '\0') {
            if (*token_cursor == '\\') {
                backslash_count++;
                token_cursor++;
                continue;
            }
            if (*token_cursor == '"') {
                while (backslash_count > 0u) {
                    *cursor++ = '\\';
                    *cursor++ = '\\';
                    backslash_count--;
                }
                *cursor++ = '\\';
                *cursor++ = '"';
                token_cursor++;
                continue;
            }
            while (backslash_count > 0u) {
                *cursor++ = '\\';
                backslash_count--;
            }
            *cursor++ = *token_cursor;
            token_cursor++;
        }
        while (backslash_count > 0u) {
            *cursor++ = '\\';
            *cursor++ = '\\';
            backslash_count--;
        }
        *cursor++ = '"';
    }
    *cursor = '\0';

    startup_info.cb = sizeof(startup_info);
    create_ok = CreateProcessA(program,
                               command_line,
                               NULL,
                               NULL,
                               FALSE,
                               CREATE_NEW_PROCESS_GROUP,
                               NULL,
                               NULL,
                               &startup_info,
                               &process_info);
    if (!create_ok) {
        fprintf(stderr,
                "test_runner: CreateProcessA failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    if (parsed_delay > 0ul) {
        Sleep((DWORD)parsed_delay);
    }

    handler_ignore_set = SetConsoleCtrlHandler(NULL, TRUE);
    if (!handler_ignore_set) {
        fprintf(stderr,
                "test_runner: SetConsoleCtrlHandler(ignore) failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    ctrl_break_sent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,
                                               process_info.dwProcessId);
    if (!ctrl_break_sent) {
        fprintf(stderr,
                "test_runner: GenerateConsoleCtrlEvent failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    wait_result = WaitForSingleObject(process_info.hProcess,
                                      (DWORD)parsed_timeout);
    if (wait_result == WAIT_OBJECT_0) {
        child_terminated = TRUE;
        exit_status = EXIT_SUCCESS;
        goto cleanup;
    }

    if (wait_result == WAIT_TIMEOUT) {
        fprintf(stderr,
                "test_runner: timeout waiting child after CTRL_BREAK_EVENT\n");
        goto cleanup;
    }

    fprintf(stderr,
            "test_runner: WaitForSingleObject failed: %lu\n",
            (unsigned long)GetLastError());

cleanup:
    if (handler_ignore_set) {
        SetConsoleCtrlHandler(NULL, FALSE);
    }
    if (!child_terminated && process_info.hProcess != NULL) {
        TerminateProcess(process_info.hProcess, 1u);
        WaitForSingleObject(process_info.hProcess, 5000u);
    }
    if (process_info.hThread != NULL) {
        CloseHandle(process_info.hThread);
    }
    if (process_info.hProcess != NULL) {
        CloseHandle(process_info.hProcess);
    }
    free(command_line);
    return exit_status;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr,
            "test_runner: --win32-ctrl-break-run is unavailable\n");
    return EXIT_FAILURE;
#endif
}

/*
 * The SIGINT-driven runner is available only on non-Windows builds.
 * Keep its sleep helper in the same platform scope so -Wunused-function
 * does not fail Windows toolchains that treat warnings as errors.
 */
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
static int
test_runner_sleep_milliseconds(unsigned long milliseconds)
{
    struct timespec delay;
    struct timespec remain;
    int sleep_status;

    delay.tv_sec = 0;
    delay.tv_nsec = 0L;
    remain.tv_sec = 0;
    remain.tv_nsec = 0L;
    sleep_status = 0;

    delay.tv_sec = (time_t)(milliseconds / 1000ul);
    delay.tv_nsec = (long)(milliseconds % 1000ul) * 1000000L;
    for (;;) {
        sleep_status = nanosleep(&delay, &remain);
        if (sleep_status == 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
        delay = remain;
    }
}
#endif

static int
test_runner_run_posix_sigint(int argc, char **argv)
{
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char const *delay_token;
    char const *timeout_token;
    unsigned long parsed_delay;
    unsigned long parsed_timeout;
    char *delay_end;
    char *timeout_end;
    char const *program;
    char **program_argv;
    pid_t child_pid;
    pid_t wait_pid;
    int wait_status;
    int child_running;
    unsigned long elapsed_ms;
    unsigned long poll_interval_ms;
    int send_result;
    int kill_result;

    delay_token = NULL;
    timeout_token = NULL;
    parsed_delay = 0ul;
    parsed_timeout = 0ul;
    delay_end = NULL;
    timeout_end = NULL;
    program = NULL;
    program_argv = NULL;
    child_pid = (pid_t)0;
    wait_pid = (pid_t)0;
    wait_status = 0;
    child_running = 1;
    elapsed_ms = 0ul;
    poll_interval_ms = 10ul;
    send_result = 0;
    kill_result = 0;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <delay-ms> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    delay_token = argv[1];
    timeout_token = argv[2];
    program = argv[3];
    program_argv = argv + 3;

    errno = 0;
    parsed_delay = strtoul(delay_token, &delay_end, 10);
    if (errno != 0 || delay_end == delay_token || *delay_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid delay milliseconds: %s\n",
                delay_token);
        return EXIT_FAILURE;
    }

    errno = 0;
    parsed_timeout = strtoul(timeout_token, &timeout_end, 10);
    if (errno != 0 || timeout_end == timeout_token || *timeout_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid timeout milliseconds: %s\n",
                timeout_token);
        return EXIT_FAILURE;
    }

    child_pid = fork();
    if (child_pid < (pid_t)0) {
        fprintf(stderr, "test_runner: fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == (pid_t)0) {
        (void)setpgid(0, 0);
        (void)execvp(program, program_argv);
        fprintf(stderr, "test_runner: execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (setpgid(child_pid, child_pid) != 0
        && errno != EACCES && errno != ESRCH) {
        fprintf(stderr, "test_runner: setpgid failed: %s\n", strerror(errno));
    }

    if (parsed_delay > 0ul
        && test_runner_sleep_milliseconds(parsed_delay) != 0) {
        fprintf(stderr, "test_runner: nanosleep failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }

    send_result = kill((pid_t)(-child_pid), SIGINT);
    if (send_result != 0 && errno != ESRCH) {
        fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                strerror(errno));
    }

    if (parsed_timeout > 0ul && poll_interval_ms > parsed_timeout) {
        poll_interval_ms = parsed_timeout;
    }

    while (child_running != 0) {
        wait_pid = waitpid(child_pid, &wait_status, WNOHANG);
        if (wait_pid == child_pid) {
            child_running = 0;
            break;
        }
        if (wait_pid < (pid_t)0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "test_runner: waitpid failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout == 0ul || elapsed_ms >= parsed_timeout) {
            fprintf(stderr,
                    "test_runner: timeout waiting child after SIGINT\n");
            goto timeout_cleanup;
        }
        if (test_runner_sleep_milliseconds(poll_interval_ms) != 0) {
            fprintf(stderr, "test_runner: nanosleep failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (elapsed_ms > parsed_timeout - poll_interval_ms) {
            elapsed_ms = parsed_timeout;
        } else {
            elapsed_ms += poll_interval_ms;
        }
    }

    return EXIT_SUCCESS;

timeout_cleanup:
    kill_result = kill((pid_t)(-child_pid), SIGKILL);
    if (kill_result != 0 && errno != ESRCH) {
        fprintf(stderr, "test_runner: SIGKILL send failed: %s\n",
                strerror(errno));
    }
    for (;;) {
        wait_pid = waitpid(child_pid, &wait_status, 0);
        if (wait_pid == child_pid) {
            break;
        }
        if (wait_pid < (pid_t)0 && errno == EINTR) {
            continue;
        }
        if (wait_pid < (pid_t)0 && errno == ECHILD) {
            break;
        }
        if (wait_pid < (pid_t)0) {
            fprintf(stderr, "test_runner: waitpid cleanup failed: %s\n",
                    strerror(errno));
            break;
        }
    }
    return EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr, "test_runner: --sigint-run is unavailable\n");
    return EXIT_FAILURE;
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
# if defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);
# endif
    if (name == NULL || value == NULL) {
        return -1;
    }
# if defined(HAVE_SETENV)
    return setenv(name, value, 1);
# else
    (void)name;
    (void)value;

    return -1;
# endif
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

    if (strcmp(argv[first_index], "--win32-ctrl-break-run") == 0) {
        return test_runner_run_windows_ctrl_break(argc - first_index,
                                                  argv + first_index);
    }

    if (strcmp(argv[first_index], "--sigint-run") == 0) {
        return test_runner_run_posix_sigint(argc - first_index,
                                            argv + first_index);
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
