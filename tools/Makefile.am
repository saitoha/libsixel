if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

if HAVE_ONNXRUNTIME
noinst_PROGRAMS = lpips
lpips_SOURCES = $(srcdir)/lpips.c
lpips_CPPFLAGS = -I$(top_builddir)/include/ \
                 -I$(top_srcdir)/src \
                 $(AM_CPPFLAGS) \
                 $(ONNXRUNTIME_CFLAGS)
lpips_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
lpips_LDFLAGS = $(MAYBE_COVERAGE)
lpips_LDADD = $(top_builddir)/src/libsixel.la \
              $(ONNXRUNTIME_LIBS)
endif

bin_SCRIPTS = libsixel-config

if WANT_THUMBNAILER
bin_SCRIPTS += sixel-thumbnailer
endif

EXTRA_DIST = libsixel-config.in \
             quicklook-extension.bash \
             sixel-thumbnailer \
             update-mime-database.sh \
             libsixel-thumbnailer.thumbnailer \
             sixel.xml

install-data-hook:
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/thumbnailers"; do \
	@WANT_THUMBNAILER_TRUE@      $(MKDIR_P) "$$dest" && \
	@WANT_THUMBNAILER_TRUE@      $(INSTALL_DATA) "$(srcdir)/libsixel-thumbnailer.thumbnailer" "$$dest/"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/packages"; do \
	@WANT_THUMBNAILER_TRUE@      $(MKDIR_P) "$$dest" && \
	@WANT_THUMBNAILER_TRUE@      $(INSTALL_DATA) "$(srcdir)/sixel.xml" "$$dest/"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @if test -n "$(UPDATE_MIME_DATABASE)"; then \
	@WANT_THUMBNAILER_TRUE@      if test -z "$(DESTDIR)"; then \
	@WANT_THUMBNAILER_TRUE@        $(SHELL) "$(top_srcdir)/tools/update-mime-database.sh" "$(UPDATE_MIME_DATABASE)" "$(datadir)" install; \
	@WANT_THUMBNAILER_TRUE@      fi; \
	@WANT_THUMBNAILER_TRUE@    fi
	@WANT_THUMBNAILER_FALSE@    @:

uninstall-hook:
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/thumbnailers"; do \
	@WANT_THUMBNAILER_TRUE@      rm -f "$$dest/libsixel-thumbnailer.thumbnailer"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/packages"; do \
	@WANT_THUMBNAILER_TRUE@      rm -f "$$dest/sixel.xml"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @if test -n "$(UPDATE_MIME_DATABASE)"; then \
	@WANT_THUMBNAILER_TRUE@      if test -z "$(DESTDIR)"; then \
	@WANT_THUMBNAILER_TRUE@        $(SHELL) "$(top_srcdir)/tools/update-mime-database.sh" "$(UPDATE_MIME_DATABASE)" "$(datadir)" uninstall; \
	@WANT_THUMBNAILER_TRUE@      fi; \
	@WANT_THUMBNAILER_TRUE@    fi
	@WANT_THUMBNAILER_FALSE@    @:
