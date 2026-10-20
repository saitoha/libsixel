#!/bin/sh
# Package has the fixed receipt identifier.
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
    '/pkg-info/@identifier') || {
    echo "not ok 1 - Cannot read package identifier"
    exit 0
}
test "$value" = "com.saitoha.libsixel.quicklook.pkg" || {
    echo "not ok 1 - Unexpected package identifier"
    exit 0
}
echo "ok 1 - Package has the fixed receipt identifier."
exit 0
