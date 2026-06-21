/*
 * Unified test runner for C-based tests under tests/.
 *
 * The runner dispatches to per-test functions so multiple test sources can be
 * linked into a single executable while keeping per-test exit status.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
/*
 * NetBSD may keep getpgid() hidden under strict POSIX mode unless the XSI
 * surface is also requested. Keep this aligned with POSIX.1-2008 visibility.
 */
# define _XOPEN_SOURCE 700
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#if HAVE_TIME_H
# include <time.h>
#endif

#if !defined(_WIN32)
# if HAVE_FCNTL_H
#  include <fcntl.h>
# endif
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

#if defined(_WIN32) || defined(__CYGWIN__)
# include <windows.h>
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) \
    && HAVE_FCNTL_H && HAVE_FORK && HAVE_NANOSLEEP \
    && HAVE_SIGNAL_H && HAVE_SYS_WAIT_H && HAVE_UNISTD_H \
    && HAVE_WAITPID && HAVE_EXECVP
# define TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER 1
#else
# define TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER 0
#endif

static int
test_runner_setenv_portable(char const *name, char const *value);

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
# define TEST_RUNNER_SIGINT_SYNC_FD_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_FD"
#endif

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
# define TEST_RUNNER_SIGINT_SYNC_PID_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_PID"
# define TEST_RUNNER_SIGINT_SYNC_EVENT_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_EVENT"
# define TEST_RUNNER_SIGINT_SYNC_EVENT_DISPATCH_START \
    "callback_dispatch_start"

static volatile sig_atomic_t test_runner_sigint_sync_ready;

static void
test_runner_sigint_sync_handler(int signum)
{
    (void)signum;
    test_runner_sigint_sync_ready = 1;
}
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
int test_encoder_core_0001_encoder_core_factory(int argc, char **argv);
int test_encoder_core_0002_encoder_core_ormode_body_full_band(int argc,
                                                              char **argv);
int test_encoder_core_0003_encoder_core_ormode_body_tail_band(int argc,
                                                              char **argv);
int test_encoder_core_0004_encoder_core_ormode_body_bad_argument(int argc,
                                                                 char **argv);
int test_sixel_writer_0001_sixel_writer_factory(int argc, char **argv);
int test_frame_0001_frame_factory(int argc, char **argv);
int test_chunk_0001_chunk_factory(int argc, char **argv);
int test_timeline_0001_timeline_logger_factory(int argc, char **argv);
int test_timeline_0002_timeline_parallel_encode_decode(int argc, char **argv);
int test_timeline_0003_timeline_clock_origin(int argc, char **argv);
int test_threadpool_0001_threadpool_service(int argc, char **argv);
int test_decoder_0001_decoder_parallel_split_after_newline(int argc,
                                                           char **argv);
int test_decoder_0002_decoder_ormode_raw_overlay(int argc, char **argv);
int test_decoder_0003_decoder_ormode_wide_overlay(int argc, char **argv);
int test_decoder_0004_decoder_ormode_direct_overlay(int argc, char **argv);
int test_decoder_0005_decoder_ormode_repeat_overlay(int argc, char **argv);
int test_decoder_0006_decoder_ormode_legacy_api(int argc, char **argv);
int test_dither_0001_dither_policy(int argc, char **argv);
int test_lookup_0001_lookup_policy(int argc, char **argv);

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
int test_palette_0004_palette_factory(int argc, char **argv);

int test_cli_0029_cli_token_is_known_option(int argc, char **argv);
int test_cli_0030_cli_option_requires_argument(int argc, char **argv);
int test_cli_0031_cli_guard_missing_argument(int argc, char **argv);
int test_security_0001_issue222_encoder_encode_bytes_packed_g1(int argc,
                                                                char **argv);

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
int test_loader_0060_loader_builtin_webp_anim_policy_limits_plan(int argc,
                                                                  char **argv);
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
    { "security/0001_issue222_encoder_encode_bytes_packed_g1",
      test_security_0001_issue222_encoder_encode_bytes_packed_g1 },
    { "encoder-core/0001_encoder_core_factory",
      test_encoder_core_0001_encoder_core_factory },
    { "encoder-core/0002_encoder_core_ormode_body_full_band",
      test_encoder_core_0002_encoder_core_ormode_body_full_band },
    { "encoder-core/0003_encoder_core_ormode_body_tail_band",
      test_encoder_core_0003_encoder_core_ormode_body_tail_band },
    { "encoder-core/0004_encoder_core_ormode_body_bad_argument",
      test_encoder_core_0004_encoder_core_ormode_body_bad_argument },
    { "sixel-writer/0001_sixel_writer_factory",
      test_sixel_writer_0001_sixel_writer_factory },
    { "frame/0001_frame_factory", test_frame_0001_frame_factory },
    { "chunk/0001_chunk_factory", test_chunk_0001_chunk_factory },
    { "timeline/0001_timeline_logger_factory",
      test_timeline_0001_timeline_logger_factory },
    { "timeline/0002_timeline_parallel_encode_decode",
      test_timeline_0002_timeline_parallel_encode_decode },
    { "timeline/0003_timeline_clock_origin",
      test_timeline_0003_timeline_clock_origin },
    { "threadpool/0001_threadpool_service",
      test_threadpool_0001_threadpool_service },
    { "decoder/0001_decoder_parallel_split_after_newline",
      test_decoder_0001_decoder_parallel_split_after_newline },
    { "decoder/0002_decoder_ormode_raw_overlay",
      test_decoder_0002_decoder_ormode_raw_overlay },
    { "decoder/0003_decoder_ormode_wide_overlay",
      test_decoder_0003_decoder_ormode_wide_overlay },
    { "decoder/0004_decoder_ormode_direct_overlay",
      test_decoder_0004_decoder_ormode_direct_overlay },
    { "decoder/0005_decoder_ormode_repeat_overlay",
      test_decoder_0005_decoder_ormode_repeat_overlay },
    { "decoder/0006_decoder_ormode_legacy_api",
      test_decoder_0006_decoder_ormode_legacy_api },
    { "dither/0001_dither_policy", test_dither_0001_dither_policy },
    { "lookup/0001_lookup_policy",
      test_lookup_0001_lookup_policy },
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
    { "palette/0004_palette_factory", test_palette_0004_palette_factory },
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
    { "loader/0060_loader_builtin_webp_anim_policy_limits_plan",
      test_loader_0060_loader_builtin_webp_anim_policy_limits_plan },
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
            "       %s --win32-ctrl-break-run <delay-ms> <timeout-ms|0> "
            "<program> [args...]\n",
            program);
    fprintf(stderr,
            "       %s --sigint-run <delay-ms> <timeout-ms|0> "
            "<program> [args...]\n",
            program);
    fprintf(stderr,
            "       %s --sigint-run-until <needle> <timeout-ms|0> "
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

#if defined(_WIN32) || defined(__CYGWIN__)
/*
 * CreateProcessA requires native Win32 drive paths.
 * Convert Cygwin/MSYS drive forms so child launch stays stable when
 * this runner is invoked from POSIX-like shells.
 */
static char *
test_runner_duplicate_win32_path(char const *path)
{
    size_t source_length;
    size_t source_index;
    size_t drive_index;
    size_t payload_length;
    size_t target_length;
    size_t index;
    char *converted;
    int convert_drive;

    source_length = 0u;
    source_index = 0u;
    drive_index = 0u;
    payload_length = 0u;
    target_length = 0u;
    index = 0u;
    converted = NULL;
    convert_drive = 0;

    if (path == NULL) {
        return NULL;
    }

    source_length = strlen(path);
    if (source_length >= 11u &&
            strncmp(path, "/cygdrive/", 10u) == 0 &&
            isalpha((unsigned char)path[10]) &&
            (path[11] == '/' || path[11] == '\\' || path[11] == '\0')) {
        drive_index = 10u;
        source_index = 11u;
        convert_drive = 1;
    } else if (source_length >= 2u && path[0] == '/' &&
               isalpha((unsigned char)path[1]) &&
               (path[2] == '/' || path[2] == '\\' || path[2] == '\0')) {
        drive_index = 1u;
        source_index = 2u;
        convert_drive = 1;
    }

    if (!convert_drive) {
        converted = (char *)malloc(source_length + 1u);
        if (converted == NULL) {
            return NULL;
        }
        memcpy(converted, path, source_length + 1u);
        return converted;
    }

    /*
     * Skip the optional slash after the drive token while keeping drive_index
     * anchored to the original letter position.
     */
    if (source_index < source_length &&
            (path[source_index] == '/' || path[source_index] == '\\')) {
        source_index += 1u;
    }

    if (source_index > source_length) {
        return NULL;
    }
    if (drive_index >= source_length
            || !isalpha((unsigned char)path[drive_index])) {
        return NULL;
    }

    /*
     * Keep allocation and copy bounds in the same unit after removing
     * an optional separator so static analyzers can track the limit.
     */
    payload_length = source_length - source_index;
    target_length = payload_length + 4u;
    converted = (char *)malloc(target_length);
    if (converted == NULL) {
        return NULL;
    }

    converted[0] = (char)toupper((unsigned char)path[drive_index]);
    converted[1] = ':';
    converted[2] = '\\';
    index = 3u;

    while (payload_length > 0u) {
        if (index + 1u >= target_length) {
            free(converted);
            return NULL;
        }
        if (path[source_index] == '/') {
            converted[index] = '\\';
        } else {
            converted[index] = path[source_index];
        }
        ++source_index;
        ++index;
        --payload_length;
    }
    if (index >= target_length) {
        free(converted);
        return NULL;
    }
    converted[index] = '\0';

    return converted;
}
#endif

static int
test_runner_run_windows_ctrl_break(int argc, char **argv)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    char const *delay_token;
    char const *timeout_token;
    unsigned long parsed_delay;
    unsigned long parsed_timeout;
    char *delay_end;
    char *timeout_end;
    char **command_tokens;
    size_t command_token_count;
    char const *program;
    char const *token;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    char *command_line;
    size_t command_line_length;
    size_t index;
    size_t token_index;
    size_t token_length;
    char *cursor;
    char const *token_cursor;
    size_t backslash_count;
    DWORD wait_result;
    DWORD wait_timeout;
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
    command_tokens = NULL;
    command_token_count = 0u;
    program = NULL;
    token = NULL;
    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    command_line = NULL;
    command_line_length = 1u;
    index = 0u;
    token_index = 0u;
    token_length = 0u;
    cursor = NULL;
    token_cursor = NULL;
    backslash_count = 0u;
    wait_result = WAIT_FAILED;
    wait_timeout = INFINITE;
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

    command_token_count = (size_t)argc - 3u;
    command_tokens = (char **)calloc(command_token_count, sizeof(char *));
    if (command_tokens == NULL) {
        fprintf(stderr, "test_runner: failed to allocate token array\n");
        return EXIT_FAILURE;
    }

    for (index = 0u; index < command_token_count; ++index) {
        command_tokens[index] =
            test_runner_duplicate_win32_path(argv[index + 3u]);
        if (command_tokens[index] == NULL) {
            fprintf(stderr,
                    "test_runner: failed to normalize command token\n");
            goto cleanup;
        }
    }
    program = command_tokens[0];

    for (index = 0u; index < command_token_count; ++index) {
        token = command_tokens[index];
        token_length = strlen(token);
        command_line_length += 3u + (token_length * 2u);
    }

    command_line = (char *)malloc(command_line_length);
    if (command_line == NULL) {
        fprintf(stderr, "test_runner: failed to allocate command line\n");
        return EXIT_FAILURE;
    }

    cursor = command_line;
    for (index = 0u; index < command_token_count; ++index) {
        if (index != 0u) {
            *cursor++ = ' ';
        }
        *cursor++ = '"';
        token_cursor = command_tokens[index];
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

    if (parsed_timeout > 0ul) {
        wait_timeout = (DWORD)parsed_timeout;
    }
    wait_result = WaitForSingleObject(process_info.hProcess,
                                      wait_timeout);
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
    if (command_tokens != NULL) {
        for (token_index = 0u; token_index < command_token_count;
                ++token_index) {
            free(command_tokens[token_index]);
        }
        free(command_tokens);
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
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
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

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
/*
 * Some runtimes (notably APE wrappers) do not always propagate signals sent
 * only to the process group. Send to both the process group and the direct
 * child so SIGINT/SIGKILL delivery remains stable across runtimes.
 */
static int
test_runner_signal_group_and_child(pid_t child_pid, int signum)
{
    int group_result;
    int child_result;
    int group_errno;
    int child_errno;
    pid_t child_pgid;
    int should_signal_child;

    group_result = 0;
    child_result = 0;
    group_errno = 0;
    child_errno = 0;
    child_pgid = (pid_t)-1;
    should_signal_child = 1;

    group_result = kill((pid_t)(-child_pid), signum);
    if (group_result != 0) {
        group_errno = errno;
    }

    if (group_result == 0) {
        child_pgid = getpgid(child_pid);
        if (child_pgid == child_pid) {
            should_signal_child = 0;
        }
    }

    /*
     * Avoid delivering the same signal twice to the group leader.
     * Keep direct-child fallback when the process-group relationship
     * is unknown so wrapped runtimes still get signal delivery.
     */
    if (should_signal_child != 0) {
        child_result = kill(child_pid, signum);
        if (child_result != 0) {
            child_errno = errno;
        }
    }

    if ((group_errno == 0 || group_errno == ESRCH)
            && (child_errno == 0 || child_errno == ESRCH)) {
        return 0;
    }

    if (child_errno != 0 && child_errno != ESRCH) {
        errno = child_errno;
    } else {
        errno = group_errno;
    }
    return -1;
}
#endif

static int
test_runner_run_posix_sigint(int argc, char **argv)
{
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
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

    send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
    if (send_result != 0) {
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
        if (parsed_timeout > 0ul && elapsed_ms >= parsed_timeout) {
            fprintf(stderr,
                    "test_runner: timeout waiting child after SIGINT\n");
            goto timeout_cleanup;
        }
        if (test_runner_sleep_milliseconds(poll_interval_ms) != 0) {
            fprintf(stderr, "test_runner: nanosleep failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout > 0ul) {
            if (elapsed_ms > parsed_timeout - poll_interval_ms) {
                elapsed_ms = parsed_timeout;
            } else {
                elapsed_ms += poll_interval_ms;
            }
        }
    }

    return EXIT_SUCCESS;

timeout_cleanup:
    kill_result = test_runner_signal_group_and_child(child_pid, SIGKILL);
    if (kill_result != 0) {
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

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
static int
test_runner_buffer_contains(char const *buffer,
                            size_t buffer_length,
                            char const *needle,
                            size_t needle_length)
{
    size_t offset;
    size_t index;

    offset = 0u;
    index = 0u;

    if (needle_length == 0u) {
        return 1;
    }
    if (buffer == NULL || needle == NULL) {
        return 0;
    }
    if (buffer_length < needle_length) {
        return 0;
    }

    for (offset = 0u; offset + needle_length <= buffer_length; ++offset) {
        for (index = 0u; index < needle_length; ++index) {
            if (buffer[offset + index] != needle[index]) {
                break;
            }
        }
        if (index == needle_length) {
            return 1;
        }
    }

    return 0;
}
#endif

static int
test_runner_run_posix_sigint_until(int argc, char **argv)
{
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
    char const *needle_token;
    char const *timeout_token;
    unsigned long parsed_timeout;
    char *timeout_end;
    char const *program;
    char **program_argv;
    size_t needle_length;
    size_t carry_length;
    size_t combined_length;
    size_t copy_length;
    ssize_t read_size;
    int stderr_pipe[2];
    int stderr_read_open;
    int stderr_write_open;
    int stderr_closed;
    pid_t child_pid;
    pid_t wait_pid;
    int wait_status;
    int child_running;
    unsigned long elapsed_ms;
    unsigned long poll_interval_ms;
    int send_result;
    int kill_result;
    int signal_sent;
    int found_trigger;
    int sync_pipe[2];
    int sync_read_open;
    int sync_write_open;
    int sync_closed;
    int sync_fd_flags;
    int sync_fd_size;
    ssize_t sync_read_size;
    char sync_fd_text[32];
    char sync_byte;
    char read_buffer[1024];
    char *combined_buffer;
    size_t combined_capacity;
    int fd_flags;
    int sleep_status;
    int read_progress;
    int token_matched;
    unsigned long resend_interval_ms;
    unsigned long resend_elapsed_ms;
#if HAVE_SIGNAL_H
    struct sigaction sync_action;
    struct sigaction sync_old_action;
    pid_t parent_pid;
    int sync_action_installed;
    int sync_signal_supported;
    int sync_env_status;
    int sync_pid_size;
    char sync_pid_text[32];
#endif

    needle_token = NULL;
    timeout_token = NULL;
    parsed_timeout = 0ul;
    timeout_end = NULL;
    program = NULL;
    program_argv = NULL;
    needle_length = 0u;
    carry_length = 0u;
    combined_length = 0u;
    copy_length = 0u;
    read_size = 0;
    stderr_pipe[0] = -1;
    stderr_pipe[1] = -1;
    stderr_read_open = 0;
    stderr_write_open = 0;
    stderr_closed = 0;
    child_pid = (pid_t)0;
    wait_pid = (pid_t)0;
    wait_status = 0;
    child_running = 1;
    elapsed_ms = 0ul;
    poll_interval_ms = 10ul;
    send_result = 0;
    kill_result = 0;
    signal_sent = 0;
    found_trigger = 0;
    sync_pipe[0] = -1;
    sync_pipe[1] = -1;
    sync_read_open = 0;
    sync_write_open = 0;
    sync_closed = 0;
    sync_fd_flags = 0;
    sync_fd_size = 0;
    sync_read_size = 0;
    sync_fd_text[0] = '\0';
    sync_byte = '\0';
    combined_buffer = NULL;
    combined_capacity = 0u;
    fd_flags = 0;
    sleep_status = 0;
    read_progress = 0;
    token_matched = 0;
    resend_interval_ms = 200ul;
    resend_elapsed_ms = 0ul;
#if HAVE_SIGNAL_H
    memset(&sync_action, 0, sizeof(sync_action));
    memset(&sync_old_action, 0, sizeof(sync_old_action));
    parent_pid = (pid_t)0;
    sync_action_installed = 0;
    sync_signal_supported = 0;
    sync_env_status = 0;
    sync_pid_size = 0;
    sync_pid_text[0] = '\0';
#endif

    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <needle> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    needle_token = argv[1];
    timeout_token = argv[2];
    program = argv[3];
    program_argv = argv + 3;
    needle_length = strlen(needle_token);

    if (needle_length == 0u) {
        fprintf(stderr, "test_runner: --sigint-run-until needs a token\n");
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

#if HAVE_SIGNAL_H
    /*
     * Use a direct signal handshake when available so SIGINT trigger timing
     * does not depend on runtime-specific stderr forwarding behavior.
     */
    test_runner_sigint_sync_ready = 0;
    sync_action.sa_handler = test_runner_sigint_sync_handler;
    (void)sigemptyset(&sync_action.sa_mask);
# if defined(SA_RESTART)
    sync_action.sa_flags = SA_RESTART;
# else
    sync_action.sa_flags = 0;
# endif
    if (sigaction(SIGUSR1, &sync_action, &sync_old_action) == 0) {
        sync_action_installed = 1;
        sync_signal_supported = 1;
    }

    if (sync_signal_supported != 0) {
        parent_pid = getpid();
        sync_pid_size = snprintf(sync_pid_text,
                                 sizeof(sync_pid_text),
                                 "%ld",
                                 (long)parent_pid);
        if (sync_pid_size <= 0
                || (size_t)sync_pid_size >= sizeof(sync_pid_text)) {
            sync_signal_supported = 0;
        }
    }

    if (sync_signal_supported != 0) {
        sync_env_status = test_runner_setenv_portable(
            TEST_RUNNER_SIGINT_SYNC_EVENT_ENVVAR,
            TEST_RUNNER_SIGINT_SYNC_EVENT_DISPATCH_START);
        if (sync_env_status != 0) {
            sync_signal_supported = 0;
        }
    }
    if (sync_signal_supported != 0) {
        sync_env_status = test_runner_setenv_portable(
            TEST_RUNNER_SIGINT_SYNC_PID_ENVVAR,
            sync_pid_text);
        if (sync_env_status != 0) {
            sync_signal_supported = 0;
        }
    }
#endif

    combined_capacity = needle_length + sizeof(read_buffer);
    combined_buffer = (char *)malloc(combined_capacity);
    if (combined_buffer == NULL) {
        fprintf(stderr,
                "test_runner: failed to allocate sigint trace buffer\n");
        return EXIT_FAILURE;
    }

    /*
     * Expose a dedicated one-byte handshake pipe so callback-handoff
     * synchronization does not depend on stderr buffering or signal runtime
     * behavior. Child-side writes happen in encoder.c at dispatch-start.
     */
    if (pipe(sync_pipe) != 0) {
        fprintf(stderr, "test_runner: sync pipe failed: %s\n",
                strerror(errno));
        goto timeout_cleanup;
    }
    sync_read_open = 1;
    sync_write_open = 1;

    sync_fd_flags = fcntl(sync_pipe[1], F_GETFD, 0);
    if (sync_fd_flags >= 0) {
        (void)fcntl(sync_pipe[1], F_SETFD, sync_fd_flags & ~FD_CLOEXEC);
    }

    sync_fd_flags = fcntl(sync_pipe[0], F_GETFL, 0);
    if (sync_fd_flags >= 0) {
        (void)fcntl(sync_pipe[0], F_SETFL, sync_fd_flags | O_NONBLOCK);
    }

    sync_fd_size = snprintf(sync_fd_text,
                            sizeof(sync_fd_text),
                            "%d",
                            sync_pipe[1]);
    if (sync_fd_size <= 0
            || (size_t)sync_fd_size >= sizeof(sync_fd_text)) {
        fprintf(stderr,
                "test_runner: failed to encode sigint sync fd\n");
        goto timeout_cleanup;
    }
    if (test_runner_setenv_portable(TEST_RUNNER_SIGINT_SYNC_FD_ENVVAR,
                                    sync_fd_text) != 0) {
        fprintf(stderr,
                "test_runner: failed to export sigint sync fd env\n");
        goto timeout_cleanup;
    }

    if (pipe(stderr_pipe) != 0) {
        fprintf(stderr, "test_runner: pipe failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }
    stderr_read_open = 1;
    stderr_write_open = 1;

    child_pid = fork();
    if (child_pid < (pid_t)0) {
        fprintf(stderr, "test_runner: fork failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }

    if (child_pid == (pid_t)0) {
        (void)setpgid(0, 0);
        (void)close(sync_pipe[0]);
        (void)close(stderr_pipe[0]);
        (void)dup2(stderr_pipe[1], STDERR_FILENO);
        (void)close(stderr_pipe[1]);
        (void)execvp(program, program_argv);
        fprintf(stderr, "test_runner: execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (setpgid(child_pid, child_pid) != 0
        && errno != EACCES && errno != ESRCH) {
        fprintf(stderr, "test_runner: setpgid failed: %s\n", strerror(errno));
    }

    (void)close(stderr_pipe[1]);
    stderr_write_open = 0;
    (void)close(sync_pipe[1]);
    sync_write_open = 0;

    fd_flags = fcntl(stderr_pipe[0], F_GETFL, 0);
    if (fd_flags >= 0) {
        (void)fcntl(stderr_pipe[0], F_SETFL, fd_flags | O_NONBLOCK);
    }

    if (parsed_timeout > 0ul && poll_interval_ms > parsed_timeout) {
        poll_interval_ms = parsed_timeout;
    }

    for (;;) {
        read_progress = 0;
        if (signal_sent == 0 && sync_closed == 0) {
            for (;;) {
                sync_read_size = read(sync_pipe[0], &sync_byte, 1u);
                if (sync_read_size > 0) {
                    found_trigger = 1;
                    send_result =
                        test_runner_signal_group_and_child(child_pid, SIGINT);
                    if (send_result != 0) {
                        fprintf(stderr,
                                "test_runner: SIGINT send failed: %s\n",
                                strerror(errno));
                    }
                    signal_sent = 1;
                    /*
                     * Keep independent timeout budgets for trigger detection
                     * and post-SIGINT shutdown so slow trace startup does not
                     * consume the child-exit grace window.
                     */
                    elapsed_ms = 0ul;
                    resend_elapsed_ms = 0ul;
                    break;
                }
                if (sync_read_size == 0) {
                    sync_closed = 1;
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                fprintf(stderr, "test_runner: sync read failed: %s\n",
                        strerror(errno));
                goto timeout_cleanup;
            }
        }
#if HAVE_SIGNAL_H
        if (signal_sent == 0
                && sync_signal_supported != 0
                && test_runner_sigint_sync_ready != 0) {
            found_trigger = 1;
            send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
            if (send_result != 0) {
                fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                        strerror(errno));
            }
            signal_sent = 1;
            /*
             * Keep independent timeout budgets for trigger detection
             * and post-SIGINT shutdown so slow trigger setup does not
             * consume the child-exit grace window.
             */
            elapsed_ms = 0ul;
            resend_elapsed_ms = 0ul;
        }
#endif
        wait_pid = waitpid(child_pid, &wait_status, WNOHANG);
        if (wait_pid == child_pid) {
            child_running = 0;
        } else if (wait_pid < (pid_t)0) {
            if (errno == EINTR) {
                /* Retry the loop after interrupted waitpid calls. */
            } else if (errno == ECHILD) {
                /*
                 * Some wrapper runtimes reap direct children eagerly.
                 * Treat this the same as observing normal child exit.
                 */
                child_running = 0;
            } else {
                fprintf(stderr, "test_runner: waitpid failed: %s\n",
                        strerror(errno));
                goto timeout_cleanup;
            }
        }

        for (;;) {
            read_size = read(stderr_pipe[0],
                             read_buffer,
                             sizeof(read_buffer));
            if (read_size > 0) {
                read_progress = 1;
                if (fwrite(read_buffer,
                           1u,
                           (size_t)read_size,
                           stderr) != (size_t)read_size) {
                    fprintf(stderr, "test_runner: stderr relay failed\n");
                    goto timeout_cleanup;
                }
                fflush(stderr);

                memcpy(combined_buffer + carry_length,
                       read_buffer,
                       (size_t)read_size);
                combined_length = carry_length + (size_t)read_size;
                token_matched = test_runner_buffer_contains(
                    combined_buffer,
                    combined_length,
                    needle_token,
                    needle_length);
                if (token_matched != 0) {
                    found_trigger = 1;
                    /*
                     * Send the first SIGINT at token detection. Subsequent
                     * retries are rate-limited in the main loop to avoid
                     * per-frame signal storms on looped animations.
                     */
                    if (signal_sent == 0) {
                        send_result = test_runner_signal_group_and_child(
                            child_pid,
                            SIGINT);
                        if (send_result != 0) {
                            fprintf(stderr,
                                    "test_runner: SIGINT send failed: %s\n",
                                    strerror(errno));
                        }
                        signal_sent = 1;
                        /*
                         * Keep independent timeout budgets for trigger
                         * detection and post-SIGINT shutdown so slow trace
                         * startup does not consume the child-exit grace
                         * window.
                         */
                        elapsed_ms = 0ul;
                        resend_elapsed_ms = 0ul;
                    }
                }

                if (needle_length > 1u) {
                    if (combined_length >= needle_length - 1u) {
                        copy_length = needle_length - 1u;
                        memmove(combined_buffer,
                                combined_buffer + combined_length
                                - copy_length,
                                copy_length);
                        carry_length = copy_length;
                    } else {
                        carry_length = combined_length;
                    }
                } else {
                    carry_length = 0u;
                }
                continue;
            }
            if (read_size == 0) {
                stderr_closed = 1;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            fprintf(stderr, "test_runner: read failed: %s\n", strerror(errno));
            goto timeout_cleanup;
        }

        if (signal_sent == 0 && child_running == 0) {
            fprintf(stderr,
                    "test_runner: child exited before SIGINT trigger\n");
            goto timeout_cleanup;
        }
        if (signal_sent != 0 && child_running == 0) {
            /*
             * Drain relayed stderr while data is still flowing, but stop
             * waiting once the direct child has exited and no more bytes are
             * immediately readable. Some wrappers keep inherited write ends
             * open in helper descendants, so waiting only for EOF can stall.
             */
            if (stderr_closed != 0 || read_progress == 0) {
                break;
            }
            continue;
        }
        if (signal_sent != 0
                && child_running != 0
                && resend_elapsed_ms >= resend_interval_ms) {
            /*
             * Keep nudging wrapped runtimes that can drop the first SIGINT
             * forwarded from the parent process tree.
             */
            send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
            if (send_result != 0) {
                fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                        strerror(errno));
            }
            resend_elapsed_ms = 0ul;
        }
        if (parsed_timeout > 0ul && elapsed_ms >= parsed_timeout) {
            if (found_trigger == 0) {
                fprintf(stderr,
                        "test_runner: timeout waiting for trigger token\n");
            } else {
                fprintf(stderr,
                        "test_runner: timeout waiting child after SIGINT\n");
            }
            goto timeout_cleanup;
        }

        sleep_status = test_runner_sleep_milliseconds(poll_interval_ms);
        if (sleep_status != 0) {
            fprintf(stderr, "test_runner: nanosleep failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout > 0ul) {
            if (elapsed_ms > parsed_timeout - poll_interval_ms) {
                elapsed_ms = parsed_timeout;
            } else {
                elapsed_ms += poll_interval_ms;
            }
        }
        if (signal_sent != 0 && child_running != 0) {
            if (resend_elapsed_ms > resend_interval_ms - poll_interval_ms) {
                resend_elapsed_ms = resend_interval_ms;
            } else {
                resend_elapsed_ms += poll_interval_ms;
            }
        }
    }

    if (stderr_read_open != 0) {
        (void)close(stderr_pipe[0]);
    }
    if (sync_read_open != 0) {
        (void)close(sync_pipe[0]);
    }
#if HAVE_SIGNAL_H
    if (sync_action_installed != 0) {
        (void)sigaction(SIGUSR1, &sync_old_action, NULL);
    }
#endif
    free(combined_buffer);
    return EXIT_SUCCESS;

timeout_cleanup:
    if (child_pid > (pid_t)0) {
        kill_result = test_runner_signal_group_and_child(child_pid, SIGKILL);
        if (kill_result != 0) {
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
    }
    if (stderr_read_open != 0) {
        (void)close(stderr_pipe[0]);
    }
    if (stderr_write_open != 0) {
        (void)close(stderr_pipe[1]);
    }
    if (sync_read_open != 0) {
        (void)close(sync_pipe[0]);
    }
    if (sync_write_open != 0) {
        (void)close(sync_pipe[1]);
    }
#if HAVE_SIGNAL_H
    if (sync_action_installed != 0) {
        (void)sigaction(SIGUSR1, &sync_old_action, NULL);
    }
#endif
    free(combined_buffer);
    return EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr, "test_runner: --sigint-run-until is unavailable\n");
    return EXIT_FAILURE;
#endif
}

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
    if (strcmp(argv[first_index], "--sigint-run-until") == 0) {
        return test_runner_run_posix_sigint_until(argc - first_index,
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
