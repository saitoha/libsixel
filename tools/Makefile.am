if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

bin_SCRIPTS = libsixel-config

if WANT_THUMBNAILER
bin_SCRIPTS += sixel-thumbnailer
endif

noinst_PROGRAMS = lso-timer
EXTRA_PROGRAMS =
if BUILD_LSO_TIMEOUT
noinst_PROGRAMS += lso-timeout
endif

EXTRA_DIST = libsixel-config.in \
             quicklook-extension.bash \
             sixel-thumbnailer \
             update-mime-database.sh \
             update-gdk-pixbuf-cache.sh \
             libsixel-thumbnailer.thumbnailer \
             sixel.xml \
             gen_completion_embed.awk \
             gen-amalgamation.sh \
             timer.c \
             lso-timeout.c \
             jpegref.c

if HAVE_JPEG
EXTRA_PROGRAMS += jpegref
jpegref_SOURCES = $(srcdir)/jpegref.c
jpegref_CPPFLAGS = $(AM_CPPFLAGS) $(LIBJPEG_CFLAGS)
jpegref_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
jpegref_LDFLAGS = $(MAYBE_COVERAGE)
jpegref_LDADD = $(LIBJPEG_LIBS)
endif

lso_timer_SOURCES = timer.c
lso_timer_LDADD = $(CLOCK_GETTIME_LIBS)
lso_timer_CFLAGS = $(AM_CFLAGS)
if BUILD_LSO_TIMEOUT
lso_timeout_SOURCES = lso-timeout.c
lso_timeout_LDADD = $(CLOCK_GETTIME_LIBS)
lso_timeout_CFLAGS = $(AM_CFLAGS)
endif

# Keep this recipe tab-indented; GNU make requires tabs for commands.
install-data-hook:
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/thumbnailers"; do \
	@WANT_THUMBNAILER_TRUE@      $(MKDIR_P) "$$dest" && \
	@WANT_THUMBNAILER_TRUE@      $(INSTALL_DATA) "$(srcdir)/libsixel-thumbnailer.thumbnailer" "$$dest/"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/mime/packages"; do \
	@WANT_THUMBNAILER_TRUE@      $(MKDIR_P) "$$dest" && \
	@WANT_THUMBNAILER_TRUE@      $(INSTALL_DATA) "$(srcdir)/sixel.xml" "$$dest/"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @if test -n "$(UPDATE_MIME_DATABASE)"; then \
	@WANT_THUMBNAILER_TRUE@      $(SHELL) "$(top_srcdir)/tools/update-mime-database.sh" "$(UPDATE_MIME_DATABASE)" "$(datadir)" install; \
	@WANT_THUMBNAILER_TRUE@    fi
	@WANT_THUMBNAILER_FALSE@    @:

# Keep this recipe tab-indented; GNU make requires tabs for commands.
uninstall-hook:
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/thumbnailers"; do \
	@WANT_THUMBNAILER_TRUE@      rm -f "$$dest/libsixel-thumbnailer.thumbnailer"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @for dest in "$(DESTDIR)$(datadir)/mime/packages"; do \
	@WANT_THUMBNAILER_TRUE@      rm -f "$$dest/sixel.xml"; \
	@WANT_THUMBNAILER_TRUE@    done
	@WANT_THUMBNAILER_TRUE@    @if test -n "$(UPDATE_MIME_DATABASE)"; then \
	@WANT_THUMBNAILER_TRUE@      $(SHELL) "$(top_srcdir)/tools/update-mime-database.sh" "$(UPDATE_MIME_DATABASE)" "$(datadir)" uninstall; \
	@WANT_THUMBNAILER_TRUE@    fi
	@WANT_THUMBNAILER_FALSE@    @:
