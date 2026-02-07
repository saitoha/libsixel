#
# @configure_input@
#
SUBDIRS = include src converters assessment tools python examples images
if WANT_AMALGAMATED_SOURCES
SUBDIRS += amalgamation
endif
if ENABLE_GDK_PIXBUF_LOADER
SUBDIRS += gdk-pixbuf-loader
endif
if HAVE_RUBY
SUBDIRS += ruby
endif
if HAVE_WICCODEC
SUBDIRS += wic
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
SUBDIRS += quicklook-extension
endif
SUBDIRS += tests

DIST_SUBDIRS = \
        include \
        src \
        converters \
        assessment \
        tools \
        python \
        examples \
        images \
        ruby \
        wic \
        gdk-pixbuf-loader \
        quicklook-extension \
        amalgamation \
        tests

# Keep depcomp in the aux directory so "make dist" can stage it correctly.
EXTRA_DIST = LICENSE build-aux/depcomp \
	m4/ax_count_cpus.m4 \
	package.json.in.in \
	build-aux/lso-tap-driver.sh.in

# Honor user-supplied MAKEFLAGS while providing a CPU-based default -j.
# Only set this at the top-level to avoid forcing -j in sub-makes.
MAKEFLAGS := $(if $(filter 0,$(MAKELEVEL)),@LS_DEFAULT_MAKEFLAGS@ $(MAKEFLAGS),$(MAKEFLAGS))


.PHONY: install-quicklook-extension uninstall-quicklook-extension

install-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension install-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

uninstall-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension uninstall-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

srcdir = @srcdir@
VPATH = @srcdir@

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = $(builddir)/libsixel.pc

CLEANFILES = $(builddir)/package.json

all-am:
	cp $(builddir)/package.json.in $(builddir)/package.json

check-am:
	@:

install-data-local:
	@:

uninstall-local:
	@:

register:
	-regsvr32 -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Registered: .libs/libwicsixel.dll

unregister:
	-regsvr32 -u -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Unregistered: .libs/libwicsixel.dll

dist-windows: dist-zip
	mkdir -p @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows
	cp converters/*.exe @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/
	zip -u @PACKAGE_NAME@-@PACKAGE_VERSION@.zip @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/*.exe
	@host_triplet=''; \
	for confopt in $$(./config.status --config); do \
	  case "$$confopt" in \
	    --host=*) host_triplet=$${confopt#--host=} ;; \
	  esac; \
	done; \
	mv -v @PACKAGE_NAME@-@PACKAGE_VERSION@.zip \
	  @PACKAGE_NAME@-@PACKAGE_VERSION@-$$host_triplet.zip
