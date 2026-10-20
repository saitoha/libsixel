#!/bin/sh
# Preview bundle contains the completed executable.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

cmp "${TOP_BUILDDIR}/quicklook-extension/SixelPreview" "${TOP_BUILDDIR}/quicklook-extension/products/SixelQuickLookHost.app/Contents/PlugIns/SixelPreview.appex/Contents/MacOS/SixelPreview" || {
    echo "not ok 1 - Preview bundle contains the completed executable"
    exit 0
}
echo "ok 1 - Preview bundle contains the completed executable"
exit 0
