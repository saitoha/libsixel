
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_python.egg-info build dist

if HAVE_PYTHON
libsixeldir = $(pythondir)/libsixel
libsixel_PYTHON = \
    libsixel/__init__.py \
    libsixel/encoder.py \
    libsixel/decoder.py
endif

EXTRA_DIST = \
    setup.py

check: check-am
