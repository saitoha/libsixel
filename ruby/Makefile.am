# Ruby bindings: build helpers, tests, install/uninstall

all-local:
	@if test -x "$(RUBY)"; then \
	  echo "Ensuring Ruby constants are generated..."; \
	  $(MKDIR_P) $(builddir)/lib/libsixel 2>/dev/null || true; \
	  $(RUBY) $(top_srcdir)/tools/gen_ruby_constants.rb $(top_srcdir)/include/sixel.h $(builddir)/lib/libsixel/constants.rb || true; \
	fi

check-local:
	@if test -x "$(RUBY)"; then \
	  echo "Running Ruby tests..."; \
	  LD_LIBRARY_PATH=$(top_builddir)/src/.libs:$$LD_LIBRARY_PATH \
	  DYLD_LIBRARY_PATH=$(top_builddir)/src/.libs:$$DYLD_LIBRARY_PATH \
	  $(RUBY) -I $(srcdir)/lib -I $(srcdir)/test $(srcdir)/test/test_libsixel.rb || exit 1; \
	else \
	  echo "Ruby not found; skipping Ruby tests"; \
	fi

install-data-local:
	@if test "x$(RUBY_SITE_LIBDIR)" != x; then \
	  echo "Installing Ruby bindings to $(DESTDIR)$(RUBY_SITE_LIBDIR)"; \
	  $(MKDIR_P) $(DESTDIR)$(RUBY_SITE_LIBDIR); \
	  $(INSTALL_DATA) $(srcdir)/lib/libsixel.rb $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel.rb; \
	  $(MKDIR_P) $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel; \
	  for f in api.rb encoder.rb decoder.rb output.rb dither.rb helper.rb version.rb frame.rb; do \
	    $(INSTALL_DATA) $(srcdir)/lib/libsixel/$$f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/$$f; \
	  done; \
	  if test -f $(builddir)/lib/libsixel/version_runtime.rb; then \
	    $(INSTALL_DATA) $(builddir)/lib/libsixel/version_runtime.rb $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/version_runtime.rb; \
	  fi; \
	  if test -f $(builddir)/lib/libsixel/constants.rb; then \
	    $(INSTALL_DATA) $(builddir)/lib/libsixel/constants.rb $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/constants.rb; \
	  else \
	    if test -f $(srcdir)/lib/libsixel/constants.rb; then \
	      $(INSTALL_DATA) $(srcdir)/lib/libsixel/constants.rb $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/constants.rb; \
	    fi; \
	  fi; \
	else \
	  echo "Ruby site libdir not detected; skip installing Ruby bindings"; \
	fi

uninstall-local:
	@if test "x$(RUBY_SITE_LIBDIR)" != x; then \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/api.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/encoder.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/decoder.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/output.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/dither.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/helper.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/version.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/version_runtime.rb; \
	  rm -f $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel/constants.rb; \
	  rmdir $(DESTDIR)$(RUBY_SITE_LIBDIR)/libsixel 2>/dev/null || true; \
	fi

