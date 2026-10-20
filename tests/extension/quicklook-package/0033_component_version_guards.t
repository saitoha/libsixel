#!/bin/sh
# Component version guards name all fixed bundles.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check component-bundle-version \
    "${TOP_BUILDDIR}/quicklook-extension/package" || {
    echo "not ok 1 - Component version guards name all fixed bundles."
    exit 0
}

echo "ok 1 - Component version guards name all fixed bundles."
exit 0
