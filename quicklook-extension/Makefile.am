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
AM_OBJCFLAGS = @APPLE_QUICKLOOK_OBJCFLAGS@ -fmodules-cache-path=$(MODULE_CACHE_DIR)
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
    Resources/QuickLookExtension.entitlements

noinst_LIBRARIES = libsixelquicklookcore.a

# +----------------------------------------------+
# |  Core static archive                         |
# +----------------------------------------------+
# The Quick Look helpers rely on the same compat
# layer as the main library.  By keeping the
# archive self-contained we avoid undefined
# references when the Objective-C targets link it.
#
libsixelquicklookcore_a_SOURCES = \
    $(top_srcdir)/src/status.c \
    $(top_srcdir)/src/allocator.c \
    $(top_srcdir)/src/fromsixel.c \
    $(top_srcdir)/src/compat_stub.c
libsixelquicklookcore_a_CPPFLAGS = $(AM_CPPFLAGS)

if HAVE_APPLE_QUICKLOOK_EXTENSION

noinst_PROGRAMS = \
    SixelPreview \
    SixelThumbnail \
    SixelQuickLookHost \
    SixelPreviewBridge \
    register_sixel_preview

install_quicklook_script = $(top_srcdir)/tools/quicklook-extension.bash

SixelPreview_SOURCES = \
    Sources/SixelPreviewMain.m \
    Sources/SixelPreviewProvider.m \
    Sources/SixelQuickLookShared.m
SixelPreview_CPPFLAGS = $(AM_CPPFLAGS)
SixelPreview_OBJCFLAGS = $(AM_OBJCFLAGS) -fapplication-extension
SixelPreview_LDADD = libsixelquicklookcore.a @APPLE_QUICKLOOK_EXTENSION_LIBS@

SixelThumbnail_SOURCES = \
    Sources/SixelThumbnailMain.m \
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
    Sources/SixelQuickLookShared.m
SixelPreviewBridge_CPPFLAGS = $(AM_CPPFLAGS)
SixelPreviewBridge_OBJCFLAGS = $(AM_OBJCFLAGS)
SixelPreviewBridge_LDADD = libsixelquicklookcore.a @APPLE_QUICKLOOK_EXTENSION_LIBS@

register_sixel_preview_SOURCES = $(top_srcdir)/tools/register_sixel_preview.m
register_sixel_preview_CPPFLAGS = $(AM_CPPFLAGS)
register_sixel_preview_OBJCFLAGS = $(AM_OBJCFLAGS)
register_sixel_preview_LDADD = @APPLE_QUICKLOOK_REGISTER_LIBS@

PRODUCTS_DIR = $(builddir)/products
HOST_APP = $(PRODUCTS_DIR)/SixelQuickLookHost.app
PREVIEW_APPEX = $(HOST_APP)/Contents/PlugIns/SixelPreview.appex
THUMB_APPEX = $(HOST_APP)/Contents/PlugIns/SixelThumbnail.appex
BRIDGE_APP = $(PRODUCTS_DIR)/SixelPreviewBridge.app

plist_templates = Host-Info.plist Preview-Info.plist Thumbnail-Info.plist Bridge-Info.plist
GENERATED_RESOURCES_DIR = $(builddir)/generated-resources
plist_outputs = $(addprefix $(GENERATED_RESOURCES_DIR)/,$(plist_templates))

bundle-stamp: $(plist_outputs) SixelPreview SixelThumbnail SixelQuickLookHost SixelPreviewBridge
	@$(MKDIR_P) $(HOST_APP)/Contents/MacOS $(HOST_APP)/Contents/Resources $(HOST_APP)/Contents/PlugIns
	@$(MKDIR_P) $(PREVIEW_APPEX)/Contents/MacOS $(PREVIEW_APPEX)/Contents/Resources
	@$(MKDIR_P) $(THUMB_APPEX)/Contents/MacOS $(THUMB_APPEX)/Contents/Resources
	@$(MKDIR_P) $(BRIDGE_APP)/Contents/MacOS $(BRIDGE_APP)/Contents/Resources
	$(INSTALL_PROGRAM) SixelQuickLookHost $(HOST_APP)/Contents/MacOS/SixelQuickLookHost
	$(INSTALL_PROGRAM) SixelPreview $(PREVIEW_APPEX)/Contents/MacOS/SixelPreview
	$(INSTALL_PROGRAM) SixelThumbnail $(THUMB_APPEX)/Contents/MacOS/SixelThumbnail
	$(INSTALL_PROGRAM) SixelPreviewBridge $(BRIDGE_APP)/Contents/MacOS/SixelPreviewBridge
	$(INSTALL_DATA) $(GENERATED_RESOURCES_DIR)/Host-Info.plist $(HOST_APP)/Contents/Info.plist
	$(INSTALL_DATA) $(GENERATED_RESOURCES_DIR)/Preview-Info.plist $(PREVIEW_APPEX)/Contents/Info.plist
	$(INSTALL_DATA) $(GENERATED_RESOURCES_DIR)/Thumbnail-Info.plist $(THUMB_APPEX)/Contents/Info.plist
	$(INSTALL_DATA) $(GENERATED_RESOURCES_DIR)/Bridge-Info.plist $(BRIDGE_APP)/Contents/Info.plist
	@touch $@

all-local: bundle-stamp

install-exec-local: bundle-stamp register_sixel_preview
	$(MKDIR_P) $(DESTDIR)$(libexecdir)/libsixel
	rm -rf $(DESTDIR)$(libexecdir)/libsixel/SixelQuickLookHost.app
	cp -R $(HOST_APP) $(DESTDIR)$(libexecdir)/libsixel/
	rm -rf $(DESTDIR)$(libexecdir)/libsixel/SixelPreviewBridge.app
	cp -R $(BRIDGE_APP) $(DESTDIR)$(libexecdir)/libsixel/
	$(INSTALL_PROGRAM) register_sixel_preview $(DESTDIR)$(libexecdir)/libsixel/register_sixel_preview
	@if test -z "$(DESTDIR)"; then \
	  quicklook_user=$${SUDO_USER:-$${LOGNAME:-$${USER}}}; \
	  if test -n "$$quicklook_user"; then \
	    cmd="$(install_quicklook_script) install --products-dir $(abs_builddir)/products --register-tool $(abs_builddir)/register_sixel_preview"; \
	    if test "$$quicklook_user" = "$${USER}"; then \
	      $$cmd; \
	    elif command -v sudo >/dev/null 2>&1; then \
	      sudo -u "$$quicklook_user" $$cmd; \
	    else \
	      echo "warning: sudo not found; skipping Quick Look user install for $$quicklook_user" >&2; \
	    fi; \
	  fi; \
	else \
	  echo "Skipping Quick Look user install because DESTDIR is set"; \
	fi

.PHONY: install-quicklook-extension uninstall-quicklook-extension

install-quicklook-extension: bundle-stamp register_sixel_preview
	$(AM_V_at)$(install_quicklook_script) install --products-dir $(abs_builddir)/products --register-tool $(abs_builddir)/register_sixel_preview

uninstall-quicklook-extension:
	$(AM_V_at)$(install_quicklook_script) uninstall --products-dir $(abs_builddir)/products --register-tool $(abs_builddir)/register_sixel_preview

uninstall-local:
	rm -rf $(DESTDIR)$(libexecdir)/libsixel/SixelQuickLookHost.app
	rm -rf $(DESTDIR)$(libexecdir)/libsixel/SixelPreviewBridge.app
	rm -f $(DESTDIR)$(libexecdir)/libsixel/register_sixel_preview
	@if test -z "$(DESTDIR)"; then \
	  quicklook_user=$${SUDO_USER:-$${LOGNAME:-$${USER}}}; \
	  if test -n "$$quicklook_user"; then \
	    cmd="$(install_quicklook_script) uninstall --products-dir $(abs_builddir)/products --register-tool $(abs_builddir)/register_sixel_preview"; \
	    if test "$$quicklook_user" = "$${USER}"; then \
	      $$cmd; \
	    elif command -v sudo >/dev/null 2>&1; then \
	      sudo -u "$$quicklook_user" $$cmd; \
	    else \
	      echo "warning: sudo not found; skipping Quick Look user uninstall for $$quicklook_user" >&2; \
	    fi; \
	  fi; \
	fi

clean-local:
	rm -rf $(PRODUCTS_DIR) $(MODULE_CACHE_DIR) bundle-stamp

else

all-local:
install-exec-local:
uninstall-local:
clean-local:

endif

CLEANFILES = bundle-stamp
