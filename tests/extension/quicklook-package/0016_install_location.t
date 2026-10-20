#!/bin/sh
# Package uses the fixed system install location.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

value=$("${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" \
    xml-value \
    "${TOP_BUILDDIR}/quicklook-extension/package/work/expanded/payload.pkg/PackageInfo" \
    '/pkg-info/@install-location') || {
    echo "not ok 1 - Cannot read package install-location"
    exit 0
}
test "$value" = "/" || {
    echo "not ok 1 - Unexpected package install-location"
    exit 0
}
echo "ok 1 - Package uses the fixed system install location."
exit 0
