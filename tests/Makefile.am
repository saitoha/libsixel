if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif
SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh
AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c
if WANT_AMALGAMATED_TOOLS
BUILT_SOURCES = $(AMALGAMATION_SIXEL)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/converters completion_embed.h
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation sixel.c
endif
TESTS = @SIXEL_CHECK_TESTS@

SERIAL_TESTS = \
    cli/aborttrace/0001_img2sixel_aborttrace.t

COPY_TESTS = $(TESTS)
check_PROGRAMS = test_runner

TEST_EXTENSIONS = .t

HELPER_FILES = \
    _lib/sh/common.sh \
    docs/consistency/list_envvars.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;
# Keep executable suffix in sync with the configured target so TAP
# wrappers can resolve cross-built Windows binaries under Wine.
AM_TESTS_ENVIRONMENT += SIXEL_BIN_EXT='@EXEEXT@'; export SIXEL_BIN_EXT;
AM_TESTS_ENVIRONMENT += SIXEL_TSAN_BUILD='@SIXEL_TSAN_BUILD@'; \
       export SIXEL_TSAN_BUILD;
AM_TESTS_ENVIRONMENT += SIXEL_MSAN_BUILD='@SIXEL_MSAN_BUILD@'; \
       export SIXEL_MSAN_BUILD;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
AM_TESTS_ENVIRONMENT += ENABLE_PYTHON='@ENABLE_PYTHON@'; export ENABLE_PYTHON;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_VAR='@SIXEL_LIBTOOL_SHLIBPATH_VAR@'; \
       export RUNTIME_SHLIBPATH_VAR;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_SEP='@SIXEL_LIBTOOL_PATH_SEPARATOR@'; \
       export RUNTIME_SHLIBPATH_SEP;
AM_TESTS_ENVIRONMENT += SIXEL_SHLIBPATH_OVERRIDES_RUNPATH='@SIXEL_SHLIBPATH_OVERRIDES_RUNPATH@'; \
       export SIXEL_SHLIBPATH_OVERRIDES_RUNPATH;
AM_TESTS_ENVIRONMENT += SIXEL_TEST_PYTHON_VENV='$(SIXEL_TEST_PYTHON_VENV)'; \
       export SIXEL_TEST_PYTHON_VENV;
TESTS_COMMON_CPPFLAGS_VAL = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(LIBCURL_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBTIFF_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
        $(LIBWEBP_CFLAGS) \
        $(GDK_PIXBUF_CFLAGS) \
        $(GD_CFLAGS) \
        $(WIC_CFLAGS) \
        $(COREGRAPHICS_CFLAGS) \
        $(QUICKLOOK_CFLAGS) \
        $(CLIPBOARD_CFLAGS) \
        @THREAD_CFLAGS@ \
        $(MAYBE_COVERAGE)
else
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(MAYBE_COVERAGE)
endif
TESTS_COMMON_LDFLAGS_VAL = $(AM_LDFLAGS) $(MAYBE_COVERAGE)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_LDADD_VAL = -lm \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        $(ABORTTRACE_LIBS) \
        @THREAD_LIBS@
else
TESTS_COMMON_LDADD_VAL = $(top_builddir)/src/libsixel.la $(LIBS)
endif

TESTS_AMALGAMATION_CPPFLAGS_COMMON = $(TESTS_COMMON_CPPFLAGS_VAL)
TESTS_AMALGAMATION_CFLAGS_COMMON = $(TESTS_COMMON_CFLAGS_VAL)
TESTS_AMALGAMATION_LDFLAGS_COMMON = $(TESTS_COMMON_LDFLAGS_VAL)
if AMALGAMATION_NEEDS_OBJC
TESTS_AMALGAMATION_CPPFLAGS_COMMON += -x objective-c
endif
if WANT_AMALGAMATED_TOOLS
TESTS_AMALGAMATION_LDADD_COMMON = -lm \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        $(ABORTTRACE_LIBS) \
        @THREAD_LIBS@
else
TESTS_AMALGAMATION_LDADD_COMMON = $(top_builddir)/src/libsixel.la $(LIBS)
endif

if WANT_AMALGAMATED_TOOLS
nodist_test_runner_SOURCES = $(AMALGAMATION_SIXEL)
TEST_RUNNER_AMALGAMATION_DEFINES = \
    -DBUILD_TEST_RUNNER \
    -DBUILD_SIXEL_PIXBUF_LOADER \
    -DBUILD_TEST_0001_FILTER_CLIP \
    -DBUILD_TEST_0001_GDK_PIXBUF_LOADER \
    -DBUILD_TEST_0001_IMG2SIXEL_ABORTTRACE \
    -DBUILD_TEST_0001_KMEANS_INIT \
    -DBUILD_TEST_0001_PROBE_PARSE \
    -DBUILD_TEST_0002_FILTER_SAMPLE \
    -DBUILD_TEST_0002_INCREMENTAL_LOAD \
    -DBUILD_TEST_0003_CORRUPT_DATA \
    -DBUILD_TEST_0003_FILTER_RESIZE \
    -DBUILD_TEST_0004_FILTER_COLORS \
    -DBUILD_TEST_0004_PROPAGATE_ERROR \
    -DBUILD_TEST_0005_CONTEXT_FREE \
    -DBUILD_TEST_0005_FILTER_LOOKUP \
    -DBUILD_TEST_0006_FILTER_FINAL_MERGE \
    -DBUILD_TEST_0006_LOADER_MODULE \
    -DBUILD_TEST_0007_FILTER_FHEDT \
    -DBUILD_TEST_0007_LOADER_PLUGIN_INTEGRATION \
    -DBUILD_TEST_0008_FILTER_LOAD \
    -DBUILD_TEST_0008_LOADER_COREGRAPHICS_PIXELFORMAT \
    -DBUILD_TEST_0009_FILTER_DITHER \
    -DBUILD_TEST_0009_LOADER_WIC_PIXELFORMAT \
    -DBUILD_TEST_0009_PIPELINE_ROW_NOTIFY_INVALID_INPUTS \
    -DBUILD_TEST_0010_FILTER_ENCODE \
    -DBUILD_TEST_0010_LOADER_GDK_PIXBUF_PIXELFORMAT \
    -DBUILD_TEST_0011_LOADER_GD_PIXELFORMAT \
    -DBUILD_TEST_0012_LOADER_LIBPNG_PIXELFORMAT \
    -DBUILD_TEST_0013_LOADER_LIBJPEG_PIXELFORMAT \
    -DBUILD_TEST_0014_LOADER_BUILTIN_PIXELFORMAT \
    -DBUILD_TEST_0015_LOADER_QUICKLOOK_PIXELFORMAT \
    -DBUILD_TEST_0016_LOADER_GNOME_THUMBNAILER_PIXELFORMAT \
    -DBUILD_TEST_0017_LOADER_LIBWEBP_PIXELFORMAT \
    -DBUILD_TEST_0029_CLI_TOKEN_IS_KNOWN_OPTION \
    -DBUILD_TEST_0030_CLI_OPTION_REQUIRES_ARGUMENT \
    -DBUILD_TEST_0031_CLI_GUARD_MISSING_ARGUMENT \
    -DBUILD_ABORTTRACE \
    -DBUILD_CLI
test_runner_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    $(TEST_RUNNER_AMALGAMATION_DEFINES) \
    $(GDK_PIXBUF_CFLAGS) \
    -DGDK_PIXBUF_ENABLE_BACKEND \
    -DSIXEL_PIXBUF_LOADER_TESTING \
    -DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS
test_runner_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
test_runner_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
test_runner_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
test_runner_SOURCES = test_runner.c \
    cli/aborttrace/0001_img2sixel_aborttrace.c \
    cli/0029_cli_token_is_known_option.c \
    cli/0030_cli_option_requires_argument.c \
    cli/0031_cli_guard_missing_argument.c \
    processing/filter/0001_filter_clip.c \
    processing/filter/0002_filter_sample.c \
    processing/filter/0003_filter_resize.c \
    processing/filter/0004_filter_colors.c \
    processing/filter/0005_filter_lookup.c \
    processing/filter/0006_filter_final_merge.c \
    processing/filter/0007_filter_fhedt.c \
    processing/filter/0008_filter_load.c \
    processing/filter/0009_filter_dither.c \
    processing/filter/0010_filter_encode.c \
    processing/filter/filter_test_common.h \
    io/loader/0008_loader_coregraphics_pixelformat.c \
    io/loader/0009_loader_wic_pixelformat.c \
    io/loader/0010_loader_gdk_pixbuf_pixelformat.c \
    io/loader/0011_loader_gd_pixelformat.c \
    io/loader/0012_loader_libpng_pixelformat.c \
    io/loader/0013_loader_libjpeg_pixelformat.c \
    io/loader/0014_loader_builtin_pixelformat.c \
    io/loader/0015_loader_quicklook_pixelformat.c \
    io/loader/0016_loader_gnome_thumbnailer_pixelformat.c \
    io/loader/0017_loader_libwebp_pixelformat.c \
    io/loader/pixelformat_test_common.h \
    quant/palette/0001_kmeans_init.c \
    diagnostics/probe/0001_probe_parse.c \
    extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c \
    extension/gdk-pixbuf-loader/0002_incremental_load.c \
    extension/gdk-pixbuf-loader/0003_corrupt_data.c \
    extension/gdk-pixbuf-loader/0004_propagate_error.c \
    extension/gdk-pixbuf-loader/0005_context_free.c \
    extension/gdk-pixbuf-loader/0006_loader_module.c \
    extension/gdk-pixbuf-loader/0007_loader_plugin_integration.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c \
    $(top_srcdir)/converters/aborttrace.c \
    $(top_srcdir)/converters/cli.c
test_runner_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL) \
    $(GDK_PIXBUF_CFLAGS) \
    -DGDK_PIXBUF_ENABLE_BACKEND \
    -DSIXEL_PIXBUF_LOADER_TESTING \
    -DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS
test_runner_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
test_runner_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
test_runner_LDADD = $(TESTS_COMMON_LDADD_VAL) \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        $(ABORTTRACE_LIBS) \
        @THREAD_LIBS@
endif

LOG_DRIVER = $(T_LOG_DRIVER)
LOG_DRIVER_FLAGS = $(AM_TEST_LOG_DRIVER_FLAGS)
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_builddir)/build-aux/lso-tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments

# Default to soft errors so the harness continues after individual failures.
DISABLE_HARD_ERRORS = yes
AM_TEST_TIMEOUT = 90
AM_LOG_DRIVER_FLAGS = --timeout=$(AM_TEST_TIMEOUT)
# Leave AM_LOG_FLAGS empty so Automake does not append timeout parameters to
# LOG_COMPILE; the drivers consume timeout hints themselves.
AM_LOG_FLAGS =

.PHONY: staticcheck

staticcheck:
	@$(SHELL) "$(top_srcdir)/tests/_static/sh/shellcheck.sh" "$(abs_top_srcdir)"
	@cd "$(abs_top_srcdir)" && \
	find src tests -type f \( -name '*.[ch]' -o -name '*.md' -o \
	-name '*.1' -o -name '*.in' -o -name '*.am' -o -name '*.build' \
	-o -name '*.t' -o -name 'LICENSE' -o -name '*.py' -o \
	-name '*.rb' -o -name '*.thumbnailer' -o -name '*.sh' \) \
	-exec codespell {} +

# Override check-TESTS to avoid nested single quotes in automake helpers.
# Export config.h feature toggles once so tests can rely on environment
# variables instead of repeatedly reading the header.
check-TESTS: $(check_PROGRAMS)
	@$(am__rm_f) $(RECHECK_LOGS)
	@$(am__rm_f) $(RECHECK_LOGS:.log=.trs)
	@$(am__rm_f) $(TEST_SUITE_LOG)
	@set +e; \
	config_header="$(top_builddir)/config.h"; \
	resolved_test_paths=`$(SHELL) "$(top_builddir)/build-aux/resolve-test-tool-paths.sh" \
	    "$(abs_top_builddir)" "$(srcdir)/check-test-exports.list"`; \
	set -a; \
	eval "$$resolved_test_paths"; \
	config_exports=`$(top_srcdir)/build-aux/resolve-config-h-exports.sh "$$config_header" exports`; \
	eval "$$config_exports"; \
	set +a; \
	python_venv_exports=`$(SHELL) "$(top_srcdir)/build-aux/resolve-python-test-venv.sh" \
	    "@ENABLE_PYTHON@" "$(PYTHON)" "$(abs_top_builddir)/python/dist" \
	    "$(abs_top_builddir)/tests/.python-test-venv"`; \
	eval "$$python_venv_exports"; \
	export SIXEL_TEST_PYTHON_VENV; \
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner@EXEEXT@ --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	log_list=""; \
	for i in $(TEST_LOGS); do \
	    log_list="$$log_list $$i"; \
	done; \
	log_list=`echo $$log_list`; \
	$(MAKE) $(AM_MAKEFLAGS) $(TEST_SUITE_LOG) \
	    TEST_LOGS="$$log_list" \
	    SIXEL_TEST_PYTHON_VENV="$$SIXEL_TEST_PYTHON_VENV"; \
	exit $$?;

EXTRA_DIST = $(TESTS) $(HELPER_FILES) \
    _static/sh/shellcheck.sh \
    check-tests.list \
    check-test-exports.list \
    data/inputs/thumbnailer/bin/evince-thumbnailer \
    data/inputs/thumbnailer/bin/fake-thumb \
    data/inputs/thumbnailer/bin/fake-thumb-args \
    data/inputs/thumbnailer/bin/fake-thumb-size \
    data/inputs/thumbnailer/bin/fake-thumb-placeholders \
    data/inputs/thumbnailer/bin/gdk-pixbuf-thumbnailer \
    data/inputs/formats/grayscale.jpg \
    data/inputs/formats/grayscale.tiff \
    data/inputs/formats/palette.png \
    data/inputs/formats/rgb.png \
    data/inputs/formats/rgb-lzw.tiff \
    data/inputs/formats/rgb-tiled.tiff \
    data/inputs/formats/snake-64-reference-rgb-flip.png \
    data/inputs/formats/snake-64-reference-rgb.png \
    data/inputs/formats/snake-64-reference-gray-flip.png \
    data/inputs/formats/snake-64-reference-gray.png \
    data/inputs/formats/snake-64-reference-rgba.png \
    data/inputs/formats/snake-64-grayscale.jpg \
    data/inputs/formats/snake-32.png \
    data/inputs/formats/rgba.png \
    data/inputs/formats/rgba.tiff \
    data/inputs/formats/snake-bmp2-pal8.bmp \
    data/inputs/formats/snake-bmp3-rgb.bmp \
    data/inputs/formats/snake-bmp3-rle8.bmp \
    data/inputs/formats/snake-dds-dxt1.dds \
    data/inputs/formats/snake-dds-dxt3.dds \
    data/inputs/formats/snake-dds-dxt5.dds \
    data/inputs/formats/snake-gif-interlaced.gif \
    data/inputs/formats/snake-ico-multisize.ico \
    data/inputs/formats/snake-ico-pal8.ico \
    data/inputs/formats/snake-ico-rgba.ico \
    data/inputs/formats/snake-jpeg-444.jpg \
    data/inputs/formats/snake-jpeg-progressive-420.jpg \
    data/inputs/formats/snake-png-adam7-rgb.png \
    data/inputs/formats/snake-png-pal8.png \
    data/inputs/formats/snake-png-gray16.png \
    data/inputs/formats/snake-png-pal8-trns.png \
    data/inputs/formats/snake-tiff-rle-rgb.tiff \
    data/inputs/formats/snake-tiff-zip-rgb.tiff \
    data/inputs/formats/snake-tga-type1-pal4.tga \
    data/inputs/formats/snake-tga-type1-pal8.tga \
    data/inputs/formats/snake-tga-type2-rgb.tga \
    data/inputs/formats/snake-tga-type3-gray.tga \
    data/inputs/formats/snake-tga-type9-pal4.tga \
    data/inputs/formats/snake-tga-type9-pal8.tga \
    data/inputs/formats/snake-tga-type10-rgb.tga \
    data/inputs/formats/snake-tga-type11-gray.tga \
    data/inputs/formats/stbi_minimal.hdr \
    data/inputs/formats/stbi_minimal.psd \
    data/inputs/formats/stbi_minimal.pic \
    data/inputs/small.ppm \
    data/inputs/tall.ppm \
    data/inputs/small.gif \
    data/inputs/snake_64.bmp \
    data/inputs/snake_64.gif \
    data/inputs/snake_64.jpg \
    data/inputs/snake_64.pbm \
    data/inputs/snake_64.pgm \
    data/inputs/snake_64.png \
    data/inputs/snake_64.ppm \
    data/inputs/snake_64.six \
    data/inputs/snake_64-oversized.six \
    data/inputs/snake_64.tga \
    data/inputs/snake_64.tiff \
    data/inputs/snake_64-indexed.tiff \
    data/inputs/snake_64.webp \
    data/inputs/snake_64_jpg_resize_63x.png \
    data/inputs/scaling/snake_64_80pct.png \
    data/inputs/scaling/snake_64_120pct.png \
    data/inputs/scaling/snake_64_h32.png \
    data/inputs/scaling/snake_64_h32px.png \
    data/inputs/scaling/snake_64_h80pct.png \
    data/inputs/scaling/snake_64_h100.png \
    data/inputs/scaling/snake_64_h100px.png \
    data/inputs/scaling/snake_64_h120pct.png \
    data/inputs/scaling/snake_64_nearest_80pct.png \
    data/inputs/scaling/snake_64_nearest_120pct.png \
    data/inputs/scaling/snake_64_gaussian_80pct.png \
    data/inputs/scaling/snake_64_gaussian_120pct.png \
    data/inputs/scaling/snake_64_hanning_80pct.png \
    data/inputs/scaling/snake_64_hanning_120pct.png \
    data/inputs/scaling/snake_64_hamming_80pct.png \
    data/inputs/scaling/snake_64_hamming_120pct.png \
    data/inputs/scaling/snake_64_bilinear_80pct.png \
    data/inputs/scaling/snake_64_bilinear_120pct.png \
    data/inputs/scaling/snake_64_welsh_80pct.png \
    data/inputs/scaling/snake_64_welsh_120pct.png \
    data/inputs/scaling/snake_64_bicubic_80pct.png \
    data/inputs/scaling/snake_64_bicubic_120pct.png \
    data/inputs/scaling/snake_64_lanczos2_80pct.png \
    data/inputs/scaling/snake_64_lanczos2_120pct.png \
    data/inputs/scaling/snake_64_lanczos3_80pct.png \
    data/inputs/scaling/snake_64_lanczos3_120pct.png \
    data/inputs/scaling/snake_64_lanczos4_80pct.png \
    data/inputs/scaling/snake_64_lanczos4_120pct.png \
    data/inputs/thumbnailer/thumbnailers/absolute-tryexec.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/empty-tryexec.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/entry-missing-exec.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/entry-valid.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/env-priority.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/evince.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/exec-quoting.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/fake.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/fallback-gdk.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/group-scope.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/hint-size-zero.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/hint-size.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/mime-overwrite.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/missing-absolute.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/override-env.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/path-order.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/placeholders.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/tryexec-missing.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/unknown-placeholder.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/valid-absolute.thumbnailer \
    data/inputs/thumbnailer/thumbnailers/wildcard.thumbnailer \
    data/inputs/thumbnailer/cases/loader/bin/fake-thumb \
    data/inputs/thumbnailer/cases/loader/thumbnailers/png.thumbnailer \
    data/inputs/thumbnailer/cases/0026/thumbnailers/fake.thumbnailer \
    data/inputs/thumbnailer/cases/0027/thumbnailers/wildcard.thumbnailer \
    data/inputs/thumbnailer/cases/0028/thumbnailers/placeholders.thumbnailer \
    data/inputs/thumbnailer/cases/0029/thumbnailers/fallback-gdk.thumbnailer \
    data/inputs/thumbnailer/cases/0029/thumbnailers/fallback.thumbnailer \
    data/inputs/thumbnailer/cases/0029/thumbnailers/tryexec-missing.thumbnailer \
    data/inputs/thumbnailer/cases/0030/thumbnailers/env-priority.thumbnailer \
    data/inputs/thumbnailer/cases/0031/thumbnailers/override-env.thumbnailer \
    data/inputs/thumbnailer/cases/0032/bin/fake-thumb \
    data/inputs/thumbnailer/cases/0032/thumbnailers/hint-size.thumbnailer \
    data/inputs/thumbnailer/cases/0033/thumbnailers/evince.thumbnailer \
    data/inputs/thumbnailer/cases/0034/bin/fake-thumb \
    data/inputs/thumbnailer/cases/0034/thumbnailers/unknown-placeholder.thumbnailer \
    data/inputs/thumbnailer/cases/0035/thumbnailers/entry-missing-exec.thumbnailer \
    data/inputs/thumbnailer/cases/0035/thumbnailers/entry-valid.thumbnailer \
    data/inputs/thumbnailer/cases/0036/thumbnailers/path-order.thumbnailer \
    data/inputs/thumbnailer/cases/0037/bin/fake-thumb \
    data/inputs/thumbnailer/cases/0037/thumbnailers/hint-size-zero.thumbnailer \
    data/inputs/thumbnailer/cases/0038/thumbnailers/exec-quoting.thumbnailer \
    data/inputs/thumbnailer/cases/0039/thumbnailers/absolute-tryexec.thumbnailer \
    data/inputs/thumbnailer/cases/0040/thumbnailers/missing-absolute.thumbnailer.in \
    data/inputs/thumbnailer/cases/0040/thumbnailers/valid-absolute.thumbnailer.in \
    data/inputs/thumbnailer/cases/0041/thumbnailers/empty-tryexec.thumbnailer \
    data/inputs/thumbnailer/cases/0042/thumbnailers/mime-overwrite.thumbnailer \
    data/inputs/thumbnailer/cases/0043/thumbnailers/group-scope.thumbnailer \
    data/resolutions/hd_landscape.png \
    data/resolutions/panorama_tall.png \
    data/resolutions/panorama_wide.png \
    data/resolutions/quarter_square.png \
    data/resolutions/svga_landscape.png \
    data/resolutions/tiny_square.png \
    data/corrupted/metadata_noise.jpg \
    data/corrupted/truncated.png \
    data/security/issue/data/166/poc \
    data/security/issue/data/167/poc \
    data/security/issue/data/200/POC_img2sixel_heap_buffer_overflow \
    data/security/issue/data/207/poc \
    data/security/issue/data/libsixel-libsixel/51/stbi_1561_poc.bin \
    data/security/issue/data/libsixel-libsixel/73/stbi_1561_poc.bin \
    data/security/issue/data/131/2020-01-30-img2sixel.gif \
    data/security/issue/data/136/poc \
    data/loader/pngsuite_expected/0021_pngsuite_basic_default_basn0g01_msssim.ppm \
    data/loader/pngsuite_expected/0022_pngsuite_basic_default_basn0g02_msssim.ppm \
    data/loader/pngsuite_expected/0023_pngsuite_basic_default_basn0g04_msssim.ppm \
    data/loader/pngsuite_expected/0024_pngsuite_basic_default_basn0g08_msssim.ppm \
    data/loader/pngsuite_expected/0025_pngsuite_basic_default_basn0g16_msssim.ppm \
    data/loader/pngsuite_expected/0026_pngsuite_basic_default_basn3p01_msssim.ppm \
    data/loader/pngsuite_expected/0027_pngsuite_basic_default_basn3p02_msssim.ppm \
    data/loader/pngsuite_expected/0028_pngsuite_basic_default_basn3p04_msssim.ppm \
    data/loader/pngsuite_expected/0029_pngsuite_basic_default_basn3p08_msssim.ppm \
    data/loader/pngsuite_expected/0030_pngsuite_basic_default_basn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0031_pngsuite_basic_default_basn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0032_pngsuite_basic_default_basn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0033_pngsuite_basic_default_basn6a16_msssim.ppm \
    data/loader/pngsuite_expected/0034_pngsuite_basic_width32_basn0g01_msssim.ppm \
    data/loader/pngsuite_expected/0035_pngsuite_basic_width32_basn0g02_msssim.ppm \
    data/loader/pngsuite_expected/0036_pngsuite_basic_width32_basn0g04_msssim.ppm \
    data/loader/pngsuite_expected/0037_pngsuite_basic_width32_basn0g08_msssim.ppm \
    data/loader/pngsuite_expected/0038_pngsuite_basic_width32_basn0g16_msssim.ppm \
    data/loader/pngsuite_expected/0039_pngsuite_basic_width32_basn3p01_msssim.ppm \
    data/loader/pngsuite_expected/0040_pngsuite_basic_width32_basn3p02_msssim.ppm \
    data/loader/pngsuite_expected/0041_pngsuite_basic_width32_basn3p04_msssim.ppm \
    data/loader/pngsuite_expected/0042_pngsuite_basic_width32_basn3p08_msssim.ppm \
    data/loader/pngsuite_expected/0043_pngsuite_basic_width32_basn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0044_pngsuite_basic_width32_basn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0045_pngsuite_basic_width32_basn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0046_pngsuite_basic_width32_basn6a16_msssim.ppm \
    data/loader/pngsuite_expected/0047_pngsuite_basic_cropped_basn0g01_msssim.ppm \
    data/loader/pngsuite_expected/0048_pngsuite_basic_cropped_basn0g02_msssim.ppm \
    data/loader/pngsuite_expected/0049_pngsuite_basic_cropped_basn0g04_msssim.ppm \
    data/loader/pngsuite_expected/0050_pngsuite_basic_cropped_basn0g08_msssim.ppm \
    data/loader/pngsuite_expected/0051_pngsuite_basic_cropped_basn0g16_msssim.ppm \
    data/loader/pngsuite_expected/0052_pngsuite_basic_cropped_basn3p01_msssim.ppm \
    data/loader/pngsuite_expected/0053_pngsuite_basic_cropped_basn3p02_msssim.ppm \
    data/loader/pngsuite_expected/0054_pngsuite_basic_cropped_basn3p04_msssim.ppm \
    data/loader/pngsuite_expected/0055_pngsuite_basic_cropped_basn3p08_msssim.ppm \
    data/loader/pngsuite_expected/0056_pngsuite_basic_cropped_basn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0057_pngsuite_basic_cropped_basn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0058_pngsuite_basic_cropped_basn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0059_pngsuite_basic_cropped_basn6a16_msssim.ppm \
    data/loader/pngsuite_expected/0060_pngsuite_background_default_bgai4a08_msssim.ppm \
    data/loader/pngsuite_expected/0061_pngsuite_background_default_bgai4a16_msssim.ppm \
    data/loader/pngsuite_expected/0062_pngsuite_background_default_bgan6a08_msssim.ppm \
    data/loader/pngsuite_expected/0063_pngsuite_background_default_bgan6a16_msssim.ppm \
    data/loader/pngsuite_expected/0064_pngsuite_background_default_bgbn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0065_pngsuite_background_default_bggn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0066_pngsuite_background_default_bgwn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0067_pngsuite_background_default_bgyn6a16_msssim.ppm \
    data/loader/pngsuite_expected/0068_pngsuite_background_white_bgai4a08_msssim.ppm \
    data/loader/pngsuite_expected/0069_pngsuite_background_white_bgai4a16_msssim.ppm \
    data/loader/pngsuite_expected/0070_pngsuite_background_white_bgan6a08_msssim.ppm \
    data/loader/pngsuite_expected/0071_pngsuite_background_white_bgan6a16_msssim.ppm \
    data/loader/pngsuite_expected/0072_pngsuite_background_white_bgbn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0073_pngsuite_background_white_bggn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0074_pngsuite_background_white_bgwn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0075_pngsuite_background_white_bgyn6a16_msssim.ppm \
    data/loader/pngsuite_expected/0076_pngsuite_background_width32_white_bgai4a08_msssim.ppm \
    data/loader/pngsuite_expected/0077_pngsuite_background_width32_white_bgai4a16_msssim.ppm \
    data/loader/pngsuite_expected/0078_pngsuite_background_width32_white_bgan6a08_msssim.ppm \
    data/loader/pngsuite_expected/0079_pngsuite_background_width32_white_bgan6a16_msssim.ppm \
    data/loader/pngsuite_expected/0080_pngsuite_background_width32_white_bgbn4a08_msssim.ppm \
    data/loader/pngsuite_expected/0081_pngsuite_background_width32_white_bggn4a16_msssim.ppm \
    data/loader/pngsuite_expected/0082_pngsuite_background_width32_white_bgwn6a08_msssim.ppm \
    data/loader/pngsuite_expected/0083_pngsuite_background_width32_white_bgyn6a16_msssim.ppm \
    data/io/network/certs/server.crt \
    data/io/network/certs/server.key

# Avoid deleting the in-tree timeout wrapper when building in the source
# directory. The wrapper is regenerated for out-of-tree builds, so remove it
# only in that case to prevent accidental loss from "make clean".
CLEANFILES =

# Avoid deleting in-tree test scripts when "make clean" is invoked.
# Only remove copied test files in an out-of-tree build so that the
# distributed sources remain intact for developers and package builds.

# Keep this recipe tab-indented; GNU make requires tabs for commands.
clean-local:
	@if test "$(abs_builddir)" != "$(abs_srcdir)"; then \
	rm -f $(LOG_COMPILER) $(COPY_TESTS) $(HELPER_FILES); \
	fi
	rm -rf $(ARTIFACT_ROOT)
	rm -rf $(abs_top_builddir)/tests/.python-test-venv
	rm -f $(builddir)/*_metrics.json

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(COPY_TESTS) $(HELPER_FILES):
	$(MKDIR_P) $(builddir)/$(dir $@)
	$(AM_V_GEN){ if test -f $(srcdir)/$@; then \
	    cp $(srcdir)/$@ $(builddir)/$@; \
	fi; }


if WITH_SHEBANG_FILE
SIXEL_SHEBANG_TARGETS = $(check_PROGRAMS)

apply-shebang-check: $(SIXEL_SHEBANG_TARGETS)
	@if test -n "$(SIXEL_SHEBANG_TARGETS)"; then \
	    $(SIXEL_SHEBANG_HELPER) "$(SIXEL_SHEBANG_FILE)" \
	        $(SIXEL_SHEBANG_TARGETS); \
	fi

check-am: apply-shebang-check
endif
