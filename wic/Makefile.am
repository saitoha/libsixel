
srcdir = @srcdir@
top_srcdir = @top_srcdir@
VPATH = @srcdir@
prefix = @prefix@
exec_prefix = @exec_prefix@
top_builddir = @top_builddir@
builddir = @builddir@

CLEANFILES = *.o *.obj $(wic_installer) wic-msi.log wic-msi.trs \
    wic-installed.log wic-installed.trs test-suite.log installcheck-suite.log

if USING_CL
# Automake appends CFLAGS after target flags; remove dynamic CRT switches so
# the codec's /MT setting cannot be overridden by the ordinary library build.
CFLAGS = $(filter-out /MD /MDd -MD -MDd,@CFLAGS@)
endif

# Only the installer owns the deployed DLL and COM registration.
noinst_LTLIBRARIES = libwicsixel.la
libwicsixel_la_SOURCES = $(srcdir)/wic.c \
                         $(srcdir)/wic_stub.c \
                         $(srcdir)/wic_stub.h \
                         $(top_srcdir)/src/fromsixel.c \
                         $(top_srcdir)/src/colorspace-hls.c \
                         $(top_srcdir)/src/cpu.c \
                         $(top_srcdir)/src/allocator.c \
                         $(top_srcdir)/src/timer.c \
                         $(top_srcdir)/src/timeline-logger.c \
                         $(top_srcdir)/src/timeline-writer.c \
                         $(top_srcdir)/src/status.c \
                         $(top_srcdir)/src/probe.c \
                         $(top_srcdir)/src/compat_stub.c \
                         $(top_srcdir)/src/path.c \
                         $(top_srcdir)/src/threading.c \
                         $(top_srcdir)/src/options-registry.c

if SIXEL_ENABLE_THREADS
libwicsixel_la_SOURCES += \
    $(top_srcdir)/src/decoder-parallel.c \
    $(top_srcdir)/src/threading.h \
    $(top_srcdir)/src/threadpool.c \
    $(top_srcdir)/src/threadpool.h
endif

libwicsixel_la_CPPFLAGS = $(AM_CPPFLAGS) \
                          -I$(top_builddir)/include/ \
                          -I$(top_srcdir)/include/ \
                          -I$(top_builddir)/src/ \
                          -I$(top_srcdir)/src/ \
                          -DSIXEL_STATUS_HAVE_TTY_HELPERS=0 \
                          -DSIXEL_DECODE_PIXELS_NO_FAST4=1
libwicsixel_la_CFLAGS = $(CFLAGS) \
                        $(AM_CFLAGS) \
                        $(WICCODEC_CFLAGS)

# -rpath asks libtool to build a DLL even though it is not installed by libtool.
libwicsixel_la_LDFLAGS = -no-undefined -module -avoid-version \
                         -rpath $(pkglibdir) $(WICCODEC_LDFLAGS) \
                         -export-symbols $(srcdir)/wicsixel.def
libwicsixel_la_LIBADD = $(WICCODEC_LIBS)
libwicsixel_la_DEPENDENCIES = $(srcdir)/wicsixel.def

wic_installer = libsixel-wic.msi
wicinstallerdir = $(datadir)/libsixel/installers
wic_msi_tests = \
    0001_per_machine.ps1 \
    0002_embedded_cabinet_reference.ps1 \
    0003_embedded_cabinet_stream.ps1 \
    0004_payload_file_size.ps1 \
    0005_upgrade_transaction.ps1 \
    0006_downgrade_detection.ps1 \
    0007_same_version_replacement.ps1 \
    0008_com_registration_path.ps1 \
    0009_component_architecture.ps1 \
    0010_no_self_registration.ps1 \
    0011_no_custom_actions.ps1 \
    0012_destdir_skips_install.ps1 \
    0013_registration.ps1 \
    0014_dependencies.ps1
wic_installed_tests = \
    0001_product_installed.ps1 \
    0002_installed_payload.ps1 \
    0003_installed_registration.ps1 \
    0004_decode_red_pixel.ps1 \
    0005_factory_held.ps1 \
    0006_factory_released.ps1 \
    0007_automation_factory_held.ps1 \
    0008_automation_factory_released.ps1 \
    0009_decoder_held.ps1 \
    0010_decoder_released.ps1 \
    0011_automation_held.ps1 \
    0012_automation_released.ps1 \
    0013_frame_held.ps1 \
    0014_frame_released.ps1 \
    0015_thumbnail_held.ps1 \
    0016_thumbnail_released.ps1 \
    0017_transform_held.ps1 \
    0018_transform_released.ps1 \
    0019_decoder_reader_held.ps1 \
    0020_decoder_reader_released.ps1 \
    0021_frame_reader_held.ps1 \
    0022_frame_reader_released.ps1 \
    0023_writer_held.ps1 \
    0024_writer_released.ps1 \
    0025_enumerator_held.ps1 \
    0026_enumerator_released.ps1 \
    0027_clone_held.ps1 \
    0028_clone_released.ps1 \
    0029_factory_unsupported_iid.ps1 \
    0030_decoder_unsupported_iid.ps1 \
    0031_automation_unsupported_iid.ps1 \
    0032_factory_allocation_1_rollback.ps1 \
    0033_decoder_allocation_1_rollback.ps1 \
    0034_automation_allocation_1_rollback.ps1 \
    0035_frame_allocation_1_rollback.ps1 \
    0036_frame_allocation_2_rollback.ps1 \
    0037_frame_allocation_3_rollback.ps1 \
    0038_thumbnail_allocation_1_rollback.ps1 \
    0039_thumbnail_allocation_2_rollback.ps1 \
    0040_thumbnail_allocation_3_rollback.ps1 \
    0041_decoder_reader_allocation_1_rollback.ps1 \
    0042_enumerator_allocation_1_rollback.ps1 \
    0043_clone_allocation_1_rollback.ps1 \
    0044_server_lock_held.ps1 \
    0045_concurrent_objects_held.ps1 \
    0046_server_lock_released.ps1 \
    0047_concurrent_objects_released.ps1
EXTRA_DIST = installer.ps1 registry.tsv powershell.sh wicsixel.def \
    $(top_srcdir)/tests/extension/wic-installer/msi-query.ps1 \
    $(top_srcdir)/tests/extension/wic-installer/registration-capture.ps1 \
    $(top_srcdir)/tests/extension/wic-installer/run-tests.ps1 \
    $(addprefix $(top_srcdir)/tests/extension/wic-installer/,$(wic_msi_tests)) \
    $(addprefix $(top_srcdir)/tests/extension/wic-installed/,$(wic_installed_tests))

if HAVE_WIC_MSI
nodist_wicinstaller_DATA = $(wic_installer)

$(wic_installer): libwicsixel.la $(srcdir)/installer.ps1 \
                 $(srcdir)/registry.tsv $(top_srcdir)/LICENSE
	$(AM_V_GEN)$(SHELL) "$(srcdir)/powershell.sh" "$(WIC_POWERSHELL)" \
	    "$(srcdir)/installer.ps1" -Dll .libs/libwicsixel.dll \
	    -Version "$(PACKAGE_VERSION)" -License "$(top_srcdir)/LICENSE" \
	    -Output "$@"

# DESTDIR takes precedence even when staging on a Windows build machine.
install-data-hook:
	@if test -n "$(DESTDIR)"; then \
	    echo "WIC installer staged; installation skipped (DESTDIR)."; \
	else \
	    $(SHELL) "$(srcdir)/powershell.sh" "$(WIC_POWERSHELL)" \
	        "$(srcdir)/installer.ps1" -Action Install \
	        -Output "$(wicinstallerdir)/$(wic_installer)"; \
	fi

if COND_TESTS
check_PROGRAMS = wic-installed-decode wic-installed-lifetime
wic_installed_decode_SOURCES = \
    $(top_srcdir)/tests/extension/wic-installed/decode-red-pixel.c
wic_installed_decode_LDADD = $(WICCODEC_LIBS)
wic_installed_lifetime_SOURCES = \
    $(top_srcdir)/tests/extension/wic-installed/lifetime-probe.c
wic_installed_lifetime_LDADD = $(WICCODEC_LIBS)

# Keep the installation lifecycle ordered even when the caller uses make -j.
# This explicit target installs the codec; ordinary check never installs it.
.PHONY: check-installation
check-installation:
	@test -z "$(DESTDIR)$${SIXEL_INSTALLCHECK_DESTDIR-}" || { \
	    echo "check-installation requires a system install, not DESTDIR." >&2; \
	    exit 1; \
	}
	$(MAKE) $(AM_MAKEFLAGS) check
	$(MAKE) $(AM_MAKEFLAGS) install
	$(MAKE) $(AM_MAKEFLAGS) installcheck

# Test files stay atomic; one TAP runner shares Windows/PowerShell startup.
WIC_MSI_TEST ?= *
check-local: $(wic_installer)
	@status=0; \
	TOP_SRCDIR="$(abs_top_srcdir)" TOP_BUILDDIR="$(abs_top_builddir)" \
	    LSO_TAP_TIMER_BIN="$(abs_top_builddir)/tools/lso-timer" \
	    $(SHELL) "$(top_builddir)/build-aux/lso-tap-driver.sh" \
	    --test-name wic-msi --log-file wic-msi.log --trs-file wic-msi.trs \
	    --enable-hard-errors yes -- $(SHELL) "$(srcdir)/powershell.sh" \
	    "$(WIC_POWERSHELL)" \
	    "$(top_srcdir)/tests/extension/wic-installer/run-tests.ps1" \
	    -Dll .libs/libwicsixel.dll -Package "$(wic_installer)" \
	    -Test "$(WIC_MSI_TEST)" || status=$$?; \
	printf '%s\n' wic-msi | $(AWK) \
	    -v quiet="$(SIXEL_TEST_TIME_DEFER)" \
	    -v allowlist="$(top_srcdir)/tests/test-time-allowlist.tsv" \
	    -f "$(top_srcdir)/build-aux/test-suite-report.awk" > test-suite.log || exit $$?; \
	exit $$status

# Installation is an explicit prerequisite supplied by the caller, never an
# action of the test. Query the exact MSI and discover its DLL through WIC.
installcheck-local: $(wic_installer) wic-installed-decode$(EXEEXT) \
                    wic-installed-lifetime$(EXEEXT)
	@status=0; \
	TOP_SRCDIR="$(abs_top_srcdir)" TOP_BUILDDIR="$(abs_top_builddir)" \
	    LSO_TAP_TIMER_BIN="$(abs_top_builddir)/tools/lso-timer" \
	    $(SHELL) "$(top_builddir)/build-aux/lso-tap-driver.sh" \
	    --test-name wic-installed --log-file wic-installed.log \
	    --trs-file wic-installed.trs --enable-hard-errors yes -- \
	    $(SHELL) "$(srcdir)/powershell.sh" "$(WIC_POWERSHELL)" \
	    "$(top_srcdir)/tests/extension/wic-installer/run-tests.ps1" \
	    -Mode Installed -Dll .libs/libwicsixel.dll \
	    -Package "$(wic_installer)" -Probe ./wic-installed-decode$(EXEEXT) \
	    -LifetimeProbe ./wic-installed-lifetime$(EXEEXT) \
	    -Test "$(WIC_MSI_TEST)" || status=$$?; \
	printf '%s\n' wic-installed | $(AWK) \
	    -v allowlist="$(top_srcdir)/tests/test-time-allowlist.tsv" \
	    -f "$(top_srcdir)/build-aux/test-suite-report.awk" \
	    > installcheck-suite.log || exit $$?; \
	exit $$status
endif
else
# A Unix cross build stages a packaging kit, not a system-installed codec.
wicpackagedir = $(wicinstallerdir)/wic-package
nodist_wicpackage_DATA = .libs/libwicsixel.dll
wicpackage_DATA = installer.ps1 registry.tsv $(top_srcdir)/LICENSE
.libs/libwicsixel.dll: libwicsixel.la
	@test -f "$@"
install-data-hook:
	@echo "WIC packaging kit copied; run installer.ps1 on Windows to generate the MSI."
endif

include $(top_srcdir)/build-aux/dependency-bootstrap.am
