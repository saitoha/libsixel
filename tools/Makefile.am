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
	             gen_6cells_h.awk \
	             gen_factory_classid_gperf.awk \
	             gen_serviceid_gperf.awk \
	             gen-amalgamation.sh \
	             timer.c \
             lso-timeout.c \
             jpegref.c \
             ls-action-summarize \
             ls-summarize \
             check_dither_policy_measurements.py \
             check_dither_spectrum_measurements.py \
             check_makefile_generated_pairs.sh \
             check_lookup_policy_measurements.py \
             check_merge_policy_measurements.py \
             check_pixelformat_precision_measurements.py \
             check_precision_measurements.py \
             check_quantize_model_measurements.py \
             check_snap_policy_measurements.py \
             check_threading_measurements.py \
             check_working_colorspace_measurements.py \
             plot_cover_policy_figures.py \
             plot_dither_policy_measurements.py \
             plot_dither_spectrum_measurements.py \
             plot_lookup_policy_acceleration.py \
             plot_quality_curve.py \
             plot_lookup_policy_quality.sh \
             plot_lookup_policy_speed.py \
             plot_merge_policy_measurements.py \
             plot_pixelformat_precision_measurements.py \
             plot_precision_measurements.py \
             plot_quantize_model_measurements.py \
             plot_snap_policy_measurements.py \
             plot_threading_measurements.py \
             plot_working_colorspace_measurements.py \
             reproduce_lookup_policy_acceleration.sh \
             reproduce_cover_policy_figures.sh \
             reproduce_dither_policy_measurements.sh \
             reproduce_dither_spectrum_measurements.sh \
             reproduce_lookup_policy_measurements.sh \
             reproduce_merge_policy_measurements.sh \
             reproduce_pixelformat_precision_measurements.sh \
             reproduce_precision_measurements.sh \
             reproduce_quantize_model_measurements.sh \
             reproduce_snap_policy_measurements.sh \
             reproduce_threading_measurements.sh \
             reproduce_working_colorspace_measurements.sh \
             snap_policy_suite.json \
             timeline.py

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
# Automake emits the link rule unconditionally, so a _LINK defined only
# in one branch leaves the variable empty everywhere else and make then
# tries to run the first object file as a command.  Spell out the
# default for the other branch.
if OPENVMS_GNV
lso_timer_LINK = $(SHELL) $(top_srcdir)/openvms/gnv-link-program.sh -o $@
else
lso_timer_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=link $(CCLD) $(lso_timer_CFLAGS) \
	$(CFLAGS) $(lso_timer_LDFLAGS) $(LDFLAGS) -o $@
endif
if BUILD_LSO_TIMEOUT
lso_timeout_SOURCES = lso-timeout.c
lso_timeout_LDADD = $(CLOCK_GETTIME_LIBS)
lso_timeout_CFLAGS = $(AM_CFLAGS)
if OPENVMS_GNV
lso_timeout_LINK = $(SHELL) $(top_srcdir)/openvms/gnv-link-program.sh -o $@
else
lso_timeout_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=link $(CCLD) $(lso_timeout_CFLAGS) \
	$(CFLAGS) $(lso_timeout_LDFLAGS) $(LDFLAGS) -o $@
endif
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
