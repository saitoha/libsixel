# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 libsixel developers. See `AUTHORS`.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
MODULE_CACHE_DIR = $(builddir)/module-cache
# Keep sanitizer flags consistent between the C static archive and the
# Objective-C Quick Look binaries.  Some CI profiles inject sanitizers via
# CFLAGS only, which otherwise leaves the Objective-C link step without the
# required ASan/UBSan runtime libraries.
SIXEL_OBJC_SANITIZER_FLAGS = \
    $(filter -fsanitize=% -fno-sanitize=%,$(CFLAGS) $(LDFLAGS))
AM_OBJCFLAGS = @APPLE_QUICKLOOK_OBJCFLAGS@ \
    -fmodules-cache-path=$(MODULE_CACHE_DIR) \
    $(SIXEL_OBJC_SANITIZER_FLAGS)
AM_LDFLAGS = $(SIXEL_OBJC_SANITIZER_FLAGS)
AM_CPPFLAGS = -DHAVE_CONFIG_H -DSIXEL_DISABLE_EXT_LIBS=1 \
    -I$(top_builddir) \
    -I$(top_builddir)/include \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src \
    -I$(top_builddir)/src \
    -I$(srcdir)/Sources

EXTRA_DIST = \
    Resources/Host-Info.plist.in \
    Resources/Preview-Info.plist.in \
    Resources/Thumbnail-Info.plist.in \
    Resources/Bridge-Info.plist.in \
    Resources/QuickLookExtension.entitlements \
    assemble-bundles.sh \
    check-package-tools.sh \
    package-preinstall \
    package-postinstall \
    meson.build \
    products/meson.build \
    package/meson.build

noinst_LIBRARIES = libsixelquicklookcore.a

# +----------------------------------------------+
# |  Core static archive                         |
# +----------------------------------------------+
# The Quick Look helpers rely on the same compat
# layer as the main library.  By keeping the
# archive self-contained we avoid undefined
# references when the Objective-C targets link it.
# Include CPU detection for the decoder SIMD runtime policy.
#
libsixelquicklookcore_a_SOURCES = \
    Sources/SixelQuickLookStub.c \
    $(top_srcdir)/src/status.c \
    $(top_srcdir)/src/allocator.c \
    $(top_srcdir)/src/fromsixel.c \
    $(top_srcdir)/src/colorspace-hls.c \
    $(top_srcdir)/src/cpu.c \
    $(top_srcdir)/src/timer.c \
    $(top_srcdir)/src/timeline-logger.c \
    $(top_srcdir)/src/timeline-writer.c \
    $(top_srcdir)/src/compat_stub.c \
    $(top_srcdir)/src/options-registry.c \
    $(top_srcdir)/src/threading.c \
    $(top_srcdir)/src/threading.h \
    $(top_srcdir)/src/path.c

if SIXEL_ENABLE_THREADS
libsixelquicklookcore_a_SOURCES += \
    $(top_srcdir)/src/decoder-parallel.c \
    $(top_srcdir)/src/threadpool.c \
    $(top_srcdir)/src/threadpool.h
endif

libsixelquicklookcore_a_CPPFLAGS = $(AM_CPPFLAGS) \
    -DSIXEL_STATUS_HAVE_TTY_HELPERS=0 \
    -DSIXEL_DECODE_PIXELS_NO_FAST4=1

if HAVE_APPLE_QUICKLOOK_EXTENSION

noinst_PROGRAMS = \
    SixelPreview \
    SixelThumbnail \
    SixelQuickLookHost \
    SixelPreviewBridge \
    register_sixel_preview \
    sixel-quicklook-package

install_quicklook_script = $(top_srcdir)/tools/quicklook-extension.bash

SixelPreview_SOURCES = \
    Sources/SixelPreviewMain.m \
    Sources/SixelPreviewProvider.h \
    Sources/SixelPreviewProvider.m \
    Sources/SixelQuickLookShared.m
SixelPreview_CPPFLAGS = $(AM_CPPFLAGS)
SixelPreview_OBJCFLAGS = $(AM_OBJCFLAGS) -fapplication-extension
SixelPreview_LDADD = libsixelquicklookcore.a @APPLE_QUICKLOOK_EXTENSION_LIBS@

SixelThumbnail_SOURCES = \
    Sources/SixelThumbnailMain.m \
    Sources/SixelThumbnailProvider.h \
    Sources/SixelThumbnailProvider.m \
    Sources/SixelQuickLookShared.m
SixelThumbnail_CPPFLAGS = $(AM_CPPFLAGS)

SixelThumbnail_OBJCFLAGS = $(AM_OBJCFLAGS) -fapplication-extension
SixelThumbnail_LDADD = libsixelquicklookcore.a @APPLE_QUICKLOOK_EXTENSION_LIBS@

SixelQuickLookHost_SOURCES = Sources/SixelQuickLookHost.m
SixelQuickLookHost_CPPFLAGS = $(AM_CPPFLAGS)
SixelQuickLookHost_LDFLAGS = -framework Foundation
SixelQuickLookHost_OBJCFLAGS = $(AM_OBJCFLAGS)

SixelPreviewBridge_SOURCES = \
    Sources/SixelPreviewBridge.m \
    Sources/SixelQuickLookShared.h \
    Sources/SixelQuickLookShared.m

SixelPreviewBridge_CPPFLAGS = $(AM_CPPFLAGS)
SixelPreviewBridge_OBJCFLAGS = $(AM_OBJCFLAGS)
SixelPreviewBridge_LDADD = libsixelquicklookcore.a @APPLE_QUICKLOOK_EXTENSION_LIBS@

register_sixel_preview_SOURCES = $(top_srcdir)/tools/register_sixel_preview.m
register_sixel_preview_CPPFLAGS = $(AM_CPPFLAGS)
register_sixel_preview_OBJCFLAGS = $(AM_OBJCFLAGS)
register_sixel_preview_LDADD = @APPLE_QUICKLOOK_REGISTER_LIBS@

sixel_quicklook_package_SOURCES = Sources/SixelPackageTool.m Sources/SixelInstall.h \
    Sources/SixelPackageChecks.h
sixel_quicklook_package_OBJCFLAGS = $(AM_OBJCFLAGS)
sixel_quicklook_package_LDFLAGS = $(AM_LDFLAGS) -framework Foundation -framework Security

PRODUCTS_DIR = $(builddir)/products
HOST_APP = $(PRODUCTS_DIR)/SixelQuickLookHost.app
PREVIEW_APPEX = $(HOST_APP)/Contents/PlugIns/SixelPreview.appex
THUMB_APPEX = $(HOST_APP)/Contents/PlugIns/SixelThumbnail.appex
BRIDGE_APP = $(PRODUCTS_DIR)/SixelPreviewBridge.app

plist_templates = Host-Info.plist Preview-Info.plist Thumbnail-Info.plist Bridge-Info.plist
GENERATED_RESOURCES_DIR = $(builddir)/generated-resources
plist_outputs = $(addprefix $(GENERATED_RESOURCES_DIR)/,$(plist_templates))

# Support direct subdirectory builds after a template edit, not only the
# top-level config.status rules generated by Automake.
$(GENERATED_RESOURCES_DIR)/%-Info.plist: $(srcdir)/Resources/%-Info.plist.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status \
	    quicklook-extension/generated-resources/$*-Info.plist

# All binary transformations must finish before assembly. These are native
# Mach-O bundles: a script prefix is not a supported executable format.
bundle-stamp: $(plist_outputs) SixelPreview SixelThumbnail SixelQuickLookHost SixelPreviewBridge $(srcdir)/assemble-bundles.sh
	$(SHELL) $(srcdir)/assemble-bundles.sh "$(HOST_APP)" "$(BRIDGE_APP)" \
	    SixelQuickLookHost SixelPreview SixelThumbnail SixelPreviewBridge \
	    $(GENERATED_RESOURCES_DIR)/Host-Info.plist \
	    $(GENERATED_RESOURCES_DIR)/Preview-Info.plist \
	    $(GENERATED_RESOURCES_DIR)/Thumbnail-Info.plist \
	    $(GENERATED_RESOURCES_DIR)/Bridge-Info.plist
	@touch $@

# Package a separate copy so the assembly byte comparisons remain meaningful.
# The package is published last, after signing and archive verification.
package/SixelQuickLook.pkg: bundle-stamp register_sixel_preview sixel-quicklook-package $(srcdir)/package-preinstall $(srcdir)/package-postinstall $(srcdir)/Resources/QuickLookExtension.entitlements Makefile
	./sixel-quicklook-package build "$(abs_builddir)/package" \
	    "$(abs_builddir)/$(HOST_APP)" "$(abs_builddir)/$(BRIDGE_APP)" \
	    "$(abs_builddir)/register_sixel_preview" \
	    "$(abs_builddir)/sixel-quicklook-package" \
	    "$(abs_srcdir)/Resources/QuickLookExtension.entitlements" \
	    "$(PACKAGE_VERSION)" "$(QUICKLOOK_PACKAGE_VERSION)" \
	    "$(abs_srcdir)/package-preinstall" \
	    "$(abs_srcdir)/package-postinstall"

all-local: bundle-stamp package/SixelQuickLook.pkg

# Both standard and explicit entry points use the same finished package.
quicklook_manage = $(SHELL) "$(install_quicklook_script)"
quicklook_manage_args = --package-dir "$(abs_builddir)/package" \
    --tool "$(abs_builddir)/sixel-quicklook-package" \
    --datadir "$(datadir)" --destdir "$(DESTDIR)"

install-exec-local: install-quicklook-extension

.PHONY: install-quicklook-extension uninstall-quicklook-extension \
    installcheck-quicklook

install-quicklook-extension: package/SixelQuickLook.pkg
	$(quicklook_manage) install $(quicklook_manage_args)

# User deactivation is explicit. Set QUICKLOOK_UNINSTALL_FLAGS only after
# restoring every affected user's choices; root never edits their settings.
uninstall-quicklook-extension:
	$(quicklook_manage) uninstall $(quicklook_manage_args) $(QUICKLOOK_UNINSTALL_FLAGS)

uninstall-local: uninstall-quicklook-extension

installcheck-local: installcheck-quicklook

installcheck-quicklook:
	$(quicklook_manage) installcheck $(quicklook_manage_args)

clean-local:
	rm -rf $(HOST_APP) $(BRIDGE_APP) $(MODULE_CACHE_DIR) bundle-stamp $(builddir)/package/work
	rm -f $(builddir)/package/SixelQuickLook.pkg \
	    $(builddir)/package/payload-manifest.json \
	    $(builddir)/package/package-evidence.json

else

all-local:
install-exec-local:
uninstall-local:
clean-local:

endif

CLEANFILES = bundle-stamp

include $(top_srcdir)/build-aux/dependency-bootstrap.am
