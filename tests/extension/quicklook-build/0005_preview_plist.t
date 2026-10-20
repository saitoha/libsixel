#!/bin/sh
# Preview bundle contains its generated plist.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

cmp "${QUICKLOOK_PLIST_DIR}/Preview-Info.plist" "${TOP_BUILDDIR}/quicklook-extension/products/SixelQuickLookHost.app/Contents/PlugIns/SixelPreview.appex/Contents/Info.plist" || {
    echo "not ok 1 - Preview bundle contains its generated plist"
    exit 0
}
echo "ok 1 - Preview bundle contains its generated plist"
exit 0
