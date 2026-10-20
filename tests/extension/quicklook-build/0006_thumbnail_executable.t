#!/bin/sh
# Thumbnail bundle contains the completed executable.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

cmp "${TOP_BUILDDIR}/quicklook-extension/SixelThumbnail" "${TOP_BUILDDIR}/quicklook-extension/products/SixelQuickLookHost.app/Contents/PlugIns/SixelThumbnail.appex/Contents/MacOS/SixelThumbnail" || {
    echo "not ok 1 - Thumbnail bundle contains the completed executable"
    exit 0
}
echo "ok 1 - Thumbnail bundle contains the completed executable"
exit 0
