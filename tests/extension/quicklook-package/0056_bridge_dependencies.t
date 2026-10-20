#!/bin/sh
# Bridge has the required system-only dylib, dynamic-loader and rpath references.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check dependencies \
    "${TOP_BUILDDIR}/quicklook-extension/package" bridge || {
    echo "not ok 1 - Bridge has the required system-only dylib, dynamic-loader and rpath references."
    exit 0
}

echo "ok 1 - Bridge has the required system-only dylib, dynamic-loader and rpath references."
exit 0
