
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_wheel.egg-info build dist libsixel/_libs

if HAVE_PYTHON
libsixeldir = $(pythondir)/libsixel
libsixel_PYTHON = \
    libsixel/__init__.py \
    libsixel/_libs/__init__.py \
    libsixel/encoder.py \
    libsixel/decoder.py
endif

if HAVE_PYTHON_WHEEL
wheel-local:
	$(AM_V_GEN) $(PYTHON) $(srcdir)/wheel_builder.py \
		--libdir "$(top_builddir)/src/.libs" \
		--distdir "$(abs_top_builddir)/python/dist"

all-local: wheel-local
endif

EXTRA_DIST = \
    setup.py \
    wheel_builder.py \
    libsixel/_wheel_ext.c

check: check-am
