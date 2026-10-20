#!/bin/sh
# Package version equals the configured distribution build.
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
    '/pkg-info/@version') || {
    echo "not ok 1 - Cannot read package version"
    exit 0
}
test "$value" = "${QUICKLOOK_PACKAGE_VERSION:?}" || {
    echo "not ok 1 - Unexpected package version"
    exit 0
}
echo "ok 1 - Package version equals the configured distribution build."
exit 0
