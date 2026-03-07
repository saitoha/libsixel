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

TEST_EXTENSIONS = .t .py .rb


HELPER_FILES = \
    _lib/sh/common.sh \
    docs/consistency/list_envvars.sh \
    bindings/python/_taptest.py

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
AM_TESTS_ENVIRONMENT += SIXEL_CONFIGURED_RUBY='@RUBY@'; export SIXEL_CONFIGURED_RUBY;
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
    -DBUILD_TEST_0013_LOADER_LIBWEBP_PIXELFORMAT \
    -DBUILD_TEST_0014_LOADER_LIBWEBP_ANIMATION_FRAMES \
    -DBUILD_TEST_0015_LOADER_LIBWEBP_PALETTE_PROMOTION_GUARD \
    -DBUILD_TEST_0020_LOADER_LIBRSVG_DETECT_SVG_LIKE \
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
    loader/librsvg/0020_loader_librsvg_detect_svg_like.c \
    loader/pixelformat_test_common.h \
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
	staticcheck-build-doc-configure-help \
	staticcheck-build-doc-meson-options

staticcheck: \
	staticcheck-clean \
	staticcheck-actionlint \
	staticcheck-shellcheck \
	staticcheck-codespell \
	staticcheck-python-compileall \
	staticcheck-build-doc-configure-help \
	staticcheck-build-doc-meson-options

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

# Override check-TESTS to avoid nested single quotes in automake helpers.
# Export config.h feature toggles once so tests can rely on environment
# variables instead of repeatedly reading the header.
check-TESTS: $(check_PROGRAMS)
	@$(am__rm_f) $(RECHECK_LOGS)
	@$(am__rm_f) $(RECHECK_LOGS:.log=.trs)
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
	    "$(abs_top_builddir)/tests/.python-test-venv" 2>$(top_builddir)/resolve-python-test-venv.log`; \
	eval "$$python_venv_exports"; \
	ruby_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-ruby-test-venv.sh" \
	    "@ENABLE_RUBY@" "@RUBY@" "$(abs_top_builddir)/ruby/dist" \
	    "$(abs_top_builddir)/tests/.ruby-test-venv" 2>$(top_builddir)/resolve-ruby-test-venv.log`; \
	eval "$$ruby_venv_exports"; \
	python_make_arg=; \
	ruby_make_arg=; \
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
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner@EXEEXT@ --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	$(SIXEL_RUNTIME) $(top_builddir)/tests/test_runner --is-running-under-wine && export RUNTIME_ENV_IS_WINE=1; \
	log_list=`echo $(TEST_LOGS)`; \
	mkdir -p "$(ARTIFACT_ROOT)"; \
	$(MAKE) $(AM_MAKEFLAGS) $(TEST_SUITE_LOG) \
	    TEST_LOGS="$$log_list" \
	    $$python_make_arg \
	    $$ruby_make_arg; \
	exit $$?;

.PHONY: check-python
check-python:
	@set -eu; \
	python_venv_exports=`$(SHELL) -xv "$(top_srcdir)/build-aux/resolve-python-test-venv.sh" \
	    "@ENABLE_PYTHON@" "$(PYTHON)" "$(abs_top_builddir)/python/dist" \
	    "$(abs_top_builddir)/tests/.python-test-venv" 2>$(top_builddir)/resolve-python-test-venv.log`; \
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
	echo "python coverage exported to $(abs_top_builddir)/python-coverage.info"

# Collect all test fixture files under tests/data automatically so
# EXTRA_DIST stays in sync when fixtures are added, removed, or moved.
# LC_ALL=C keeps path ordering deterministic across locales.
TEST_DATA_FILES = $(shell cd $(srcdir) && find data -type f | LC_ALL=C sort)

EXTRA_DIST = $(TESTS) $(HELPER_FILES) \
    _static/sh/shellcheck.sh \
    _static/sh/staticcheck-actionlint.sh \
    _static/sh/staticcheck-shellcheck.sh \
    _static/sh/staticcheck-codespell.sh \
    _static/sh/staticcheck-python-compileall.sh \
    _static/sh/staticcheck-build-doc-configure-help.sh \
    _static/sh/staticcheck-build-doc-meson-options.sh \
    check-test-exports.list \
    $(TEST_DATA_FILES)

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
	rm -rf $(abs_top_builddir)/tests/.ruby-test-venv
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
