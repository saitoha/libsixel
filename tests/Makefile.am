TESTS = t/test_0001_01_invalid_positional.t \
        t/test_0001_02_invalid_option_arguments.t \
        t/test_0001_03_invalid_option_combinations.t \
        t/test_0002_stdin.t \
        t/test_0003_information.t \
        t/test_0004_conversion_options_01.t \
        t/test_0004_conversion_options_02.t \
        t/test_0004_conversion_options_03.t \
        t/test_0004_conversion_options_04.t \
        t/test_0005_dcs_arguments.t \
        t/test_0006_dcs_variations.t \
        t/test_0007_animation.t \
        t/test_0008_progressive_jpeg.t \
        t/test_0009_pngsuite.t \
        t/test_0010_curl.t \
        t/test_0011_basic.t \
        t/test_0012_regressions.t \
        t/test_0013_documentation.t \
        t/test_0014_option_matching.t \
        t/test_0015_argument_shift.t \
        t/test_0016_assessment_spool.t \
        t/test_0017_clipboard.t \
        t/test_0018_lsqa_regression.t \
        t/test_0019_python_wheel.t
TEST_EXTENSIONS = .t

HELPER_FILES = t/converters-common.t

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
CLEANFILES = $(LOG_COMPILER) $(TESTS) $(HELPER_FILES)

check-local: $(LOG_COMPILER) $(TESTS) $(HELPER_FILES)

$(LOG_COMPILER): $(srcdir)/tools/with-timeout.sh
	$(MKDIR_P) $(builddir)/tools
	$(AM_V_GEN)cp $(srcdir)/tools/with-timeout.sh $(LOG_COMPILER)

$(TESTS):
	$(MKDIR_P) $(builddir)/t
	$(AM_V_GEN)cp $(srcdir)/$@ $(builddir)/$@

$(HELPER_FILES):
	$(MKDIR_P) $(builddir)/t
	$(AM_V_GEN)cp $(srcdir)/$@ $(builddir)/$@
