#!/bin/sh
# Package declares preservation of existing directory attributes.
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
    '/pkg-info/@overwrite-permissions') || {
    echo "not ok 1 - Cannot read directory permissions policy"
    exit 0
}
test "$value" = false || {
    echo "not ok 1 - Existing directory attributes may be overwritten"
    exit 0
}

echo "ok 1 - Package declares preservation of existing directory attributes."
exit 0
