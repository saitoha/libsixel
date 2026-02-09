
CLEANFILES=libsixel/*.pyc

clean-local:
	rm -rf libsixel_wheel.egg-info build dist
	rm -f $(abs_top_builddir)/python/dist/.wheel-stamp
	if test -d "$(builddir)/libsixel/_libs"; then \
		find "$(builddir)/libsixel/_libs" -type f \
			! -name '__init__.py' -exec rm -f {} +; \
	fi

if HAVE_PYTHON

# Keep wheel generation incremental by using an explicit stamp target.
# This avoids rebuilding the wheel when make is re-run without changes,
# and it also prevents duplicate work when recursive make visits python/
# more than once during the same top-level invocation.
WHEEL_STAMP = $(abs_top_builddir)/python/dist/.wheel-stamp

$(WHEEL_STAMP): $(srcdir)/wheel_builder.py $(srcdir)/setup.py \
		$(srcdir)/libsixel/_wheel_ext.c
	$(AM_V_at) $(MAKE) -C "$(top_builddir)/src" all
	$(AM_V_at) mkdir -p "$(abs_top_builddir)/python/dist"
	$(AM_V_GEN) $(PYTHON) $(srcdir)/wheel_builder.py \
		--libdir "$(top_builddir)/src/.libs" \
		--includedir "$(top_builddir)/include" \
		--distdir "$(abs_top_builddir)/python/dist"
	$(AM_V_at) touch "$@"

wheel-local: $(WHEEL_STAMP)

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
