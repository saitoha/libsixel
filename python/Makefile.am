
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_python.egg-info build dist

if HAVE_PYTHON
libsixeldir = $(pythondir)/libsixel
libsixel_PYTHON = \
    libsixel/__init__.py \
    libsixel/encoder.py \
    libsixel/decoder.py
TESTS = $(srcdir)/test/run-python-tests.sh
TESTS_ENVIRONMENT = \
    TOP_BUILDDIR=$(top_builddir) \
    PYTHON=$(PYTHON)
TESTS_LOG_COMPILER = $(SHELL)
endif

EXTRA_DIST = \
    setup.py \
    test/run-python-tests.sh

check: check-am
