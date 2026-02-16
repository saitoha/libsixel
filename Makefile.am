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
	build-aux/lso-tap-driver.sh.in \
	build-aux/read-check-test-list.sh \
	build-aux/resolve-config-h-exports.sh \
	build-aux/resolve-python-test-venv.sh \
	build-aux/resolve-test-tool-paths.sh.in \
	tools/fuzz-img2sixel-cli.sh

# Honor user-supplied make parallel flags while providing a CPU-based default.
#
# Do not rewrite MAKEFLAGS in the Makefile. GNU make computes runtime parallel
# behavior before variable expansion, so mutating MAKEFLAGS here can produce
# inconsistent results. Instead, append the default via AM_MAKEFLAGS only when
# no user-provided job control is visible in MFLAGS/MAKEFLAGS.
# Accept both explicit jobs options and GNU make jobserver tokens.
LS_USER_JOBS = $(filter -j% --jobs --jobs=% --jobserver-auth=% --jobserver-fds=% --jobserver-style=%,$(MFLAGS) $(MAKEFLAGS))
AM_MAKEFLAGS = $(if $(filter 0,$(MAKELEVEL)),$(if $(LS_USER_JOBS),,@LS_DEFAULT_MAKEFLAGS@),)


.PHONY: install-quicklook-extension uninstall-quicklook-extension \
	fuzz-cli-img2sixel

FUZZ_AFL ?= afl-fuzz
FUZZ_TIMEOUT ?= 0
FUZZ_INPUT_DIR ?= $(top_srcdir)/images
FUZZ_OUTPUT_DIR ?= $(abs_top_builddir)/afl-out/img2sixel
FUZZ_IMG2SIXEL ?= $(abs_top_builddir)/converters/img2sixel$(EXEEXT)
FUZZ_IMG2SIXEL_BASE_ARGS ?=

install-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension install-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

uninstall-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension uninstall-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

fuzz-cli-img2sixel: all
	@set -eu; \
	if ! command -v "$(FUZZ_AFL)" >/dev/null 2>&1; then \
		echo "error: $(FUZZ_AFL) is not installed" >&2; \
		exit 1; \
	fi; \
	if [ ! -x "$(FUZZ_IMG2SIXEL)" ]; then \
		echo "error: img2sixel binary was not found: $(FUZZ_IMG2SIXEL)" >&2; \
		exit 1; \
	fi; \
	if [ ! -r "$(top_srcdir)/tools/fuzz-img2sixel-cli.sh" ]; then \
		echo "error: fuzz wrapper is missing: tools/fuzz-img2sixel-cli.sh" >&2; \
		exit 1; \
	fi; \
	mkdir -p "$(FUZZ_OUTPUT_DIR)"; \
	echo "fuzz-cli-img2sixel: base args='$(FUZZ_IMG2SIXEL_BASE_ARGS)'"; \
	if [ "$(FUZZ_TIMEOUT)" -gt 0 ]; then \
		if ! command -v timeout >/dev/null 2>&1; then \
			echo "error: timeout command is required when FUZZ_TIMEOUT > 0" >&2; \
			exit 1; \
		fi; \
		echo "Running img2sixel fuzzing for $(FUZZ_TIMEOUT)s"; \
		set +e; \
		timeout "$(FUZZ_TIMEOUT)" "$(FUZZ_AFL)" \
			-i "$(FUZZ_INPUT_DIR)" \
			-o "$(FUZZ_OUTPUT_DIR)" \
			-- \
			env \
			FUZZ_IMG2SIXEL="$(FUZZ_IMG2SIXEL)" \
			FUZZ_IMG2SIXEL_BASE_ARGS="$(FUZZ_IMG2SIXEL_BASE_ARGS)" \
			"$(SHELL)" "$(top_srcdir)/tools/fuzz-img2sixel-cli.sh" @@; \
		rc=$$?; \
		set -e; \
		if [ $$rc -eq 124 ]; then \
			echo "fuzz-cli-img2sixel: reached FUZZ_TIMEOUT=$(FUZZ_TIMEOUT)s"; \
			exit 0; \
		fi; \
		exit $$rc; \
	fi; \
	echo "Running img2sixel fuzzing without timeout"; \
	"$(FUZZ_AFL)" \
		-i "$(FUZZ_INPUT_DIR)" \
		-o "$(FUZZ_OUTPUT_DIR)" \
		-- \
		env \
		FUZZ_IMG2SIXEL="$(FUZZ_IMG2SIXEL)" \
		FUZZ_IMG2SIXEL_BASE_ARGS="$(FUZZ_IMG2SIXEL_BASE_ARGS)" \
		"$(SHELL)" "$(top_srcdir)/tools/fuzz-img2sixel-cli.sh" @@

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
	all-python \
	staticcheck

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

staticcheck:
	$(MAKE) -C tests $(AM_MAKEFLAGS) staticcheck

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
