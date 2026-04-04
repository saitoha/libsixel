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

TEST_EXTENSIONS = .t .py .rb .pl .php


HELPER_FILES = \
    docs/consistency/list_envvars.sh \
    bindings/python/_taptest.py \
    bindings/check-loader-setopt-sync.sh \
    bindings/ruby/_coverage_runner.rb \
    bindings/ruby/_coverage_to_lcov.rb \
    bindings/perl/_coverage_to_lcov.pl \
    bindings/php/_coverage_runner.php \
    bindings/php/_coverage_to_lcov.php

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
SIXEL_TEST_MACOS_PRODUCT_VERSION := $(shell sw_vers -productVersion 2>/dev/null || printf "")
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
AM_TESTS_ENVIRONMENT += ENABLE_RUBY='@ENABLE_RUBY@'; export ENABLE_RUBY;
AM_TESTS_ENVIRONMENT += ENABLE_PERL='@ENABLE_PERL@'; export ENABLE_PERL;
AM_TESTS_ENVIRONMENT += ENABLE_PHP='@ENABLE_PHP@'; export ENABLE_PHP;
AM_TESTS_ENVIRONMENT += SIXEL_CONFIGURED_RUBY='@RUBY@'; export SIXEL_CONFIGURED_RUBY;
AM_TESTS_ENVIRONMENT += SIXEL_CONFIGURED_PHP='@PHP@'; export SIXEL_CONFIGURED_PHP;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_VAR='@SIXEL_LIBTOOL_SHLIBPATH_VAR@'; \
       export RUNTIME_SHLIBPATH_VAR;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_SEP='@SIXEL_LIBTOOL_PATH_SEPARATOR@'; \
       export RUNTIME_SHLIBPATH_SEP;
AM_TESTS_ENVIRONMENT += SIXEL_SHLIBPATH_OVERRIDES_RUNPATH='@SIXEL_SHLIBPATH_OVERRIDES_RUNPATH@'; \
       export SIXEL_SHLIBPATH_OVERRIDES_RUNPATH;
AM_TESTS_ENVIRONMENT += SIXEL_TEST_ADDITIOANL_PATH='@SIXEL_TEST_ADDITIOANL_PATH@'; \
       export SIXEL_TEST_ADDITIOANL_PATH;
AM_TESTS_ENVIRONMENT += SIXEL_TEST_MACOS_PRODUCT_VERSION='$(SIXEL_TEST_MACOS_PRODUCT_VERSION)'; \
       export SIXEL_TEST_MACOS_PRODUCT_VERSION;
AM_TESTS_ENVIRONMENT += SIXEL_TEST_HOST_ARCH='@host_cpu@'; \
       export SIXEL_TEST_HOST_ARCH;
TESTS_COMMON_CPPFLAGS_VAL = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(LIBCURL_CFLAGS) \
        $(LIBFETCH_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBTIFF_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
        $(LIBWEBP_CFLAGS) \
        $(LIBRSVG_CFLAGS) \
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
        $(LIBFETCH_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LCMS2_LIBS) \
        $(LIBWEBP_LIBS) \
        $(LIBRSVG_LIBS) \
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
        $(LIBFETCH_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LCMS2_LIBS) \
        $(LIBWEBP_LIBS) \
        $(LIBRSVG_LIBS) \
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
# Keep fault-injection coverage enabled in amalgamation mode as well, so
# behavior stays aligned across normal and amalgamated build variants.
TEST_RUNNER_AMALGAMATION_DEFINES = \
    -DBUILD_TEST_RUNNER \
    -DBUILD_SIXEL_PIXBUF_LOADER \
    -DBUILD_TEST_0001_FILTER_CLIP \
    -DBUILD_TEST_0001_GDK_PIXBUF_LOADER \
    -DBUILD_TEST_0001_IMG2SIXEL_ABORTTRACE \
    -DBUILD_TEST_0001_KMEANS_INIT \
    -DBUILD_TEST_0002_KMEDOIDS_CONSTRAINTS \
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
    -DBUILD_TEST_0013_LOADER_LIBWEBP_PIXELFORMAT \
    -DBUILD_TEST_0014_LOADER_LIBWEBP_ANIMATION_FRAMES \
    -DBUILD_TEST_0015_LOADER_LIBWEBP_PALETTE_PROMOTION_GUARD \
    -DBUILD_TEST_0016_LOADER_LIBWEBP_FAULT_INJECTION \
    -DBUILD_TEST_0020_LOADER_LIBRSVG_DETECT_SVG_LIKE \
    -DBUILD_TEST_0021_LOADER_BUILTIN_INDEXED_PNG_REQCOLORS_FALLBACK \
    -DBUILD_TEST_0022_LOADER_LIBPNG_INDEXED_PNG_REQCOLORS_FALLBACK \
    -DBUILD_TEST_0023_LOADER_LIBRSVG_PIXELFORMAT \
    -DBUILD_TEST_0025_LOADER_LIBRSVG_DECODE_MODE \
    -DBUILD_TEST_0029_CLI_TOKEN_IS_KNOWN_OPTION \
    -DBUILD_TEST_0030_CLI_OPTION_REQUIRES_ARGUMENT \
    -DBUILD_TEST_0031_CLI_GUARD_MISSING_ARGUMENT \
    -DBUILD_TEST_0050_LOADER_OSC11_COLORSPEC_PARSER \
    -DBUILD_TEST_0051_LOADER_OSC11_RESPONSE_PARSER \
    -DBUILD_TEST_0052_LOADER_OSC11_QUERY_CONTROL \
    -DBUILD_TEST_0053_LOADER_BACKGROUND_COLORSPACE_OVERRIDE \
    -DBUILD_TEST_0054_LOADER_GIF_BGCOLOR_CANVAS_FILL \
    -DBUILD_TEST_0055_LOADER_ANIMATION_HIDE_CURSOR_CONTROL \
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
    loader/coregraphics/0008_loader_coregraphics_pixelformat.c \
    loader/wic/0009_loader_wic_pixelformat.c \
    loader/gdk-pixbuf2/0010_loader_gdk_pixbuf_pixelformat.c \
    loader/gd/0011_loader_gd_pixelformat.c \
    loader/libpng/0012_loader_libpng_pixelformat.c \
    loader/libjpeg/0013_loader_libjpeg_pixelformat.c \
    loader/builtin/0014_loader_builtin_pixelformat.c \
    loader/quicklook/0015_loader_quicklook_pixelformat.c \
    loader/gnome-thumbnailer/0016_loader_gnome_thumbnailer_pixelformat.c \
    loader/libwebp/0013_loader_libwebp_pixelformat.c \
    loader/libwebp/0014_loader_libwebp_animation_frames.c \
    loader/libwebp/0015_loader_libwebp_palette_promotion_guard.c \
    loader/libwebp/0016_loader_libwebp_fault_injection.c \
    loader/librsvg/0020_loader_librsvg_detect_svg_like.c \
    loader/builtin/0021_loader_builtin_indexed_png_reqcolors_fallback.c \
    loader/libpng/0022_loader_libpng_indexed_png_reqcolors_fallback.c \
    loader/librsvg/0023_loader_librsvg_pixelformat.c \
    loader/librsvg/0025_loader_librsvg_decode_mode.c \
    loader/0050_loader_osc11_colorspec_parser.c \
    loader/0051_loader_osc11_response_parser.c \
    loader/0052_loader_osc11_query_control.c \
    loader/0053_loader_background_colorspace_override.c \
    loader/0054_loader_gif_bgcolor_canvas_fill.c \
    loader/0055_loader_animation_hide_cursor_control.c \
    loader/pixelformat_test_common.h \
    quant/palette/0001_kmeans_init.c \
    quant/palette/0002_kmedoids_constraints.c \
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
    $(LIBWEBP_CFLAGS) \
    $(GDK_PIXBUF_CFLAGS) \
    -DGDK_PIXBUF_ENABLE_BACKEND \
    -DSIXEL_PIXBUF_LOADER_TESTING \
    -DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS
test_runner_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
test_runner_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
test_runner_LDADD = $(TESTS_COMMON_LDADD_VAL) \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(LIBFETCH_LIBS) \
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

.PHONY: \
	staticcheck \
	staticcheck-actionlint \
	staticcheck-shellcheck \
	staticcheck-codespell \
	staticcheck-python-compileall \
	staticcheck-private-includes \
	staticcheck-test-runner-dllexport \
	staticcheck-makefile-recipes \
	staticcheck-meson-palette-sources \
	staticcheck-kmedoids-suboption-env-sync \
	staticcheck-test-plan-single \
	staticcheck-test-no-grep-awk \
	staticcheck-artifact-local-dir-mkdir \
	staticcheck-binding-loader-setopt-sync \
	staticcheck-build-doc-configure-help \
	staticcheck-build-doc-meson-options \
	staticcheck-docs-help-vs-man \
	staticcheck-docs-man-vs-bash-completion \
	staticcheck-docs-envvars-help-table \
	staticcheck-c-header-modeline

# Keep Autotools staticcheck semantics aligned with Meson:
# FAIL/ERROR must propagate as a non-zero exit status.
staticcheck \
staticcheck-actionlint \
staticcheck-shellcheck \
staticcheck-codespell \
staticcheck-python-compileall \
staticcheck-private-includes \
staticcheck-test-runner-dllexport \
staticcheck-makefile-recipes \
staticcheck-meson-palette-sources \
staticcheck-kmedoids-suboption-env-sync \
staticcheck-test-plan-single \
staticcheck-test-no-grep-awk \
staticcheck-artifact-local-dir-mkdir \
staticcheck-binding-loader-setopt-sync \
staticcheck-build-doc-configure-help \
staticcheck-build-doc-meson-options \
staticcheck-docs-help-vs-man \
staticcheck-docs-man-vs-bash-completion \
staticcheck-docs-envvars-help-table \
staticcheck-c-header-modeline: \
	T_LOG_DRIVER_FLAGS = $(AM_TEST_LOG_DRIVER_FLAGS) --enable-hard-errors

staticcheck: staticcheck-clean
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	"$(SHELL)" "$(top_srcdir)/tests/_static/sh/staticcheck-suite.sh" \
		"$(abs_top_srcdir)" \
		"$(abs_top_builddir)" \
		"$(ACTIONLINT)" \
		"$(top_srcdir)/tests/_static/sh/shellcheck.sh" \
		"$(SHELLCHECK)" \
		"$(CODESPELL)" \
		"$(PYTHON)"

.PHONY: staticcheck-clean

staticcheck-clean:
	@$(am__rm_f) staticcheck-*.log staticcheck-*.trs

staticcheck-actionlint:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-actionlint" \
		--log-file staticcheck-actionlint.log \
		--trs-file staticcheck-actionlint.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-actionlint.sh" \
		"$(abs_top_srcdir)" "$(ACTIONLINT)"

staticcheck-shellcheck:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-shellcheck" \
		--log-file staticcheck-shellcheck.log \
		--trs-file staticcheck-shellcheck.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-shellcheck.sh" \
		"$(top_srcdir)/tests/_static/sh/shellcheck.sh" \
		"$(abs_top_srcdir)" "$(SHELLCHECK)"

staticcheck-codespell:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-codespell" \
		--log-file staticcheck-codespell.log \
		--trs-file staticcheck-codespell.trs \
		--timeout 600 \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-codespell.sh" \
		"$(abs_top_srcdir)" "$(CODESPELL)"

staticcheck-python-compileall:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-python-compileall" \
		--log-file staticcheck-python-compileall.log \
		--trs-file staticcheck-python-compileall.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-python-compileall.sh" \
		"$(abs_top_srcdir)" "$(PYTHON)"

staticcheck-private-includes:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-private-includes" \
		--log-file staticcheck-private-includes.log \
		--trs-file staticcheck-private-includes.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-private-includes.sh" \
		"$(abs_top_srcdir)" "$(PYTHON)"

staticcheck-test-runner-dllexport:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-test-runner-dllexport" \
		--log-file staticcheck-test-runner-dllexport.log \
		--trs-file staticcheck-test-runner-dllexport.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-test-runner-dllexport.sh" \
		"$(abs_top_srcdir)"

staticcheck-makefile-recipes:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-makefile-recipes" \
		--log-file staticcheck-makefile-recipes.log \
		--trs-file staticcheck-makefile-recipes.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-makefile-recipes.sh" \
		"$(abs_top_srcdir)"

staticcheck-meson-palette-sources:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-meson-palette-sources" \
		--log-file staticcheck-meson-palette-sources.log \
		--trs-file staticcheck-meson-palette-sources.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-meson-palette-sources.sh" \
		"$(abs_top_srcdir)"

staticcheck-kmedoids-suboption-env-sync:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-kmedoids-suboption-env-sync" \
		--log-file staticcheck-kmedoids-suboption-env-sync.log \
		--trs-file staticcheck-kmedoids-suboption-env-sync.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-kmedoids-suboption-env-sync.sh" \
		"$(abs_top_srcdir)"

staticcheck-test-plan-single:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-test-plan-single" \
		--log-file staticcheck-test-plan-single.log \
		--trs-file staticcheck-test-plan-single.trs \
		--enable-hard-errors \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-test-plan-single.sh" \
		"$(abs_top_srcdir)"

staticcheck-test-no-grep-awk:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-test-no-grep-awk" \
		--log-file staticcheck-test-no-grep-awk.log \
		--trs-file staticcheck-test-no-grep-awk.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-test-no-grep-awk.sh" \
		"$(abs_top_srcdir)"

staticcheck-artifact-local-dir-mkdir:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-artifact-local-dir-mkdir" \
		--log-file staticcheck-artifact-local-dir-mkdir.log \
		--trs-file staticcheck-artifact-local-dir-mkdir.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh" \
		"$(abs_top_srcdir)"

staticcheck-binding-loader-setopt-sync:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-binding-loader-setopt-sync" \
		--log-file staticcheck-binding-loader-setopt-sync.log \
		--trs-file staticcheck-binding-loader-setopt-sync.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-binding-loader-setopt-sync.sh" \
		"$(abs_top_srcdir)"

staticcheck-build-doc-configure-help:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-build-doc-configure-help" \
		--log-file staticcheck-build-doc-configure-help.log \
		--trs-file staticcheck-build-doc-configure-help.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-build-doc-configure-help.sh" \
		"$(abs_top_srcdir)"

staticcheck-build-doc-meson-options:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-build-doc-meson-options" \
		--log-file staticcheck-build-doc-meson-options.log \
		--trs-file staticcheck-build-doc-meson-options.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-build-doc-meson-options.sh" \
		"$(abs_top_srcdir)"

staticcheck-docs-envvars-help-table:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-docs-envvars-help-table" \
		--log-file staticcheck-docs-envvars-help-table.log \
		--trs-file staticcheck-docs-envvars-help-table.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-docs-envvars-help-table.sh" \
		"$(abs_top_srcdir)"

staticcheck-docs-help-vs-man:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-docs-help-vs-man" \
		--log-file staticcheck-docs-help-vs-man.log \
		--trs-file staticcheck-docs-help-vs-man.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-docs-help-vs-man.sh" \
		"$(abs_top_srcdir)"

staticcheck-docs-man-vs-bash-completion:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-docs-man-vs-bash-completion" \
		--log-file staticcheck-docs-man-vs-bash-completion.log \
		--trs-file staticcheck-docs-man-vs-bash-completion.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh" \
		"$(abs_top_srcdir)"

staticcheck-c-header-modeline:
	@TOP_SRCDIR='$(abs_top_srcdir)' TOP_BUILDDIR='$(abs_top_builddir)' \
	ARTIFACT_ROOT='$(ARTIFACT_ROOT)' \
	$(T_LOG_DRIVER) --test-name "staticcheck-c-header-modeline" \
		--log-file staticcheck-c-header-modeline.log \
		--trs-file staticcheck-c-header-modeline.trs \
		$(AM_T_LOG_DRIVER_FLAGS) $(T_LOG_DRIVER_FLAGS) -- \
		"$(top_srcdir)/tests/_static/sh/staticcheck-c-header-modeline.sh" \
		"$(abs_top_srcdir)"

# Route Python TAP tests through the same driver as shell TAP tests so
# elapsed time and result formatting stay consistent across extensions.
.py.log:
	@p='$<'; \
	$(am__set_b); \
	$(am__check_pre) $(T_LOG_DRIVER) --test-name "$$f" \
	--log-file $$b.log --trs-file $$b.trs \
	$(am__common_driver_flags) $(AM_T_LOG_DRIVER_FLAGS) \
	$(T_LOG_DRIVER_FLAGS) -- "$$tst" $(AM_TESTS_FD_REDIRECT)

# Route Ruby TAP tests through the same driver so Python and Ruby
# bindings share identical timeout and result formatting behavior.
.rb.log:
	@p='$<'; \
	$(am__set_b); \
	$(am__check_pre) $(T_LOG_DRIVER) --test-name "$$f" \
	--log-file $$b.log --trs-file $$b.trs \
	$(am__common_driver_flags) $(AM_T_LOG_DRIVER_FLAGS) \
	$(T_LOG_DRIVER_FLAGS) -- "$$tst" $(AM_TESTS_FD_REDIRECT)

# Route Perl TAP tests through the same driver so Python/Ruby/Perl
# bindings share identical timeout and result formatting behavior.
.pl.log:
	@p='$<'; \
	$(am__set_b); \
	$(am__check_pre) $(T_LOG_DRIVER) --test-name "$$f" \
	--log-file $$b.log --trs-file $$b.trs \
	$(am__common_driver_flags) $(AM_T_LOG_DRIVER_FLAGS) \
	$(T_LOG_DRIVER_FLAGS) -- "$$tst" $(AM_TESTS_FD_REDIRECT)

# Route PHP TAP tests through the same driver so bindings share one
# timeout and result formatting path.
.php.log:
	@p='$<'; \
	$(am__set_b); \
	SIXEL_TEST_PHP='$(SIXEL_TEST_PHP)' export SIXEL_TEST_PHP; \
	SIXEL_TEST_PHP_BINDING_ROOT='$(SIXEL_TEST_PHP_BINDING_ROOT)' \
	    export SIXEL_TEST_PHP_BINDING_ROOT; \
	SIXEL_TEST_PHP_LIBDIR='$(SIXEL_TEST_PHP_LIBDIR)' \
	    export SIXEL_TEST_PHP_LIBDIR; \
	SIXEL_TEST_PHP_LIBPATH='$(SIXEL_TEST_PHP_LIBPATH)' \
	    export SIXEL_TEST_PHP_LIBPATH; \
	$(am__check_pre) $(T_LOG_DRIVER) --test-name "$$f" \
	--log-file $$b.log --trs-file $$b.trs \
	$(am__common_driver_flags) $(AM_T_LOG_DRIVER_FLAGS) \
	$(T_LOG_DRIVER_FLAGS) -- "$$tst" $(AM_TESTS_FD_REDIRECT)

# Build all test logs through a phony target so recursive make does not need
# to receive thousands of .log paths as command-line arguments.
.PHONY: sixel-check-test-logs
sixel-check-test-logs: $(TEST_LOGS)
	@:

# Override check-TESTS to avoid nested single quotes in automake helpers.
# Export config.h feature toggles once so tests can rely on environment
# variables instead of repeatedly reading the header.
check-TESTS: $(check_PROGRAMS) prepare-binding-artifacts prepare-psd-large-fixtures
	@find "$(abs_builddir)" -type f -name '*.log' ! -name 'staticcheck-*.log' -exec rm -f {} +
	@find "$(abs_builddir)" -type f -name '*.trs' ! -name 'staticcheck-*.trs' -exec rm -f {} +
	@$(am__rm_f) $(TEST_SUITE_LOG)
	@set +e; \
	config_header="$(top_builddir)/config.h"; \
	resolved_test_paths=`$(SHELL) -xv "$(top_builddir)/build-aux/resolve-test-tool-paths.sh" \
	    "$(abs_top_builddir)" "$(srcdir)/check-test-exports.list" 2>$(top_builddir)/resolve-test-tool-paths.log` || { \
	    echo "error: failed to resolve test tool paths" >&2; \
	    exit 1; \
	}; \
	set -a; \
	eval "$$resolved_test_paths"; \
	for required_export in `$(SHELL) -xv "$(top_srcdir)/build-aux/read-check-test-list.sh" \
	    tests "$(srcdir)/check-test-exports.list" 2>$(top_builddir)/read-check-test-list.log`; do \
	    eval "required_value=\$${$$required_export-}"; \
	    if test "x$$required_value" = x; then \
	        echo "error: unresolved test export: $$required_export" >&2; \
	        exit 1; \
	    fi; \
	done; \
	config_exports=`$(top_srcdir)/build-aux/resolve-config-h-exports.sh "$$config_header" exports`; \
	eval "$$config_exports"; \
	set +a; \
	python_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-python-test-venv.sh" \
	    "@ENABLE_PYTHON@" "$(PYTHON)" "$(abs_top_builddir)/python/dist" \
	    "$(abs_top_builddir)/tests/.python-test-venv" "$(abs_top_srcdir)/python" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-python-test-venv.log`; \
	eval "$$python_venv_exports"; \
	ruby_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-ruby-test-venv.sh" \
	    "@ENABLE_RUBY@" "@RUBY@" "$(abs_top_builddir)/ruby/dist" \
	    "$(abs_top_builddir)/tests/.ruby-test-venv" "$(abs_top_srcdir)/ruby" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-ruby-test-venv.log`; \
	eval "$$ruby_venv_exports"; \
	perl_bin='@PERL@'; \
	if test -z "$$perl_bin"; then \
	    perl_bin=`command -v perl 2>/dev/null || printf "";`; \
	fi; \
	perl_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-perl-test-venv.sh" \
	    "@ENABLE_PERL@" "$$perl_bin" "$(abs_top_srcdir)/perl" "$(abs_top_builddir)/src/.libs" \
	    "$(abs_top_builddir)/tests/.perl-test-venv" 2>$(top_builddir)/resolve-perl-test-venv.log`; \
	eval "$$perl_venv_exports"; \
	php_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-php-test-venv.sh" \
	    "@ENABLE_PHP@" "@PHP@" "$(abs_top_builddir)/php/dist" \
	    "$(abs_top_builddir)/tests/.php-test-venv" "$(abs_top_srcdir)/php" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-php-test-venv.log`; \
	eval "$$php_venv_exports"; \
	if test "@ENABLE_PHP@" = "1" && test -z "$$SIXEL_TEST_PHP"; then \
	    echo "error: PHP bindings are enabled but no venv PHP interpreter was resolved at $(abs_top_builddir)/tests/.php-test-venv" >&2; \
	    echo "error: system PHP fallback is disabled for PHP TAP tests" >&2; \
	    exit 1; \
	fi; \
	python_make_arg=; \
	ruby_make_arg=; \
	perl_make_arg=; \
	php_make_arg=; \
	if test -n "$$SIXEL_TEST_PYTHON"; then \
	    export SIXEL_TEST_PYTHON; \
	    python_make_arg="SIXEL_TEST_PYTHON=$$SIXEL_TEST_PYTHON"; \
	fi; \
	if test -n "$$SIXEL_TEST_RUBY"; then \
	    export SIXEL_TEST_RUBY; \
	    ruby_make_arg="SIXEL_TEST_RUBY=$$SIXEL_TEST_RUBY"; \
	fi; \
	if test -n "$$SIXEL_TEST_RUBY_GEM_HOME"; then \
	    export SIXEL_TEST_RUBY_GEM_HOME; \
	    ruby_make_arg="$$ruby_make_arg SIXEL_TEST_RUBY_GEM_HOME=$$SIXEL_TEST_RUBY_GEM_HOME"; \
	fi; \
	if test -n "$$SIXEL_TEST_RUBYLIB"; then \
	    export SIXEL_TEST_RUBYLIB; \
	    ruby_make_arg="$$ruby_make_arg SIXEL_TEST_RUBYLIB=$$SIXEL_TEST_RUBYLIB"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL"; then \
	    export SIXEL_TEST_PERL; \
	    perl_make_arg="SIXEL_TEST_PERL=$$SIXEL_TEST_PERL"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_CPANM"; then \
	    export SIXEL_TEST_PERL_CPANM; \
	    perl_make_arg="$$perl_make_arg SIXEL_TEST_PERL_CPANM=$$SIXEL_TEST_PERL_CPANM"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT"; then \
	    export SIXEL_TEST_PERL_LOCAL_LIB_ROOT; \
	    perl_make_arg="$$perl_make_arg SIXEL_TEST_PERL_LOCAL_LIB_ROOT=$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL5LIB"; then \
	    export SIXEL_TEST_PERL5LIB; \
	    perl_make_arg="$$perl_make_arg SIXEL_TEST_PERL5LIB=$$SIXEL_TEST_PERL5LIB"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_MB_OPT"; then \
	    export SIXEL_TEST_PERL_MB_OPT; \
	    perl_make_arg="$$perl_make_arg SIXEL_TEST_PERL_MB_OPT=$$SIXEL_TEST_PERL_MB_OPT"; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_MM_OPT"; then \
	    export SIXEL_TEST_PERL_MM_OPT; \
	    perl_make_arg="$$perl_make_arg SIXEL_TEST_PERL_MM_OPT=$$SIXEL_TEST_PERL_MM_OPT"; \
	fi; \
	if test -n "$$SIXEL_TEST_PHP"; then \
	    export SIXEL_TEST_PHP; \
	    php_make_arg="SIXEL_TEST_PHP=$$SIXEL_TEST_PHP"; \
	fi; \
	if test -n "$$SIXEL_TEST_PHPDBG"; then \
	    export SIXEL_TEST_PHPDBG; \
	    php_make_arg="$$php_make_arg SIXEL_TEST_PHPDBG=$$SIXEL_TEST_PHPDBG"; \
	fi; \
	if test -n "$$SIXEL_TEST_PHP_BINDING_ROOT"; then \
	    export SIXEL_TEST_PHP_BINDING_ROOT; \
	    php_make_arg="$$php_make_arg SIXEL_TEST_PHP_BINDING_ROOT=$$SIXEL_TEST_PHP_BINDING_ROOT"; \
	fi; \
	if test -n "$$SIXEL_TEST_PHP_LIBDIR"; then \
	    export SIXEL_TEST_PHP_LIBDIR; \
	    php_make_arg="$$php_make_arg SIXEL_TEST_PHP_LIBDIR=$$SIXEL_TEST_PHP_LIBDIR"; \
	fi; \
	if test -n "$$SIXEL_TEST_PHP_LIBPATH"; then \
	    export SIXEL_TEST_PHP_LIBPATH; \
	    php_make_arg="$$php_make_arg SIXEL_TEST_PHP_LIBPATH=$$SIXEL_TEST_PHP_LIBPATH"; \
	fi; \
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner@EXEEXT@ --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	mkdir -p "$(ARTIFACT_ROOT)"; \
	trs_manifest="$(abs_builddir)/.check-test-trs.list"; \
	failed_manifest="$(abs_builddir)/.check-test-failures.list"; \
	testsuite_tmp="$(TEST_SUITE_LOG).tmp"; \
	rm -f "$$trs_manifest"; \
	rm -f "$$failed_manifest"; \
	find "$(abs_builddir)" -type f -name '*.trs' ! -name 'staticcheck-*.trs' -exec rm -f {} +; \
	$(MAKE) $(AM_MAKEFLAGS) sixel-check-test-logs \
	    $$python_make_arg \
	    $$ruby_make_arg \
	    $$perl_make_arg \
	    $$php_make_arg || exit $$?; \
	find "$(abs_builddir)" -type f -name '*.trs' ! -name 'staticcheck-*.trs' | LC_ALL=C sort > "$$trs_manifest"; \
	all=0; \
	pass=0; \
	fail=0; \
	skip=0; \
	xfail=0; \
	xpass=0; \
	error=0; \
	{ \
	  echo "$(PACKAGE_STRING): $(subdir)/$(TEST_SUITE_LOG)"; \
	  echo; \
	} > "$$testsuite_tmp"; \
	while IFS= read -r trs_path; do \
	    test -n "$$trs_path" || continue; \
	    log_path="$${trs_path%.trs}.log"; \
	    rel_name="$${trs_path#$(abs_builddir)/}"; \
	    rel_name="$${rel_name%.trs}"; \
	    global_result=RUN; \
	    copy_in_global_log=yes; \
	    if test ! -r "$$trs_path"; then \
	        error=$$((error + 1)); \
	        printf '%s\n' "ERROR: $$rel_name (missing .trs)" >> "$$failed_manifest"; \
	        printf '%s\n' "ERROR: missing $$trs_path" >> "$$testsuite_tmp"; \
	        continue; \
	    fi; \
	    while IFS= read -r trs_line || test -n "$$trs_line"; do \
	        case "$$trs_line" in \
	            :test-result:*) \
	                all=$$((all + 1)); \
	                set -- $${trs_line#*:test-result:}; \
	                case "$${1-}" in \
	                    PASS) pass=$$((pass + 1)) ;; \
	                    FAIL) fail=$$((fail + 1)) ;; \
	                    SKIP) skip=$$((skip + 1)) ;; \
	                    XFAIL) xfail=$$((xfail + 1)) ;; \
	                    XPASS) xpass=$$((xpass + 1)) ;; \
	                    ERROR) error=$$((error + 1)) ;; \
	                esac \
	                ;; \
	            :global-test-result:*) \
	                set -- $${trs_line#*:global-test-result:}; \
	                global_result=$${1-RUN} \
	                ;; \
	            :copy-in-global-log:*) \
	                set -- $${trs_line#*:copy-in-global-log:}; \
	                case "$${1-}" in \
	                    [Nn][Oo]) copy_in_global_log=no ;; \
	                esac \
	                ;; \
	        esac; \
	    done < "$$trs_path"; \
	    case "$$global_result" in \
	        FAIL|XPASS|ERROR) \
	            printf '%s\n' "$$global_result: $$rel_name" >> "$$failed_manifest" \
	            ;; \
	    esac; \
	    if test "$$copy_in_global_log" = yes; then \
	        { \
	            printf '%s\n' "$$global_result: $$rel_name"; \
	            if test -r "$$log_path"; then \
	                cat "$$log_path"; \
	            else \
	                printf '%s\n' "missing log file: $$log_path"; \
	                error=$$((error + 1)); \
	            fi; \
	            echo; \
	        } >> "$$testsuite_tmp"; \
	    fi; \
	done < "$$trs_manifest"; \
	{ \
	  echo "TOTAL: $$all"; \
	  echo "PASS: $$pass"; \
	  echo "SKIP: $$skip"; \
	  echo "XFAIL: $$xfail"; \
	  echo "FAIL: $$fail"; \
	  echo "XPASS: $$xpass"; \
	  echo "ERROR: $$error"; \
	} >> "$$testsuite_tmp"; \
	mv "$$testsuite_tmp" "$(TEST_SUITE_LOG)"; \
	echo "================================================================"; \
	echo "Testsuite summary$(AM_TESTSUITE_SUMMARY_HEADER)"; \
	echo "================================================================"; \
	echo "# TOTAL: $$all"; \
	echo "# PASS:  $$pass"; \
	echo "# SKIP:  $$skip"; \
	echo "# XFAIL: $$xfail"; \
	echo "# FAIL:  $$fail"; \
	echo "# XPASS: $$xpass"; \
	echo "# ERROR: $$error"; \
	if test -s "$$failed_manifest"; then \
	    echo "#"; \
	    echo "# Failed tests:"; \
	    sed 's/^/#   /' "$$failed_manifest"; \
	fi; \
	if test $$((fail + xpass + error)) -ne 0; then \
	    test x"$$VERBOSE" = x || cat "$(TEST_SUITE_LOG)"; \
	    echo "See $(subdir)/$(TEST_SUITE_LOG) for debugging."; \
	    rm -f "$$trs_manifest" "$$failed_manifest"; \
	    exit 1; \
	fi; \
	rm -f "$$trs_manifest" "$$failed_manifest"; \
	exit 0

# Avoid expanding $(TEST_LOGS) into a single rm invocation during
# mostlyclean/distclean; the test suite can exceed shell argument limits.
mostlyclean-generic:
	-@find "$(abs_builddir)" -type f \( -name '*.log' -o -name '*.trs' \) -exec rm -f {} +

.PHONY: check-binding-loader-setopt-sync prepare-binding-artifacts prepare-psd-large-fixtures check-python check-ruby check-perl check-php sixel-check-test-logs
check-binding-loader-setopt-sync:
	@set -eu; \
	"$(SHELL)" "$(abs_top_srcdir)/tests/bindings/check-loader-setopt-sync.sh" "$(abs_top_srcdir)"

prepare-binding-artifacts:
	@set -eu; \
	if test "@ENABLE_PYTHON@" = "1"; then \
	    $(MAKE) $(AM_MAKEFLAGS) -C "$(abs_top_builddir)/python" wheel-local; \
	fi; \
	if test "@ENABLE_RUBY@" = "1"; then \
	    $(MAKE) $(AM_MAKEFLAGS) -C "$(abs_top_builddir)/ruby" all-local; \
	fi; \
	if test "@ENABLE_PHP@" = "1"; then \
	    $(MAKE) $(AM_MAKEFLAGS) -C "$(abs_top_builddir)/php" all-local; \
	fi

prepare-psd-large-fixtures:
	@set -eu; \
	"$(SHELL)" "$(abs_top_srcdir)/tests/_static/sh/prepare-psb-large-fixtures.sh" \
	    "$(abs_top_srcdir)" "$(abs_top_builddir)"

check-python: prepare-binding-artifacts
	@set -eu; \
	python_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-python-test-venv.sh" \
	    "@ENABLE_PYTHON@" "$(PYTHON)" "$(abs_top_builddir)/python/dist" \
	    "$(abs_top_builddir)/tests/.python-test-venv" "$(abs_top_srcdir)/python" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-python-test-venv.log`; \
	eval "$$python_venv_exports"; \
	if test -z "$$SIXEL_TEST_PYTHON"; then \
	    echo "check-python: skipped because Python test interpreter is unavailable"; \
	    exit 0; \
	fi; \
	"$$SIXEL_TEST_PYTHON" -m pip install --disable-pip-version-check --quiet \
	    pytest coverage; \
	mkdir -p '$(abs_top_builddir)/tests/_artifacts/python/pytest'; \
	ARTIFACT_LOCAL_DIR='$(abs_top_builddir)/tests/_artifacts/python/pytest' \
	TOP_SRCDIR='$(abs_top_srcdir)' \
	TOP_BUILDDIR='$(abs_top_builddir)' \
	LIBSIXEL_LIBDIR='$(abs_top_builddir)/src' \
	"$$SIXEL_TEST_PYTHON" -m coverage run \
	    --source=libsixel_wheel \
	    -m pytest -q -o python_files='[0-9][0-9][0-9][0-9]_*.py' \
	    '$(abs_top_srcdir)/tests/bindings/python'; \
	"$$SIXEL_TEST_PYTHON" -m coverage lcov -o '$(abs_top_builddir)/python-coverage.info'; \
	python_cov_tmp='$(abs_top_builddir)/python-coverage.info.tmp'; \
	sed -E 's#^SF:.*site-packages/libsixel_wheel/#SF:python/libsixel/#' \
	    '$(abs_top_builddir)/python-coverage.info' > "$$python_cov_tmp"; \
	mv "$$python_cov_tmp" '$(abs_top_builddir)/python-coverage.info'; \
	echo "python coverage exported to $(abs_top_builddir)/python-coverage.info"

check-ruby: prepare-binding-artifacts
	@set -eu; \
	ruby_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-ruby-test-venv.sh" \
	    "@ENABLE_RUBY@" "@RUBY@" "$(abs_top_builddir)/ruby/dist" \
	    "$(abs_top_builddir)/tests/.ruby-test-venv" "$(abs_top_srcdir)/ruby" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-ruby-test-venv.log`; \
	eval "$$ruby_venv_exports"; \
	if test -z "$$SIXEL_TEST_RUBY"; then \
	    echo "check-ruby: skipped because Ruby test interpreter is unavailable"; \
	    exit 0; \
	fi; \
	if test -n "$$SIXEL_TEST_RUBY_GEM_HOME"; then \
	    GEM_HOME="$$SIXEL_TEST_RUBY_GEM_HOME" export GEM_HOME; \
	    GEM_PATH="$$SIXEL_TEST_RUBY_GEM_HOME" export GEM_PATH; \
	fi; \
	if test -n "$$SIXEL_TEST_RUBYLIB"; then \
	    if test -n "$${RUBYLIB:-}"; then \
	        RUBYLIB="$$SIXEL_TEST_RUBYLIB:$${RUBYLIB}" export RUBYLIB; \
	    else \
	        RUBYLIB="$$SIXEL_TEST_RUBYLIB" export RUBYLIB; \
	    fi; \
	fi; \
	artifact_root='$(abs_top_builddir)/tests/_artifacts/ruby/tap'; \
	coverage_root='$(abs_top_builddir)/tests/_artifacts/ruby/coverage'; \
	coverage_runner='$(abs_top_srcdir)/tests/bindings/ruby/_coverage_runner.rb'; \
	coverage_converter='$(abs_top_srcdir)/tests/bindings/ruby/_coverage_to_lcov.rb'; \
	lcov_path='$(abs_top_builddir)/ruby-coverage.info'; \
	mkdir -p "$$artifact_root"; \
	rm -rf "$$coverage_root"; \
	mkdir -p "$$coverage_root"; \
	status=0; \
	test_seq=0; \
	for test_path in `find '$(abs_top_srcdir)/tests/bindings/ruby' -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.rb' | LC_ALL=C sort`; do \
	    test_name="$${test_path##*/}"; \
	    test_name="$${test_name%.rb}"; \
	    test_seq=$$((test_seq + 1)); \
	    test_id="$${test_name%%_*}"; \
	    case "$$test_id" in \
	        [0-9][0-9][0-9][0-9]) ;; \
	        *) test_id=t ;; \
	    esac; \
	    artifact_dir="$$artifact_root/$$test_id-$$test_seq"; \
	    log_path="$$artifact_root/$$test_id-$$test_seq.log"; \
	    coverage_path="$$coverage_root/$$test_id-$$test_seq.marshal"; \
	    mkdir -p "$$artifact_dir"; \
	    echo "==> $$test_name.rb"; \
	    if ARTIFACT_LOCAL_DIR="$$artifact_dir" \
	        TOP_SRCDIR='$(abs_top_srcdir)' \
	        TOP_BUILDDIR='$(abs_top_builddir)' \
	        LIBSIXEL_LIBDIR='$(abs_top_builddir)/src' \
	        "$$SIXEL_TEST_RUBY" --disable-gems "$$coverage_runner" "$$test_path" "$$coverage_path" >"$$log_path" 2>&1; then \
	        tap_not_ok=0; \
	        while IFS= read -r tap_line || test -n "$$tap_line"; do \
	            case "$$tap_line" in \
	                "not ok"|\
	                "not ok "*) \
	                    tap_not_ok=1; \
	                    break; \
	                    ;; \
	            esac; \
	        done < "$$log_path"; \
	        if test "$$tap_not_ok" -ne 0; then \
	            echo "not ok - $$test_name.rb (TAP not ok found)"; \
	            sed -n '1,120p' "$$log_path"; \
	            status=1; \
	        fi; \
	    else \
	        echo "not ok - $$test_name.rb (ruby exit code != 0)"; \
	        sed -n '1,120p' "$$log_path"; \
	        status=1; \
	    fi; \
	done; \
	"$$SIXEL_TEST_RUBY" --disable-gems "$$coverage_converter" "$$coverage_root" "$$lcov_path" '$(abs_top_srcdir)' || status=1; \
	echo "ruby coverage exported to $$lcov_path"; \
	if test "$$status" -ne 0; then \
	    exit "$$status"; \
	fi; \
	echo "ruby TAP logs exported to $$artifact_root"

check-perl:
	@set -eu; \
	perl_bin='@PERL@'; \
	if test -z "$$perl_bin"; then \
	    perl_bin=`command -v perl 2>/dev/null || printf "";`; \
	fi; \
	perl_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-perl-test-venv.sh" \
	    1 "$$perl_bin" "$(abs_top_srcdir)/perl" "$(abs_top_builddir)/src/.libs" \
	    "$(abs_top_builddir)/tests/.perl-test-venv" 2>$(top_builddir)/resolve-perl-test-venv.log`; \
	eval "$$perl_venv_exports"; \
	if test -z "$$SIXEL_TEST_PERL"; then \
	    echo "check-perl: skipped because Perl test interpreter is unavailable"; \
	    exit 0; \
	fi; \
	if test -z "$$SIXEL_TEST_PERL_CPANM"; then \
	    echo "check-perl: skipped because cpanm is unavailable for Perl test venv"; \
	    exit 0; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL5LIB"; then \
	    if test -n "$${PERL5LIB:-}"; then \
	        PERL5LIB="$$SIXEL_TEST_PERL5LIB:$${PERL5LIB}" export PERL5LIB; \
	    else \
	        PERL5LIB="$$SIXEL_TEST_PERL5LIB" export PERL5LIB; \
	    fi; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT"; then \
	    PERL_LOCAL_LIB_ROOT="$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" export PERL_LOCAL_LIB_ROOT; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_MB_OPT"; then \
	    PERL_MB_OPT="$$SIXEL_TEST_PERL_MB_OPT" export PERL_MB_OPT; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_MM_OPT"; then \
	    PERL_MM_OPT="$$SIXEL_TEST_PERL_MM_OPT" export PERL_MM_OPT; \
	fi; \
	if test -n "$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT"; then \
	    PERL_CPANM_HOME="$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT/.cpanm" export PERL_CPANM_HOME; \
	    mkdir -p "$$PERL_CPANM_HOME"; \
	fi; \
	"$$SIXEL_TEST_PERL" "$$SIXEL_TEST_PERL_CPANM" --quiet --notest --skip-satisfied \
	    --local-lib-contained "$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" Devel::Cover; \
	cover_bin="$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT/bin/cover"; \
	if test ! -x "$$cover_bin"; then \
	    echo "check-perl: expected cover executable was not found at $$cover_bin" >&2; \
	    exit 1; \
	fi; \
	artifact_root='$(abs_top_builddir)/tests/_artifacts/perl/tap'; \
	coverage_root='$(abs_top_builddir)/tests/_artifacts/perl/coverage'; \
	coverage_converter='$(abs_top_srcdir)/tests/bindings/perl/_coverage_to_lcov.pl'; \
	lcov_path='$(abs_top_builddir)/perl-coverage.info'; \
	mkdir -p "$$artifact_root"; \
	rm -rf "$$coverage_root"; \
	mkdir -p "$$coverage_root"; \
	status=0; \
	test_seq=0; \
	for test_path in `find '$(abs_top_srcdir)/tests/bindings/perl' -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.pl' | LC_ALL=C sort`; do \
	    test_name=`basename "$$test_path" .pl`; \
	    test_seq=$$((test_seq + 1)); \
	    test_id="$${test_name%%_*}"; \
	    case "$$test_id" in \
	        [0-9][0-9][0-9][0-9]) ;; \
	        *) test_id=t ;; \
	    esac; \
	    artifact_dir="$$artifact_root/$$test_id-$$test_seq"; \
	    log_path="$$artifact_root/$$test_id-$$test_seq.log"; \
	    mkdir -p "$$artifact_dir"; \
	    echo "==> $$test_name.pl"; \
	    if ARTIFACT_LOCAL_DIR="$$artifact_dir" \
	        TOP_SRCDIR='$(abs_top_srcdir)' \
	        TOP_BUILDDIR='$(abs_top_builddir)' \
	        LIBSIXEL_LIBDIR='$(abs_top_builddir)/src' \
	        "$$SIXEL_TEST_PERL" \
	            -MDevel::Cover=-db,$$coverage_root,-silent,1 \
	            "$$test_path" >"$$log_path" 2>&1; then \
	        if grep -Eq '^not ok([[:space:]]|$$)' "$$log_path"; then \
	            echo "not ok - $$test_name.pl (TAP not ok found)"; \
	            sed -n '1,120p' "$$log_path"; \
	            status=1; \
	        fi; \
	    else \
	        echo "not ok - $$test_name.pl (perl exit code != 0)"; \
	        sed -n '1,120p' "$$log_path"; \
	        status=1; \
	    fi; \
	done; \
	"$$cover_bin" "$$coverage_root" -silent 1 > /dev/null 2>&1 || status=1; \
	"$$SIXEL_TEST_PERL" "$$coverage_converter" "$$coverage_root" "$$lcov_path" \
	    '$(abs_top_srcdir)' "$$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" || status=1; \
	echo "perl coverage exported to $$lcov_path"; \
	if test "$$status" -ne 0; then \
	    exit "$$status"; \
	fi; \
	echo "perl TAP logs exported to $$artifact_root"

check-php: prepare-binding-artifacts
	@set -eu; \
	php_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-php-test-venv.sh" \
	    "@ENABLE_PHP@" "@PHP@" "$(abs_top_builddir)/php/dist" \
	    "$(abs_top_builddir)/tests/.php-test-venv" "$(abs_top_srcdir)/php" \
	    "$(abs_top_builddir)/src/.libs" 2>$(top_builddir)/resolve-php-test-venv.log`; \
	eval "$$php_venv_exports"; \
	if test "@ENABLE_PHP@" != "1"; then \
	    echo "check-php: skipped because PHP bindings are disabled"; \
	    exit 0; \
	fi; \
	if test -z "$$SIXEL_TEST_PHP"; then \
	    echo "check-php: failed because venv PHP interpreter is unavailable (system fallback is disabled)" >&2; \
	    exit 1; \
	fi; \
	if test -z "$$SIXEL_TEST_PHPDBG"; then \
	    echo "check-php: failed because phpdbg is unavailable in the PHP test venv" >&2; \
	    exit 1; \
	fi; \
	if test -z "$$SIXEL_TEST_PHP_BINDING_ROOT" || test -z "$$SIXEL_TEST_PHP_LIBPATH"; then \
	    echo "check-php: failed because packaged PHP binding payload is unavailable in the venv" >&2; \
	    exit 1; \
	fi; \
	status=0; \
	artifact_root='$(abs_top_builddir)/tests/_artifacts/php/tap'; \
	coverage_root='$(abs_top_builddir)/tests/_artifacts/php/coverage'; \
	coverage_runner='$(abs_top_srcdir)/tests/bindings/php/_coverage_runner.php'; \
	coverage_converter='$(abs_top_srcdir)/tests/bindings/php/_coverage_to_lcov.php'; \
	lcov_path='$(abs_top_builddir)/php-coverage.info'; \
	mkdir -p "$$artifact_root"; \
	rm -rf "$$coverage_root"; \
	mkdir -p "$$coverage_root"; \
	test_seq=0; \
	for test_path in `find '$(abs_top_srcdir)/tests/bindings/php' -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.php' | LC_ALL=C sort`; do \
	    test_name=`basename "$$test_path" .php`; \
	    test_seq=$$((test_seq + 1)); \
	    test_id="$${test_name%%_*}"; \
	    case "$$test_id" in \
	        [0-9][0-9][0-9][0-9]) ;; \
	        *) test_id=t ;; \
	    esac; \
	    artifact_dir="$$artifact_root/$$test_id-$$test_seq"; \
	    log_path="$$artifact_root/$$test_id-$$test_seq.log"; \
	    coverage_path="$$coverage_root/$$test_id-$$test_seq.json"; \
	    mkdir -p "$$artifact_dir"; \
	    echo "==> $$test_name.php"; \
	    if ARTIFACT_LOCAL_DIR="$$artifact_dir" \
	        TOP_SRCDIR='$(abs_top_srcdir)' \
	        TOP_BUILDDIR='$(abs_top_builddir)' \
	        LIBSIXEL_LIBDIR="$$SIXEL_TEST_PHP_LIBDIR" \
	        LIBSIXEL_LIBPATH="$$SIXEL_TEST_PHP_LIBPATH" \
	        SIXEL_TEST_PHP_BINDING_ROOT="$$SIXEL_TEST_PHP_BINDING_ROOT" \
	        "$$SIXEL_TEST_PHPDBG" -d ffi.enable=true -qrr "$$coverage_runner" "$$test_path" "$$coverage_path" >"$$log_path" 2>&1; then \
	        if grep -Eq '^not ok([[:space:]]|$$)' "$$log_path"; then \
	            echo "not ok - $$test_name.php (TAP not ok found)"; \
	            sed -n '1,120p' "$$log_path"; \
	            status=1; \
	        fi; \
	    else \
	        echo "not ok - $$test_name.php (phpdbg exit code != 0)"; \
	        sed -n '1,120p' "$$log_path"; \
	        status=1; \
	    fi; \
	done; \
	"$$SIXEL_TEST_PHP" "$$coverage_converter" "$$coverage_root" "$$lcov_path" \
	    '$(abs_top_srcdir)' || status=1; \
	echo "php coverage exported to $$lcov_path"; \
	if test "$$status" -ne 0; then \
	    exit "$$status"; \
	fi; \
	echo "php TAP logs exported to $$artifact_root"

# Distribute fixture trees as directories so DISTFILES remains short and
# distdir does not hit shell ARG_MAX limits on some platforms.
TEST_DATA_DIRS = data

EXTRA_DIST = $(HELPER_FILES) \
    loader/builtin/0014_loader_builtin_pixelformat_common.inc.c \
    loader/builtin/0014_loader_builtin_pixelformat_hdr.inc.c \
    loader/builtin/0014_loader_builtin_pixelformat_gif_psd.inc.c \
    _static/sh/prepare-psb-large-fixtures.sh \
    _static/sh/shellcheck.sh \
    _static/sh/staticcheck-suite.sh \
    _static/sh/staticcheck-actionlint.sh \
    _static/sh/staticcheck-binding-loader-setopt-sync.sh \
    _static/sh/staticcheck-shellcheck.sh \
    _static/sh/staticcheck-codespell.sh \
    _static/sh/staticcheck-python-compileall.sh \
    _static/sh/staticcheck-private-includes.sh \
    _static/sh/staticcheck-test-runner-dllexport.sh \
    _static/sh/staticcheck-makefile-recipes.sh \
    _static/sh/staticcheck-meson-palette-sources.sh \
    _static/sh/staticcheck-kmedoids-suboption-env-sync.sh \
    _static/sh/staticcheck-test-plan-single.sh \
    _static/sh/staticcheck-test-no-grep-awk.sh \
    _static/sh/staticcheck-artifact-local-dir-mkdir.sh \
    _static/sh/staticcheck-build-doc-configure-help.sh \
    _static/sh/staticcheck-build-doc-meson-options.sh \
    _static/sh/staticcheck-docs-help-vs-man.sh \
    _static/sh/staticcheck-docs-man-vs-bash-completion.sh \
    _static/sh/staticcheck-docs-envvars-help-table.sh \
    _static/sh/staticcheck-c-header-modeline.sh \
    check-test-exports.list \
    $(TEST_DATA_DIRS)

# Keep distdir recipe lines below per-argument shell limits by copying
# discovered test scripts in dist-hook instead of expanding $(TESTS) in
# EXTRA_DIST.
dist-hook:
	@set -eu; \
	$(SHELL) "$(top_srcdir)/build-aux/read-check-test-list.sh" \
	    tests "$(srcdir)" include | tr ' ' '\n' | \
	while IFS= read -r test_path; do \
	    test -n "$$test_path" || continue; \
	    dst="$(distdir)/$$test_path"; \
	    $(MKDIR_P) "`dirname "$$dst"`"; \
	    cp -p "$(srcdir)/$$test_path" "$$dst"; \
	done

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
	rm -f $(LOG_COMPILER) $(HELPER_FILES); \
	$(SHELL) "$(top_srcdir)/build-aux/read-check-test-list.sh" \
	    tests "$(abs_srcdir)" include | tr ' ' '\n' | \
	while IFS= read -r test_path; do \
	    test -n "$$test_path" || continue; \
	    rm -f "$$test_path"; \
	done; \
	fi
	rm -rf $(ARTIFACT_ROOT)
	rm -rf $(abs_top_builddir)/tests/.python-test-venv
	rm -rf $(abs_top_builddir)/tests/.ruby-test-venv
	rm -rf $(abs_top_builddir)/tests/.perl-test-venv
	rm -rf $(abs_top_builddir)/tests/.php-test-venv
	rm -f $(abs_top_builddir)/tests/data/inputs/formats/webp-static-icc-overlimit-padded.webp
	rm -f $(abs_top_builddir)/tests/data/inputs/formats/snake16_psb_*_high_offset_xxlarge*.psd
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
