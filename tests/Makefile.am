CLI_TESTS = cli/t/test_0001_01_invalid_positional.t \
	cli/t/test_0001_02_invalid_option_arguments.t \
	cli/t/test_0001_03_invalid_option_combinations.t \
	cli/t/test_0002_stdin.t \
	cli/t/test_0003_information.t \
	cli/t/test_0004_conversion_options_01.t \
	cli/t/test_0004_conversion_options_02.t \
	cli/t/test_0004_conversion_options_03.t \
	cli/t/test_0004_conversion_options_04.t \
	cli/t/test_0005_dcs_arguments.t \
	cli/t/test_0006_dcs_variations.t \
	cli/t/test_0007_animation.t \
	cli/t/test_0008_progressive_jpeg.t \
	cli/t/test_0009_pngsuite.t \
	cli/t/test_0010_curl.t \
	cli/t/test_0011_basic.t \
	cli/t/test_0012_regressions.t \
	cli/t/test_0013_documentation.t \
	cli/t/test_0014_option_matching.t \
	cli/t/test_0015_argument_shift.t \
	cli/t/test_0016_assessment_spool.t \
	cli/t/test_0017_clipboard.t \
	cli/t/test_0018_lsqa_regression.t \
	cli/t/test_0019_python_wheel.t \
	cli/t/test_0020_mapfile_io.t

PYTHON_TESTS = python/t/test_python_bindings.t \
       python/t/test_python_api_formats.t

TESTS = $(CLI_TESTS) $(PYTHON_TESTS)
TEST_EXTENSIONS = .t

HELPER_FILES = cli/t/converters-common.t \
       python/lib/common.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;

TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/build-aux/tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments
LOG_COMPILER = $(builddir)/tools/with-timeout.sh
AM_LOG_FLAGS = --timeout=$(AM_TEST_TIMEOUT)
AM_TEST_TIMEOUT = 300

EXTRA_DIST = $(TESTS) $(HELPER_FILES) tools/with-timeout.sh \
	     lsqa_regression.sh \
	     data/baseline/*.json data/inputs/formats/* \
	     data/resolutions/*.png data/corrupted/*
# Avoid deleting the in-tree timeout wrapper when building in the source
# directory. The wrapper is regenerated for out-of-tree builds, so remove it
# only in that case to prevent accidental loss from "make clean".
CLEANFILES =

# Avoid deleting in-tree test scripts when "make clean" is invoked.
# Only remove copied test files in an out-of-tree build so that the
# distributed sources remain intact for developers and package builds.
clean-local:
	@if test "$(abs_builddir)" != "$(abs_srcdir)"; then \\
		rm -f $(LOG_COMPILER) $(TESTS) $(HELPER_FILES); \\
	fi

check-local: $(LOG_COMPILER) $(TESTS) $(HELPER_FILES)

$(LOG_COMPILER): $(srcdir)/tools/with-timeout.sh
	$(MKDIR_P) $(builddir)/tools
	$(AM_V_GEN)cp $(srcdir)/tools/with-timeout.sh $(LOG_COMPILER)

$(TESTS) $(HELPER_FILES):
	$(MKDIR_P) $(builddir)/$(dir $@)
	$(AM_V_GEN)cp $(srcdir)/$@ $(builddir)/$@
