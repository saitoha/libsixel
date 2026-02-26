# Ruby bindings: build helpers, tests, install/uninstall

RUBY_GEM_STAMP = $(abs_top_builddir)/ruby/dist/.gem-stamp

$(RUBY_GEM_STAMP): $(srcdir)/gem_builder.rb $(srcdir)/libsixel-ruby.gemspec
	$(AM_V_at) $(MAKE) -C "$(top_builddir)/src" all
	$(AM_V_at) mkdir -p "$(abs_top_builddir)/ruby/dist"
	$(AM_V_GEN) $(RUBY) $(srcdir)/gem_builder.rb \
		--libdir "$(top_builddir)/src/.libs" \
		--distdir "$(abs_top_builddir)/ruby/dist"
	$(AM_V_at) touch "$@"

# Keep this recipe tab-indented; GNU make requires tabs for commands.
all-local:
	@if test -x "$(RUBY)"; then \
	  echo "Ensuring Ruby constants are generated..."; \
	  $(MKDIR_P) $(builddir)/lib/libsixel 2>/dev/null || true; \
	  $(RUBY) $(top_srcdir)/tools/gen_ruby_constants.rb $(top_builddir)/include/sixel.h $(builddir)/lib/libsixel/constants.rb; \
	fi
	@if test -x "$(RUBY)"; then \
	  echo "Building bundled Ruby platform gem..."; \
	  $(MAKE) $(AM_MAKEFLAGS) "$(RUBY_GEM_STAMP)"; \
	fi

# Keep this recipe tab-indented; GNU make requires tabs for commands.
check-local:
	@if test -x "$(RUBY)"; then \
	  echo "Running Ruby tests..."; \
	  LD_LIBRARY_PATH=$(top_builddir)/src/.libs:$$LD_LIBRARY_PATH \
	  DYLD_LIBRARY_PATH=$(top_builddir)/src/.libs:$$DYLD_LIBRARY_PATH \
	  $(RUBY) -I $(srcdir)/lib -I $(srcdir)/test $(srcdir)/test/test_libsixel.rb || exit 1; \
	else \
	  echo "Ruby not found; skipping Ruby tests"; \
	fi

# Keep this recipe tab-indented; GNU make requires tabs for commands.
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

# Keep this recipe tab-indented; GNU make requires tabs for commands.
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

clean-local:
	rm -rf dist
	rm -f $(RUBY_GEM_STAMP)

EXTRA_DIST = gem_builder.rb
