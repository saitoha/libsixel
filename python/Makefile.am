
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_python.egg-info build dist

if HAVE_PYTHON
libsixeldir = $(pythondir)/libsixel
libsixel_PYTHON = \
    libsixel/__init__.py \
    libsixel/encoder.py \
    libsixel/decoder.py

check-local:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) $(srcdir)/tests/issue222_encode_bytes_packed.py
endif

EXTRA_DIST = \
    setup.py \
    tests/issue222_encode_bytes_packed.py

check: check-am
