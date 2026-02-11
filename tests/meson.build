sh = find_program('sh')
artifact_root = meson.current_build_dir() / '_artifacts'
cross_compiling = 'no'
if meson.is_cross_build()
  cross_compiling = 'yes'
endif

sanitize_opt = get_option('b_sanitize')
sixel_tsan_build = 'no'
sixel_msan_build = 'no'
if sanitize_opt.contains('thread')
  sixel_tsan_build = 'yes'
endif
if sanitize_opt.contains('memory')
  sixel_msan_build = 'yes'
endif

bin_ext = ''
if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  bin_ext = '.exe'
endif

base_env_settings = [
  ['ARTIFACT_ROOT', artifact_root],
  ['TOP_SRCDIR', meson.project_source_root()],
  ['TOP_BUILDDIR', meson.project_build_root()],
  ['MESON_SOURCE_ROOT', meson.project_source_root()],
  ['MESON_BUILD_ROOT', meson.project_build_root()],
  ['CROSS_COMPILING', cross_compiling],
  ['SIXEL_TSAN_BUILD', sixel_tsan_build],
  ['SIXEL_MSAN_BUILD', sixel_msan_build],
  ['ENABLE_PYTHON', (get_option('python').disabled() ? '0' : '1')],
  # Mirror autotools check-TESTS metadata so TAP helper scripts can share
  # one execution path across build systems.
  # Ensure Python TAP tests can locate the freshly built shared library when
  # Meson is invoked without a prior `meson compile`. The binding smoke test
  # relies on LIBSIXEL_LIBDIR to point at the build artifact directory.
  ['LIBSIXEL_LIBDIR', meson.project_build_root() / 'src'],
  ['IMG2SIXEL_PATH', meson.project_build_root() / 'converters' /
   ('img2sixel' + bin_ext)],
  ['SIXEL2PNG_PATH', meson.project_build_root() / 'converters' /
   ('sixel2png' + bin_ext)],
  ['LSQA_PATH', meson.project_build_root() / 'assessment' /
   ('lsqa' + bin_ext)],
  ['TEST_RUNNER_PATH', meson.project_build_root() / 'tests' /
   ('test_runner' + bin_ext)],
]

# Meson sets runtime library lookup via rpath/runpath, so test wrappers must
# not override PATH/LD_LIBRARY_PATH/DYLD_LIBRARY_PATH in this execution path.
base_env_settings += [['SIXEL_SHLIBPATH_OVERRIDES_RUNPATH', 'no']]

# Keep feature toggle exports in sync with autotools check-TESTS.
config_macro_env_settings = []
config_macro_env_text = run_command(
  python3,
  '-c',
  '''import pathlib
import re
import sys

config_h = pathlib.Path(sys.argv[1])
if not config_h.exists():
    raise SystemExit(0)

for line in config_h.read_text(encoding="utf-8", errors="ignore").splitlines():
    m_define = re.match(r"^#define ([A-Z0-9_]+) 1$", line)
    if m_define:
        print(f"{m_define.group(1)}=1")
        continue
    m_undef = re.match(r"^/\\* #undef ([A-Z0-9_]+) \\*/$", line)
    if m_undef:
        print(f"{m_undef.group(1)}=0")
''',
  meson.project_build_root() / 'config.h',
  check: true,
).stdout().strip()
if config_macro_env_text != ''
  foreach macro_line : config_macro_env_text.split('\n')
    macro_pair = macro_line.split('=')
    if macro_pair.length() == 2
      config_macro_env_settings += [[macro_pair[0], macro_pair[1]]]
    endif
  endforeach
endif
base_env_settings += config_macro_env_settings

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  # Most TAP shell wrappers reference built tools via ${SIXEL_BIN_EXT}.
  # Meson does not export this automatically, so Windows jobs otherwise look
  # up extension-less paths and mark many tests as skipped.
  base_env_settings += [['SIXEL_BIN_EXT', '.exe']]
endif

env = environment()
foreach setting : base_env_settings
  env.set(setting[0], setting[1])
endforeach

test_runner_sources = [
  'test_runner.c',
  'cli/aborttrace/0001_img2sixel_aborttrace.c',
  'cli/0029_cli_token_is_known_option.c',
  'cli/0030_cli_option_requires_argument.c',
  'cli/0031_cli_guard_missing_argument.c',
  'processing/filter/0001_filter_clip.c',
  'processing/filter/0002_filter_sample.c',
  'processing/filter/0003_filter_resize.c',
  'processing/filter/0004_filter_colors.c',
  'processing/filter/0005_filter_lookup.c',
  'processing/filter/0006_filter_final_merge.c',
  'processing/filter/0007_filter_vpte.c',
  'processing/filter/0008_filter_load.c',
  'processing/filter/0009_filter_dither.c',
  'processing/filter/0010_filter_encode.c',
  'processing/filter/filter_test_common.h',
  'io/loader/0008_loader_coregraphics_pixelformat.c',
  'io/loader/0009_loader_wic_pixelformat.c',
  'io/loader/0010_loader_gdk_pixbuf_pixelformat.c',
  'io/loader/0011_loader_gd_pixelformat.c',
  'io/loader/0012_loader_libpng_pixelformat.c',
  'io/loader/0013_loader_libjpeg_pixelformat.c',
  'io/loader/0014_loader_builtin_pixelformat.c',
  'io/loader/0015_loader_quicklook_pixelformat.c',
  'io/loader/0016_loader_gnome_thumbnailer_pixelformat.c',
  'io/loader/0017_loader_libwebp_pixelformat.c',
  'io/loader/pixelformat_test_common.h',
  'quant/palette/0001_kmeans_init.c',
  'diagnostics/probe/0001_probe_parse.c',
  'extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c',
  'extension/gdk-pixbuf-loader/0002_incremental_load.c',
  'extension/gdk-pixbuf-loader/0003_corrupt_data.c',
  'extension/gdk-pixbuf-loader/0004_propagate_error.c',
  'extension/gdk-pixbuf-loader/0005_context_free.c',
  'extension/gdk-pixbuf-loader/0006_loader_module.c',
  'extension/gdk-pixbuf-loader/0007_loader_plugin_integration.c',
  '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  '../converters/aborttrace.c',
  '../converters/cli.c',
]

test_runner_inc = [
  project_inc,
  include_directories('..'),
]

test_runner_exe = executable('test_runner', test_runner_sources,
  include_directories: test_runner_inc,
  dependencies: [libsixel_dep, gdk_pixbuf_loader_deps, abort_trace_deps],
  c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND',
           '-DSIXEL_PIXBUF_LOADER_TESTING',
           '-DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS'],
  install: false)

test_deps = [test_runner_exe, lsqa_bin] + img2sixel_deps + sixel2png_deps
if not get_option('python').disabled()
  test_deps += [python_wheel_target]
endif

is_windows = host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
if is_windows
  # Ensure Windows-style runners (including MSYS2) locate the freshly built
  # DLLs when executing the converter TAP suite. Without these PATH entries
  # the tests fail with "cannot open shared object file" errors because
  # libsixel-1.dll or msys-sixel-1.dll is not discoverable beside the
  # converter executables.
  dll_paths = [meson.project_build_root() / 'src',
               meson.project_build_root() / 'converters']
  foreach dll_path : dll_paths
    env.prepend('PATH', dll_path)
  endforeach
endif

tests = [
  'cli/aborttrace/0001_img2sixel_aborttrace.t',
  'codec/decoder/animation/0001_disable_update_mode.t',
  'codec/decoder/animation/0002_static_frame_render.t',
  'codec/decoder/animation/0003_update_and_static_frame.t',
  'codec/decoder/animation/0004_split_sequence_atkinson.t',
  'cli/argument-shift/0001_invalid_positional.t',
  'cli/argument-shift/0002_argument_shift_missing_mapfile.t',
  'cli/argument-shift/0003_argument_shift_outfile_named_like_option.t',
  'cli/argument-shift/0005_cli_token_is_known_option.t',
  'cli/argument-shift/0006_cli_option_requires_argument.t',
  'cli/argument-shift/0007_cli_guard_missing_argument.t',
  'cli/core/0001_help.t',
  'cli/core/0002_version.t',
  'cli/core/0003_stdin.t',
  'cli/core/0004_missing_input_path_suggestions.t',
  'cli/core/0005_basic_help_option.t',
  'cli/core/0006_basic_version_option.t',
  'cli/core/0007_basic_stdin_snake.t',
  'cli/core/0008_basic_stdin_map8.t',
  'cli/core/0009_basic_stdin_stdout_map64.t',
  'cli/core/0010_basic_file_arguments_snake.t',
  'cli/core/0011_basic_direct_output.t',
  'cli/core/0012_basic_direct_with_dequantize_conflict.t',
  'cli/core/0013_basic_png_prefix_path.t',
  'cli/core/0014_basic_png_stdout.t',
  'cli/core/0015_basic_png_prefix_missing_target.t',
  'cli/core/0016_basic_dequantize_unique_prefix.t',
  'cli/core/0017_basic_parallel_indexed.t',
  'cli/core/0018_basic_parallel_direct.t',
  'cli/core/0019_basic_parallel_cli_override.t',
  'cli/core/0020_basic_invalid_threads_token.t',
  'cli/options/invalid/0001_invalid_option_arguments_invalid_width.t',
  'cli/options/invalid/0002_invalid_option_arguments_invalid_height.t',
  'cli/options/invalid/0003_invalid_option_arguments_invalid_findtype.t',
  'cli/options/invalid/0004_invalid_option_arguments_invalid_quality.t',
  'cli/options/invalid/0005_invalid_option_arguments_invalid_loop_mode.t',
  'cli/options/invalid/0006_invalid_option_arguments_invalid_builtin_palette.t',
  'cli/options/invalid/0007_invalid_option_arguments_invalid_encode_policy.t',
  'cli/options/invalid/0008_invalid_option_arguments_invalid_background_colour.t',
  'cli/options/invalid/0009_invalid_option_arguments_missing_background_component.t',
  'cli/options/invalid/0010_invalid_option_arguments_overlong_background.t',
  'cli/options/invalid/0011_invalid_option_arguments_malformed_hex_background.t',
  'cli/options/invalid/0012_invalid_option_arguments_unknown_named_colour.t',
  'cli/options/invalid/0013_invalid_option_arguments_incomplete_rgb_background.t',
  'cli/options/invalid/0014_invalid_option_arguments_unsupported_legacy_width.t',
  'cli/options/invalid/0015_invalid_option_arguments_missing_palette_file.t',
  'cli/options/invalid/0016_invalid_option_arguments_invalid_complexion_score.t',
  'cli/options/invalid/0017_invalid_option_arguments_unknown_dither.t',
  'cli/options/invalid/0018_invalid_option_arguments_unknown_resampling_option.t',
  'cli/options/invalid/0019_invalid_option_arguments_unknown_select_type_mode.t',
  'cli/options/invalid/0020_invalid_option_arguments_unknown_palette_type.t',
  'cli/options/invalid/0021_invalid_option_combinations_palette_and_encode.t',
  'cli/options/invalid/0022_invalid_option_combinations_high_color_with_palette.t',
  'cli/options/invalid/0023_invalid_option_combinations_palette_with_terminal_preset.t',
  'cli/options/invalid/0024_invalid_option_combinations_eight_bit_with_pipe_mode.t',
  'cli/options/matching/0001_option_matching_prefix_unique.t',
  'cli/options/matching/0002_option_matching_prefix_ambiguous.t',
  'cli/options/matching/0003_option_matching_diffusion_correction.t',
  'cli/options/matching/0004_option_matching_distance1_multi.t',
  'cli/options/matching/0005_option_matching_distance2.t',
  'cli/options/matching/0006_option_matching_distance3.t',
  'cli/options/matching/0007_option_matching_force_colors_ansi.t',
  'cli/options/matching/0008_option_matching_prefix_ambiguous_suggestions_off.t',
  'cli/options/matching/0009_option_matching_distance2_fuzzy_off.t',
  'cli/options/general/0001_long_option_forms.t',
  'cli/options/general/0002_ormode_option_runs.t',
  'cli/options/general/0003_gri_limit_lsqa.t',
  'io/clipboard/0001_clipboard_basic.t',
  'cli/completion/0021_completion_show_bash.t',
  'cli/completion/0022_completion_show_zsh.t',
  'cli/completion/0023_completion_install_bash.t',
  'cli/completion/0024_completion_install_zsh.t',
  'cli/completion/0025_completion_uninstall_bash.t',
  'cli/completion/0026_completion_uninstall_zsh.t',
  'cli/completion/0027_completion_show_all.t',
  'cli/completion/0028_completion_install_missing_target.t',
  'cli/completion/0029_completion_show_invalid_target.t',
  'cli/completion/0030_completion_install_bash_legacy.t',
  'cli/completion/0031_completion_install_zsh_idempotent.t',
  'codec/decoder/core/0004_tab_separated_colour_introducers.t',
  'codec/decoder/core/0005_oversized_dcs_geometry.t',
  'processing/dither/general/0001_roundtrip_dither_gamma.t',
  'processing/dither/general/0002_inspection_with_diffusion.t',
  'processing/dither/general/0003_ascii_ppm_inspection_diffusion.t',
  'processing/dither/general/0004_ascii_pbm_atkinson_inspection.t',
  'processing/dither/x-dither/0005_cropping_with_x_dither.t',
  'processing/dither/general/0006_cropping_with_alternate_dither.t',
  'processing/dither/general/0007_sixel_inspection_alternate_dither.t',
  'processing/dither/x-dither/0008_sixel_inspection_x_dither.t',
  'processing/dither/general/0009_lanczos2_without_diffusion.t',
  'processing/dither/general/0010_inspection_high_color_sierra1.t',
  'processing/dither/general/0011_inspection_high_color_sierra2.t',
  'processing/dither/general/0012_inspection_high_color_sierra3.t',
  'processing/dither/general/0013_inspection_high_color_rgba_input.t',
  'processing/dither/positional/0010_positional_8bit_dither.t',
  'processing/dither/positional/0011_positional_float32_dither.t',
  'processing/dither/varcoeff/0012_varcoeff_8bit_dither.t',
  'processing/dither/varcoeff/0013_varcoeff_float32_dither.t',
  'processing/dither/varcoeff/0014_varcoeff_8bit_serpentine_lsqa.t',
  'processing/dither/varcoeff/0015_varcoeff_8bit_carry_lsqa.t',
  'processing/dither/varcoeff/0016_varcoeff_8bit_carry_serpentine_lsqa.t',
  'processing/dither/varcoeff/0017_varcoeff_float32_carry_lsqa.t',
  'processing/dither/varcoeff/0018_varcoeff_float32_carry_serpentine_lsqa.t',
  'processing/dither/varcoeff/0019_varcoeff_float32_oklab_lsqa.t',
  'processing/dither/varcoeff/0020_varcoeff_float32_lookup_none_lsqa.t',
  'processing/dither/varcoeff/0021_varcoeff_float32_lookup_none_raster_lsqa.t',
  'processing/dither/x-dither/0020_positional_8bit_x_dither_lsqa.t',
  'processing/dither/positional/0021_positional_8bit_no_diffuse_lsqa.t',
  'processing/dither/a-dither/0022_positional_float32_a_dither_lsqa.t',
  'processing/dither/positional/0023_positional_float32_no_diffuse_lsqa.t',
  'processing/dither/positional/0024_positional_8bit_x_dither_serpentine.t',
  'processing/dither/positional/0025_positional_8bit_a_dither_forced_palette.t',
  'processing/dither/positional/0026_positional_8bit_bluenoise_lookup_none.t',
  'processing/dither/positional/0027_positional_8bit_a_dither_invalid_strength_env.t',
  'processing/dither/positional/0028_positional_8bit_x_dither_invalid_strength_env.t',
  'processing/dither/positional/0029_positional_8bit_bluenoise_phase_priority_env.t',
  'processing/dither/positional/0030_positional_8bit_bluenoise_seed_env.t',
  'processing/dither/positional/0031_positional_8bit_bluenoise_channel_rgb_env.t',
  'processing/dither/positional/0032_positional_8bit_bluenoise_invalid_size_env.t',
  'processing/dither/positional/0033_positional_float32_a_dither_invalid_strength_env.t',
  'processing/dither/positional/0034_positional_float32_x_dither_invalid_strength_env.t',
  'processing/dither/positional/0035_positional_float32_bluenoise_phase_priority_env.t',
  'processing/dither/positional/0036_positional_float32_bluenoise_seed_env.t',
  'processing/dither/positional/0037_positional_float32_bluenoise_channel_rgb_env.t',
  'processing/dither/positional/0038_positional_float32_bluenoise_invalid_size_env.t',
  'processing/dither/positional/0039_positional_float32_x_dither_serpentine.t',
  'processing/dither/positional/0040_positional_float32_x_dither_forced_palette.t',
  'processing/dither/positional/0041_positional_float32_x_dither_lookup_none.t',
  'processing/dither/positional/0042_positional_float32_x_dither_lookup_eytzinger.t',
  'processing/dither/fixed/0024_fixed_8bit_fs_carry_lsqa.t',
  'processing/dither/fixed/0025_fixed_8bit_fs_carry_serpentine_lsqa.t',
  'processing/dither/fixed/0026_fixed_8bit_atkinson_carry_lsqa.t',
  'processing/dither/fixed/0027_fixed_8bit_jajuni_carry_lsqa.t',
  'processing/dither/fixed/0028_fixed_8bit_stucki_carry_lsqa.t',
  'processing/dither/fixed/0029_fixed_8bit_burkes_carry_lsqa.t',
  'processing/dither/fixed/0030_fixed_8bit_sierra1_lsqa.t',
  'processing/dither/fixed/0031_fixed_8bit_sierra1_carry_lsqa.t',
  'processing/dither/fixed/0032_fixed_8bit_sierra2_lsqa.t',
  'processing/dither/fixed/0033_fixed_8bit_sierra2_carry_lsqa.t',
  'processing/dither/fixed/0034_fixed_8bit_sierra3_lsqa.t',
  'processing/dither/fixed/0035_fixed_8bit_sierra3_carry_lsqa.t',
  'processing/dither/fixed/0036_fixed_float32_fs_lsqa.t',
  'processing/dither/fixed/0037_fixed_float32_fs_serpentine_lsqa.t',
  'processing/dither/fixed/0038_fixed_float32_atkinson_lsqa.t',
  'processing/dither/fixed/0039_fixed_float32_jajuni_lsqa.t',
  'processing/dither/fixed/0040_fixed_float32_stucki_lsqa.t',
  'processing/dither/fixed/0041_fixed_float32_burkes_lsqa.t',
  'processing/dither/fixed/0042_fixed_float32_sierra1_lsqa.t',
  'processing/dither/fixed/0043_fixed_float32_sierra2_lsqa.t',
  'processing/dither/fixed/0044_fixed_float32_sierra3_lsqa.t',
  'processing/dither/fixed/0045_fixed_float32_lookup_none_lsqa.t',
  'processing/dither/fixed/0046_fixed_float32_lookup_none_complexion_lsqa.t',
  'processing/dither/bluenoise/0045_positional_8bit_bluenoise_gamma_lsqa.t',
  'processing/dither/bluenoise/0046_positional_float32_bluenoise_gamma_lsqa.t',
  'processing/dither/bluenoise/0047_positional_float32_bluenoise_oklab_lsqa.t',
  'processing/dither/bluenoise/0048_positional_float32_bluenoise_linear_lsqa.t',
  'processing/dither/bluenoise/0049_positional_float32_bluenoise_cielab_lsqa.t',
  'processing/dither/bluenoise/0050_positional_float32_bluenoise_din99d_lsqa.t',
  'processing/dither/a-dither/0051_positional_8bit_a_dither_gamma_lsqa.t',
  'processing/dither/a-dither/0052_positional_float32_a_dither_gamma_lsqa.t',
  'processing/dither/a-dither/0053_positional_float32_a_dither_oklab_lsqa.t',
  'processing/dither/a-dither/0054_positional_float32_a_dither_linear_lsqa.t',
  'processing/dither/a-dither/0055_positional_float32_a_dither_cielab_lsqa.t',
  'processing/dither/a-dither/0056_positional_float32_a_dither_din99d_lsqa.t',
  'processing/dither/x-dither/0057_positional_8bit_x_dither_gamma_lsqa.t',
  'processing/dither/x-dither/0058_positional_float32_x_dither_gamma_lsqa.t',
  'processing/dither/x-dither/0059_positional_float32_x_dither_oklab_lsqa.t',
  'processing/dither/x-dither/0060_positional_float32_x_dither_linear_lsqa.t',
  'processing/dither/x-dither/0061_positional_float32_x_dither_cielab_lsqa.t',
  'processing/dither/x-dither/0062_positional_float32_x_dither_din99d_lsqa.t',
  'docs/consistency/0001_help_vs_man.t',
  'docs/consistency/0002_man_vs_bash_completion.t',
  'docs/consistency/0003_bash_vs_zsh_completion.t',
  'docs/consistency/0004_envvars_vs_help.t',
  'processing/filter/0001_filter_clip.t',
  'processing/filter/0002_filter_sample.t',
  'processing/filter/0003_filter_resize.t',
  'processing/filter/0004_filter_colors.t',
  'processing/filter/0005_filter_lookup.t',
  'processing/filter/0006_filter_final_merge.t',
  'processing/filter/0007_filter_vpte.t',
  'processing/filter/0008_filter_load.t',
  'processing/filter/0009_filter_dither.t',
  'processing/filter/0010_filter_encode.t',
  'codec/format/0001_prefixed_png_output_default.t',
  'codec/format/0002_prefixed_png_output_explicit_path.t',
  'codec/format/0003_filename_driven_png_header.t',
  'codec/format/0004_gif_filters.t',
  'codec/format/0005_pgm_encode_flag.t',
  'codec/format/0006_gray1_output.t',
  'codec/format/0007_gray2_output.t',
  'codec/format/0008_gray4_output.t',
  'codec/format/0009_gray8_output.t',
  'codec/format/0010_png_inspection_color_space.t',
  'codec/format/0011_ascii_pgm_auto_encoder_inspection.t',
  'codec/format/0012_grayscale_png_conversion.t',
  'codec/format/0013_grayscale_png_inspection.t',
  'codec/format/0014_grayscale_jpeg_inspection.t',
  'codec/format/0015_bmp_inspection_with_filters.t',
  'extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.t',
  'extension/gdk-pixbuf-loader/0002_incremental_load.t',
  'extension/gdk-pixbuf-loader/0003_corrupt_data.t',
  'extension/gdk-pixbuf-loader/0004_propagate_error.t',
  'extension/gdk-pixbuf-loader/0005_context_free.t',
  'extension/gdk-pixbuf-loader/0006_loader_module.t',
  'extension/gdk-pixbuf-loader/0007_loader_plugin_integration.t',
  'processing/geometry/0001_scaling_histogram_background.t',
  'processing/geometry/0002_dcs_crc_width_scaling.t',
  'processing/geometry/0003_dcs_crc_height_scaling.t',
  'processing/geometry/0004_dcs_crc_height_scaling_auto_width.t',
  'processing/geometry/0005_dcs_crc_height_auto_width_absolute.t',
  'processing/geometry/0006_dcs_crc_height_absolute_width_percent.t',
  'processing/geometry/0007_tga_scaling.t',
  'processing/geometry/0008_indexed_png_scaling.t',
  'processing/geometry/0009_sixel_resize_palette_limit.t',
  'processing/geometry/0010_sixel_cropping_large_offsets.t',
  'processing/geometry/0011_ppm_inspection_aggressive_scaling.t',
  'processing/geometry/0012_resize_precision_mode_run.t',
  'processing/geometry/0013_resize_precision_mode_scaler_input.t',
  'processing/geometry/0014_resize_precision_mode_post_colorspace.t',
  'processing/geometry/0015_resample_nearest_downscale_80pct_lsqa.t',
  'processing/geometry/0016_resample_gaussian_downscale_80pct_lsqa.t',
  'processing/geometry/0017_resample_hanning_downscale_80pct_lsqa.t',
  'processing/geometry/0018_resample_hamming_downscale_80pct_lsqa.t',
  'processing/geometry/0019_resample_bilinear_downscale_80pct_lsqa.t',
  'processing/geometry/0020_resample_welsh_downscale_80pct_lsqa.t',
  'processing/geometry/0021_resample_bicubic_downscale_80pct_lsqa.t',
  'processing/geometry/0022_resample_lanczos2_downscale_80pct_lsqa.t',
  'processing/geometry/0023_resample_lanczos3_downscale_80pct_lsqa.t',
  'processing/geometry/0024_resample_lanczos4_downscale_80pct_lsqa.t',
  'processing/geometry/0025_resample_nearest_upscale_120pct_lsqa.t',
  'processing/geometry/0026_resample_gaussian_upscale_120pct_lsqa.t',
  'processing/geometry/0027_resample_hanning_upscale_120pct_lsqa.t',
  'processing/geometry/0028_resample_hamming_upscale_120pct_lsqa.t',
  'processing/geometry/0029_resample_bilinear_upscale_120pct_lsqa.t',
  'processing/geometry/0030_resample_welsh_upscale_120pct_lsqa.t',
  'processing/geometry/0031_resample_bicubic_upscale_120pct_lsqa.t',
  'processing/geometry/0032_resample_lanczos2_upscale_120pct_lsqa.t',
  'processing/geometry/0033_resample_lanczos3_upscale_120pct_lsqa.t',
  'processing/geometry/0034_resample_lanczos4_upscale_120pct_lsqa.t',
  'processing/geometry/0035_resize_height_32_lsqa.t',
  'processing/geometry/0036_resize_height_32px_lsqa.t',
  'processing/geometry/0037_resize_height_80pct_lsqa.t',
  'processing/geometry/0038_resize_height_100_lsqa.t',
  'processing/geometry/0039_resize_height_100px_lsqa.t',
  'processing/geometry/0040_resize_height_120pct_lsqa.t',
  'io/loader/0001_progressive_jpeg.t',
  'io/loader/0021_pngsuite_basic_default_basn0g01_msssim.t',
  'io/loader/0022_pngsuite_basic_default_basn0g02_msssim.t',
  'io/loader/0023_pngsuite_basic_default_basn0g04_msssim.t',
  'io/loader/0024_pngsuite_basic_default_basn0g08_msssim.t',
  'io/loader/0025_pngsuite_basic_default_basn0g16_msssim.t',
  'io/loader/0026_pngsuite_basic_default_basn3p01_msssim.t',
  'io/loader/0027_pngsuite_basic_default_basn3p02_msssim.t',
  'io/loader/0028_pngsuite_basic_default_basn3p04_msssim.t',
  'io/loader/0029_pngsuite_basic_default_basn3p08_msssim.t',
  'io/loader/0030_pngsuite_basic_default_basn4a08_msssim.t',
  'io/loader/0031_pngsuite_basic_default_basn4a16_msssim.t',
  'io/loader/0032_pngsuite_basic_default_basn6a08_msssim.t',
  'io/loader/0033_pngsuite_basic_default_basn6a16_msssim.t',
  'io/loader/0034_pngsuite_basic_width32_basn0g01_msssim.t',
  'io/loader/0035_pngsuite_basic_width32_basn0g02_msssim.t',
  'io/loader/0036_pngsuite_basic_width32_basn0g04_msssim.t',
  'io/loader/0037_pngsuite_basic_width32_basn0g08_msssim.t',
  'io/loader/0038_pngsuite_basic_width32_basn0g16_msssim.t',
  'io/loader/0039_pngsuite_basic_width32_basn3p01_msssim.t',
  'io/loader/0040_pngsuite_basic_width32_basn3p02_msssim.t',
  'io/loader/0041_pngsuite_basic_width32_basn3p04_msssim.t',
  'io/loader/0042_pngsuite_basic_width32_basn3p08_msssim.t',
  'io/loader/0043_pngsuite_basic_width32_basn4a08_msssim.t',
  'io/loader/0044_pngsuite_basic_width32_basn4a16_msssim.t',
  'io/loader/0045_pngsuite_basic_width32_basn6a08_msssim.t',
  'io/loader/0046_pngsuite_basic_width32_basn6a16_msssim.t',
  'io/loader/0047_pngsuite_basic_cropped_basn0g01_msssim.t',
  'io/loader/0048_pngsuite_basic_cropped_basn0g02_msssim.t',
  'io/loader/0049_pngsuite_basic_cropped_basn0g04_msssim.t',
  'io/loader/0050_pngsuite_basic_cropped_basn0g08_msssim.t',
  'io/loader/0051_pngsuite_basic_cropped_basn0g16_msssim.t',
  'io/loader/0052_pngsuite_basic_cropped_basn3p01_msssim.t',
  'io/loader/0053_pngsuite_basic_cropped_basn3p02_msssim.t',
  'io/loader/0054_pngsuite_basic_cropped_basn3p04_msssim.t',
  'io/loader/0055_pngsuite_basic_cropped_basn3p08_msssim.t',
  'io/loader/0056_pngsuite_basic_cropped_basn4a08_msssim.t',
  'io/loader/0057_pngsuite_basic_cropped_basn4a16_msssim.t',
  'io/loader/0058_pngsuite_basic_cropped_basn6a08_msssim.t',
  'io/loader/0059_pngsuite_basic_cropped_basn6a16_msssim.t',
  'io/loader/0060_pngsuite_background_default_bgai4a08_msssim.t',
  'io/loader/0061_pngsuite_background_default_bgai4a16_msssim.t',
  'io/loader/0062_pngsuite_background_default_bgan6a08_msssim.t',
  'io/loader/0063_pngsuite_background_default_bgan6a16_msssim.t',
  'io/loader/0064_pngsuite_background_default_bgbn4a08_msssim.t',
  'io/loader/0065_pngsuite_background_default_bggn4a16_msssim.t',
  'io/loader/0066_pngsuite_background_default_bgwn6a08_msssim.t',
  'io/loader/0067_pngsuite_background_default_bgyn6a16_msssim.t',
  'io/loader/0068_pngsuite_background_white_bgai4a08_msssim.t',
  'io/loader/0069_pngsuite_background_white_bgai4a16_msssim.t',
  'io/loader/0070_pngsuite_background_white_bgan6a08_msssim.t',
  'io/loader/0071_pngsuite_background_white_bgan6a16_msssim.t',
  'io/loader/0072_pngsuite_background_white_bgbn4a08_msssim.t',
  'io/loader/0073_pngsuite_background_white_bggn4a16_msssim.t',
  'io/loader/0074_pngsuite_background_white_bgwn6a08_msssim.t',
  'io/loader/0075_pngsuite_background_white_bgyn6a16_msssim.t',
  'io/loader/0076_pngsuite_background_width32_white_bgai4a08_msssim.t',
  'io/loader/0077_pngsuite_background_width32_white_bgai4a16_msssim.t',
  'io/loader/0078_pngsuite_background_width32_white_bgan6a08_msssim.t',
  'io/loader/0079_pngsuite_background_width32_white_bgan6a16_msssim.t',
  'io/loader/0080_pngsuite_background_width32_white_bgbn4a08_msssim.t',
  'io/loader/0081_pngsuite_background_width32_white_bggn4a16_msssim.t',
  'io/loader/0082_pngsuite_background_width32_white_bgwn6a08_msssim.t',
  'io/loader/0083_pngsuite_background_width32_white_bgyn6a16_msssim.t',
  'io/loader/0008_loader_coregraphics_pixelformat.t',
  'io/loader/0009_loader_wic_pixelformat.t',
  'io/loader/0010_loader_gdk_pixbuf_pixelformat.t',
  'io/loader/0011_loader_gd_pixelformat.t',
  'io/loader/0012_loader_libpng_pixelformat.t',
  'io/loader/0013_loader_libjpeg_pixelformat.t',
  'io/loader/0014_loader_builtin_pixelformat.t',
  'io/loader/0015_loader_quicklook_pixelformat.t',
  'io/loader/0016_loader_gnome_thumbnailer_pixelformat.t',
  'io/loader/0017_loader_libwebp_pixelformat.t',
  'io/loader/0018_loader_builtin_psd.t',
  'io/loader/0019_loader_builtin_hdr.t',
  'io/loader/0020_loader_builtin_pic.t',
  'io/loader/0021_loader_gdk_pixbuf2_forced_png.t',
  'io/loader/0022_loader_gdk_pixbuf2_static_gif.t',
  'io/loader/0023_loader_gdk_pixbuf2_loop_disable_gif.t',
  'io/loader/0024_loader_gdk_pixbuf2_forced_sixel_input.t',
  'io/loader/0025_loader_gdk_pixbuf2_corrupted_png.t',
  'io/loader/0026_loader_gnome_thumbnailer_forced_priority.t',
  'io/loader/0027_loader_gnome_thumbnailer_mime_wildcard.t',
  'io/loader/0028_loader_gnome_thumbnailer_exec_placeholders.t',
  'io/loader/0029_loader_gnome_thumbnailer_tryexec_skip_fallback.t',
  'io/loader/0030_loader_gnome_thumbnailer_env_priority_list.t',
  'io/loader/0031_loader_gnome_thumbnailer_cli_overrides_env.t',
  'io/loader/0032_loader_gnome_thumbnailer_hint_size_invalid_env.t',
  'io/loader/0033_loader_gnome_thumbnailer_evince_stdout_workaround.t',
  'io/loader/0034_loader_gnome_thumbnailer_unknown_placeholder_literal.t',
  'io/loader/0035_loader_gnome_thumbnailer_exec_missing_skip.t',
  'io/loader/0036_loader_gnome_thumbnailer_tryexec_path_search_order.t',
  'io/loader/0037_loader_gnome_thumbnailer_hint_size_zero_env.t',
  'io/loader/0038_loader_gnome_thumbnailer_exec_quoting_escape.t',
  'io/loader/0039_loader_gnome_thumbnailer_tryexec_absolute_path.t',
  'io/loader/0040_loader_gnome_thumbnailer_tryexec_absolute_missing_skip.t',
  'io/loader/0041_loader_gnome_thumbnailer_tryexec_empty_allowed.t',
  'io/loader/0042_loader_gnome_thumbnailer_mimetype_last_assignment_wins.t',
  'io/loader/0043_loader_gnome_thumbnailer_group_scope.t',
  'quant/mapfile/0001_mapfile_export_act.t',
  'quant/mapfile/0002_mapfile_export_pal_default.t',
  'quant/mapfile/0003_mapfile_export_pal_stdout.t',
  'quant/mapfile/0004_mapfile_export_riff.t',
  'quant/mapfile/0005_mapfile_export_gpl.t',
  'quant/mapfile/0006_mapfile_import_gpl_prefixed.t',
  'quant/mapfile/0007_mapfile_import_act_extension.t',
  'quant/mapfile/0008_mapfile_import_riff_explicit.t',
  'quant/mapfile/0009_mapfile_import_stdin.t',
  'io/network/0001_curl_invalid_file_url.t',
  'io/network/0002_curl_invalid_https_url.t',
  'io/network/0003_curl_file_scheme.t',
  'io/network/0004_curl_self_signed_blocked.t',
  'io/network/0005_curl_self_signed_with_k.t',
  'io/network/0006_invalid_https_error_message.t',
  'codec/output/0001_vt340_colour_sequences.t',
  'codec/output/0002_vt340_monochrome_sequences.t',
  'codec/output/0003_direct_sixel_encode.t',
  'codec/output/0004_sixel_metadata_inspection.t',
  'codec/output/0005_monochrome_frame_trailer.t',
  'codec/output/0006_two_pass_sixel_conversion.t',
  'quant/palette/init/0001_kmeans_init_default_unset.t',
  'quant/palette/init/0002_kmeans_init_auto.t',
  'quant/palette/init/0003_kmeans_init_unknown.t',
  'quant/palette/init/0004_kmeans_init_pca.t',
  'quant/palette/init/0005_kmeans_init_cache.t',
  'quant/palette/fallback/0006_palette_fallback_conversion.t',
  'quant/palette/fallback/0007_palette_fallback_output.t',
  'quant/palette/usage/0008_select_color_ambiguous_prefix_rejected.t',
  'quant/palette/usage/0009_select_color_unique_prefix_accepted.t',
  'quant/palette/usage/0010_explicit_dimensions_with_palette.t',
  'quant/palette/usage/0011_fast_encoder_external_palette.t',
  'quant/palette/usage/0012_palette16_conversion.t',
  'quant/palette/usage/0013_xterm_palette_override_repetition.t',
  'quant/palette/usage/0014_tiff_palette_filters.t',
  'quant/palette/usage/0015_grayscale_png_with_palette.t',
  'quant/palette/usage/0016_apply_grayscale_palette_to_colour_png.t',
  'quant/palette/usage/0017_stacked_palette_files.t',
  'quant/palette/usage/0018_xterm16_palette_preset.t',
  'quant/palette/usage/0019_jpeg_palette_welsh.t',
  'quant/palette/usage/0020_ppm_palette16_conversion.t',
  'quant/palette/usage/0021_lanczos2_palette_limit.t',
  'quant/palette/usage/0022_lanczos3_two_colour_palette.t',
  'quant/palette/usage/0023_lanczos4_palette_dump.t',
  'quant/palette/usage/0024_interlaced_palette_dump.t',
  'quant/palette/usage/0025_lookup_policy_vpte_float32.t',
  'quant/palette/usage/0026_merge_kmeans_float32_lsqa.t',
  'quant/palette/usage/0027_merge_heckbert_float32_lsqa.t',
  'quant/palette/usage/0028_merge_kmeans_8bit_lsqa.t',
  'quant/palette/usage/0029_merge_heckbert_8bit_lsqa.t',
  'quant/palette/usage/0030_snap_kmeans_float32_lsqa.t',
  'quant/palette/usage/0031_snap_heckbert_float32_lsqa.t',
  'quant/palette/usage/0032_snap_kmeans_8bit_lsqa.t',
  'quant/palette/usage/0033_snap_heckbert_8bit_lsqa.t',
  'quant/palette/usage/0034_lookup_policy_1d_eytzinger_8bit.t',
  'quant/palette/usage/0035_lookup_policy_1d_eytzinger_float32.t',
  'quant/palette/usage/0036_lookup_policy_1d_eytzinger_float32_gamma_lsqa.t',
  'quant/palette/usage/0037_lookup_policy_1d_eytzinger_float32_linear_lsqa.t',
  'quant/palette/usage/0038_lookup_policy_1d_eytzinger_float32_oklab_lsqa.t',
  'quant/palette/usage/0039_lookup_policy_1d_eytzinger_float32_cielab_lsqa.t',
  'quant/palette/usage/0040_lookup_policy_1d_eytzinger_float32_din99d_lsqa.t',
  'quant/palette/usage/0041_lookup_policy_vpte_8bit_gamma_lsqa.t',
  'quant/palette/usage/0042_lookup_policy_vpte_float32_gamma_lsqa.t',
  'quant/palette/usage/0043_lookup_policy_vpte_float32_linear_lsqa.t',
  'quant/palette/usage/0044_lookup_policy_vpte_float32_oklab_lsqa.t',
  'quant/palette/usage/0045_lookup_policy_vpte_float32_cielab_lsqa.t',
  'quant/palette/usage/0046_lookup_policy_vpte_float32_din99d_lsqa.t',
  'quant/palette/usage/0047_colorspace_rgb_cielab_lsqa.t',
  'quant/palette/usage/0048_colorspace_hls_oklab_lsqa.t',
  'quant/palette/usage/0049_colorspace_hls_vs_rgb_lsqa.t',
  'quant/palette/usage/0050_palette_map_png_rgb.t',
  'quant/palette/usage/0051_palette_map_png_pal1.t',
  'quant/palette/usage/0052_palette_map_png_pal2.t',
  'quant/palette/usage/0053_palette_map_png_pal4.t',
  'quant/palette/usage/0054_palette_map_png_pal8.t',
  'quant/palette/usage/0055_palette_map_png_g1.t',
  'quant/palette/usage/0056_palette_map_png_g2.t',
  'quant/palette/usage/0057_palette_map_png_g4.t',
  'quant/palette/usage/0058_palette_map_png_g8.t',
  'quant/palette/usage/0059_clustering_colorspace_gamma_lsqa.t',
  'quant/palette/usage/0060_clustering_colorspace_linear_lsqa.t',
  'quant/palette/usage/0061_clustering_colorspace_oklab_lsqa.t',
  'quant/palette/usage/0062_clustering_colorspace_cielab_lsqa.t',
  'quant/palette/usage/0063_clustering_colorspace_din99d_lsqa.t',
  'quant/palette/usage/0064_clustering_gamma_working_gamma_lsqa.t',
  'quant/palette/usage/0065_clustering_gamma_working_linear_lsqa.t',
  'quant/palette/usage/0066_clustering_gamma_working_oklab_lsqa.t',
  'quant/palette/usage/0067_clustering_gamma_working_cielab_lsqa.t',
  'quant/palette/usage/0068_clustering_gamma_working_din99d_lsqa.t',
  'quant/palette/usage/0069_clustering_linear_working_gamma_lsqa.t',
  'quant/palette/usage/0070_clustering_linear_working_linear_lsqa.t',
  'quant/palette/usage/0071_clustering_linear_working_oklab_lsqa.t',
  'quant/palette/usage/0072_clustering_linear_working_cielab_lsqa.t',
  'quant/palette/usage/0073_clustering_linear_working_din99d_lsqa.t',
  'quant/palette/usage/0074_clustering_oklab_working_gamma_lsqa.t',
  'quant/palette/usage/0075_clustering_oklab_working_linear_lsqa.t',
  'quant/palette/usage/0076_clustering_oklab_working_oklab_lsqa.t',
  'quant/palette/usage/0077_clustering_oklab_working_cielab_lsqa.t',
  'quant/palette/usage/0078_clustering_oklab_working_din99d_lsqa.t',
  'quant/palette/usage/0079_clustering_cielab_working_gamma_lsqa.t',
  'quant/palette/usage/0080_clustering_cielab_working_linear_lsqa.t',
  'quant/palette/usage/0081_clustering_cielab_working_oklab_lsqa.t',
  'quant/palette/usage/0082_clustering_cielab_working_cielab_lsqa.t',
  'quant/palette/usage/0083_clustering_cielab_working_din99d_lsqa.t',
  'quant/palette/usage/0084_clustering_din99d_working_gamma_lsqa.t',
  'quant/palette/usage/0085_clustering_din99d_working_linear_lsqa.t',
  'quant/palette/usage/0086_clustering_din99d_working_oklab_lsqa.t',
  'quant/palette/usage/0087_clustering_din99d_working_cielab_lsqa.t',
  'quant/palette/usage/0088_clustering_din99d_working_din99d_lsqa.t',
  'quant/palette/usage/0089_lookup_policy_vptree_8bit.t',
  'quant/palette/usage/0090_lookup_policy_vptree_float32.t',
  'quant/palette/usage/0091_lookup_policy_certlut_timeline_env_path.t',
  'quant/palette/usage/0092_lookup_policy_certlut_timeline_lines_zero.t',
  'quant/palette/usage/0093_lookup_policy_certlut_timeline_lines_stride.t',
  'quant/palette/usage/0094_lookup_policy_certlut_timeline_open_failure_nonfatal.t',
  'quant/palette/usage/0095_lookup_policy_certlut_timeline_lines_invalid_token.t',
  'quant/palette/usage/0096_lookup_policy_certlut_timeline_lines_negative.t',
  'quant/palette/usage/0097_lookup_policy_certlut_timeline_path_overwrite.t',
  'quant/palette/usage/0098_lookup_policy_certlut_timeline_empty_path.t',
  'planner/pipeline/0001_pipeline_planner_baseline_run.t',
  'planner/pipeline/0002_pipeline_planner_baseline_summary.t',
  'planner/pipeline/0003_pipeline_planner_baseline_threads.t',
  'planner/pipeline/0004_pipeline_planner_overrides_run.t',
  'planner/pipeline/0005_pipeline_planner_overrides_summary.t',
  'planner/pipeline/0006_pipeline_planner_overrides_threads.t',
  'planner/pipeline/0007_pipeline_row_ready_band_index_nonnegative.t',
  'planner/pipeline/0008_pipeline_row_ready_band_index_spans_multiple_bands.t',
  'security/issue/0001_issue167_empty_bg.t',
  'security/issue/0002_issue167_height.t',
  'security/issue/0003_issue166_width.t',
  'security/issue/0004_issue200_heap_overflow.t',
  'security/issue/0005_issue131_img2sixel_gif.t',
  'security/issue/0006_issue166_height.t',
  'security/issue/0007_issue167_height_option_first.t',
  'security/issue/0008_issue200_heap_overflow_reject.t',
  'security/issue/0009_issue207_resize_poc.t',
  'security/issue/0010_issue51_stbi_1561_poc.t',
  'security/issue/0011_issue73_stbi_1561_poc.t',
  'diagnostics/probe/0001_probe_parse.t',
  'bindings/python/0001_python_bindings.t',
  'bindings/python/0002_python_api_formats.t',
  'bindings/python/0003_python_api_options.t',
  'bindings/python/0004_python_api_errors.t',
  'bindings/python/0005_python_resource_management.t',
  'quality_gate/ms_ssim/0005_lsqa_format_grayscale.t',
  'quality_gate/ms_ssim/0006_lsqa_format_palette.t',
  'quality_gate/ms_ssim/0007_lsqa_format_rgb.t',
  'quality_gate/ms_ssim/0015_lsqa_repeat_palette_variance.t',
  'quality_gate/ms_ssim/0016_lsqa_corrupted_metadata_noise.t',
  'quality_gate/ms_ssim/0017_lsqa_corrupted_truncated_png.t',
  'quality_gate/ms_ssim/0018_lsqa_format_tga_type1_pal4.t',
  'quality_gate/ms_ssim/0019_lsqa_format_tga_type1_pal8.t',
  'quality_gate/ms_ssim/0020_lsqa_format_tga_type2_rgb.t',
  'quality_gate/ms_ssim/0021_lsqa_format_tga_type3_gray.t',
  'quality_gate/ms_ssim/0022_lsqa_format_tga_type9_pal4.t',
  'quality_gate/ms_ssim/0023_lsqa_format_tga_type9_pal8.t',
  'quality_gate/ms_ssim/0024_lsqa_format_tga_type10_rgb.t',
  'quality_gate/ms_ssim/0025_lsqa_format_tga_type11_gray.t',
  'quality_gate/ms_ssim/0026_lsqa_roundtrip_rgba_small.t',
  'quality_gate/ms_ssim/0027_lsqa_format_tiff_rgb_small.t',
  'quality_gate/ms_ssim/0028_lsqa_format_tiff_palette_small.t',
  'quality_gate/ms_ssim/0029_lsqa_format_tiff_grayscale.t',
  'quality_gate/ms_ssim/0030_lsqa_format_tiff_rgba.t',
  'quality_gate/ms_ssim/0031_lsqa_format_tiff_lzw.t',
  'quality_gate/ms_ssim/0032_lsqa_format_tiff_tiled.t',
  'quality_gate/ms_ssim/0033_lsqa_metric_highfreq_delta_numeric.t',
  'quality_gate/ms_ssim/0034_lsqa_metric_stripe_rel_numeric.t',
  'quality_gate/ms_ssim/0035_lsqa_metric_banding_rel_numeric.t',
  'quality_gate/ms_ssim/0036_lsqa_metric_clip_l_rel_numeric.t',
  'quality_gate/ms_ssim/0037_lsqa_metric_delta_e00_numeric.t',
  'quality_gate/ms_ssim/0038_lsqa_metric_delta_chroma_numeric.t',
  'quality_gate/ms_ssim/0039_lsqa_metric_gmsd_numeric.t',
  'quality_gate/ms_ssim/0040_lsqa_metric_psnr_y_numeric.t',
  'quality_gate/ms_ssim/0041_lsqa_baseline_success_dynamic.t',
  'quality_gate/ms_ssim/0042_lsqa_baseline_failure_exit5.t',
  'quality_gate/ms_ssim/0043_lsqa_baseline_metric_mismatch_error.t',
  'quality_gate/ms_ssim/0044_lsqa_baseline_missing_colon_error.t',
  'quality_gate/ms_ssim/0045_lsqa_baseline_non_numeric_error.t',
  'quality_gate/ms_ssim/0046_lsqa_metric_alias_equivalence.t',
  'quality_gate/ms_ssim/0047_lsqa_duplicate_metric_option_error.t',
  'quality_gate/ms_ssim/0048_lsqa_duplicate_baseline_option_error.t',
  'quality_gate/ms_ssim/0049_lsqa_baseline_empty_metric_name_error.t',
  'quality_gate/ms_ssim/0050_lsqa_baseline_empty_value_error.t',
  'quality_gate/ms_ssim/0051_lsqa_baseline_trailing_garbage_error.t',
  'quality_gate/ms_ssim/0052_lsqa_baseline_unknown_metric_error.t',
  'quality_gate/ms_ssim/0053_lsqa_metric_unknown_error.t',
  'quality_gate/ms_ssim/0054_lsqa_help_H_option_output.t',
  'quality_gate/ms_ssim/0055_lsqa_invalid_argument_count_error.t',
  'cli/sixel2png/0001_version.t',
  'cli/sixel2png/0002_help.t',
  'cli/sixel2png/0002_missing_args.t',
  'cli/sixel2png/0003_unknown_option.t',
  'cli/sixel2png/0004_invalid_decoder_value.t',
  'cli/sixel2png/0005_stdin_default_output.t',
]

# Match autotools TAP timeout to keep CI behavior consistent (seconds per file).
# Ensure Meson builds the TAP helper binary before running shell-based tests.
foreach t : tests
  parts = t.split('/')
  test_base = parts[parts.length() - 1]
  test_category = ''
  if parts.length() > 1
    foreach idx : range(0, parts.length() - 1)
      if test_category == ''
        test_category = parts[idx]
      else
        test_category = test_category + '/' + parts[idx]
      endif
    endforeach
  endif
  if test_category == ''
    test_category = '.'
  endif
  test_name = test_base.replace('.t', '')
  test_dir = meson.current_source_dir()
  if test_category != '.'
    test_dir = test_dir / test_category
  endif
  test_artifact_dir = artifact_root / test_category / test_base
  test_env = environment()
  foreach setting : base_env_settings
    test_env.set(setting[0], setting[1])
  endforeach
  if is_windows
    foreach dll_path : dll_paths
      test_env.prepend('PATH', dll_path)
    endforeach
  endif
  test_env.set('test_name', test_name)
  test_env.set('test_dir', test_dir)
  test_env.set('category_name', test_category)
  test_env.set('ARTIFACT_LOCAL_DIR', test_artifact_dir)
  test_env.set('SIXEL_TEST_PYTHON_VENV', meson.project_build_root() /
    'tests/.python-test-venv')
  test(t, sh,
    args: ['-c', '''
mkdir -p "$ARTIFACT_LOCAL_DIR"

if [ "x${ENABLE_PYTHON:-0}" = "x1" ]; then
  shared_python_venv="${SIXEL_TEST_PYTHON_VENV}"
  python_bin=$(command -v python3 || command -v python || true)
  wheel_path=$(find "${MESON_BUILD_ROOT}/python/dist" -maxdepth 1 \
    -type f -name 'libsixel_wheel-*.whl' 2>/dev/null | head -n 1)
  if [ -n "$python_bin" ] && [ -n "$wheel_path" ]; then
    if "$python_bin" -c 'import importlib.util,sys; missing=[m for m in ("venv","ensurepip") if importlib.util.find_spec(m) is None]; sys.exit(0 if not missing else 1)' >/dev/null 2>&1; then
      if [ ! -x "$shared_python_venv/bin/python" ]; then
        rm -rf "$shared_python_venv"
        "$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
      fi
      if [ -x "$shared_python_venv/bin/python" ]; then
        if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
          "$shared_python_venv/bin/python" -m pip install --no-deps "$wheel_path" >/dev/null 2>&1 || true
        fi
        if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
          export SIXEL_TEST_PYTHON_VENV=
        fi
      fi
    fi
  fi
fi

exec "$@"
''', 'sh', meson.current_source_dir() / t],
    depends: test_deps,
    protocol: 'tap',
    env: test_env,
    timeout: 90,
    suite: ['tap'])
endforeach

add_test_setup('ci', timeout_multiplier: 1)
