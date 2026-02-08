
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_wheel.egg-info build dist
	if test -d "$(builddir)/libsixel/_libs"; then \
		find "$(builddir)/libsixel/_libs" -type f \
			! -name '__init__.py' -exec rm -f {} +; \
	fi

if HAVE_PYTHON
wheel-local:
	$(AM_V_at) $(MAKE) -C "$(top_builddir)/src" all
	$(AM_V_GEN) $(PYTHON) $(srcdir)/wheel_builder.py \
		--libdir "$(top_builddir)/src/.libs" \
		--distdir "$(abs_top_builddir)/python/dist"

all-local: wheel-local

install-exec-local:
	@echo "Python package install is not performed by make install."
	@echo "Run: python -m pip install python/dist/libsixel_wheel-*.whl"
endif

EXTRA_DIST = \
    setup.py \
    wheel_builder.py \
    libsixel/_wheel_ext.c

check: check-am
