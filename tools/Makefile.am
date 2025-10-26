local_helpers = evaluate$(EXEEXT)

if HAVE_ONNXRUNTIME
local_helpers += lpips$(EXEEXT)

lpipsmodelsdir = $(pkgdatadir)/models/lpips
dist_lpipsmodels_DATA = $(top_srcdir)/models/lpips/lpips_diff.onnx \
                        $(top_srcdir)/models/lpips/lpips_feature.onnx
endif

CLEANFILES = $(local_helpers)

clean-local:

all-local: $(local_helpers)

# Build the fast evaluator locally so shell wrappers can reuse it without
# requiring installation.
evaluate$(EXEEXT): $(srcdir)/evaluate.c
	$(AM_V_CC)$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-I$(top_builddir) -I$(top_builddir)/include \
	-I$(top_srcdir)/src $(AM_CFLAGS) $(CFLAGS) \
	$(ONNXRUNTIME_CFLAGS) \
	-c -o $@.o $(srcdir)/evaluate.c
	$(AM_V_CC)$(CC) $(AM_CFLAGS) $(CFLAGS) \
	$@.o \
	$(ONNXRUNTIME_LIBS) \
	$(top_builddir)/src/.libs/libsixel.a \
	$(LIBPNG_LIBS) $(LIBJPEG_LIBS) $(GDK_PIXBUF_LIBS) \
	$(GD_LIBS) $(WIC_LIBS) $(COREGRAPHICS_LIBS) \
	$(QUICKLOOK_LIBS) \
	$(LIBCURL_LIBS) $(WINHTTP_LIBS) \
	$(LIBS_PRIVATE) $(LIBS) -o $@
	$(AM_V_at)rm -f $@.o


if HAVE_ONNXRUNTIME

# +-------------------+   +--------------------+   +--------------------+
# | libsixel loaders  |-->| tensor formatting |-->| ONNX Runtime graph |
# +-------------------+   +--------------------+   +--------------------+
lpips$(EXEEXT): $(srcdir)/lpips.c $(top_builddir)/src/liblibsixelloader.a
	$(AM_V_CC)$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) -I$(top_srcdir) \
	-I$(top_srcdir)/include -I$(top_srcdir)/src $(AM_CFLAGS) $(CFLAGS) \
	$(ONNXRUNTIME_CFLAGS) \
	-DSIXEL_LPIPS_MODEL_DIR=\"$(pkgdatadir)/models/lpips\" \
	$(srcdir)/lpips.c \
	$(LDFLAGS) -o $@ $(ONNXRUNTIME_LIBS) \
	$(top_builddir)/src/liblibsixelloader.a \
	$(LIBPNG_LIBS) $(LIBJPEG_LIBS) $(GDK_PIXBUF_LIBS) \
	$(GD_LIBS) $(WIC_LIBS) $(COREGRAPHICS_LIBS) \
	$(QUICKLOOK_LIBS) \
	$(LIBCURL_LIBS) $(WINHTTP_LIBS) \
	$(LIBS_PRIVATE) $(LIBS)

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
