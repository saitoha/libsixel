#
# @configure_input@
#
SUBDIRS = include src . converters assessment tools python examples images
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
#
# The configure-time detection cannot see future command-line flags, so we must
# re-check job flags at make runtime. If the caller provided -j/--jobs
# (via MFLAGS/MAKEFLAGS), keep MAKEFLAGS unchanged so that setting wins
# over LS_DEFAULT_MAKEFLAGS.
LS_USER_JOBS := $(filter -j% --jobs --jobs=%,$(MFLAGS) $(MAKEFLAGS))
MAKEFLAGS := $(if $(filter 0,$(MAKELEVEL)),$(if $(LS_USER_JOBS),$(MAKEFLAGS),@LS_DEFAULT_MAKEFLAGS@ $(MAKEFLAGS)),$(MAKEFLAGS))


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

# Build a selected group of subdirectories in parallel after src has completed.
#
# Execution flow for recursive all/check with SUBDIRS containing '.':
#   1) include -> src -> .
#   2) At '.', run phase2-all from all-am (this Makefile).
#   3) Remaining subdirs are still visited for compatibility, but most become no-op
#      because their all targets are already up-to-date.
#
# For "--enable-amalgamated-tools --disable-amalgamated-lib", run one-time
# amalgamation preparation before launching parallel phase2 members. This avoids
# races where multiple directories try to generate amalgamation/sixel.c.
.PHONY: \
	prepare-amalgamation-all \
	prepare-amalgamation-check \
	phase1-all \
	phase2-all \
	phase2-members-all \
	all-converters \
	all-tools \
	all-assessment \
	all-gdk-pixbuf-loader \
	all-quicklook-extension \
	all-wic \
	all-ruby \
	all-python

PHASE2_ALL_TARGETS = \
	all-converters \
	all-tools \
	all-assessment \
	all-python
if ENABLE_GDK_PIXBUF_LOADER
PHASE2_ALL_TARGETS += all-gdk-pixbuf-loader
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
PHASE2_ALL_TARGETS += all-quicklook-extension
endif
if HAVE_WICCODEC
PHASE2_ALL_TARGETS += all-wic
endif
if HAVE_RUBY
PHASE2_ALL_TARGETS += all-ruby
endif

prepare-amalgamation-all:
if WANT_AMALGAMATED_TOOLS_ONLY
	$(MAKE) -C converters $(AM_MAKEFLAGS) completion_embed.h
	$(MAKE) -C amalgamation $(AM_MAKEFLAGS) sixel.c
endif

prepare-amalgamation-check: prepare-amalgamation-all

phase1-all: prepare-amalgamation-all

phase2-members-all: $(PHASE2_ALL_TARGETS)

phase2-all: phase1-all
	$(MAKE) $(AM_MAKEFLAGS) phase2-members-all

all-converters:
	$(MAKE) -C converters $(AM_MAKEFLAGS) all

all-tools:
	$(MAKE) -C tools $(AM_MAKEFLAGS) all

all-assessment:
	$(MAKE) -C assessment $(AM_MAKEFLAGS) all

all-gdk-pixbuf-loader:
	$(MAKE) -C gdk-pixbuf-loader $(AM_MAKEFLAGS) all

all-quicklook-extension:
	$(MAKE) -C quicklook-extension $(AM_MAKEFLAGS) all

all-wic:
	$(MAKE) -C wic $(AM_MAKEFLAGS) all

all-ruby:
	$(MAKE) -C ruby $(AM_MAKEFLAGS) all

all-python:
	$(MAKE) -C python $(AM_MAKEFLAGS) all

all-am: phase2-all
	cp $(builddir)/package.json.in $(builddir)/package.json

check-am: prepare-amalgamation-check
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
