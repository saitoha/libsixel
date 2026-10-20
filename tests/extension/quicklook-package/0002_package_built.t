#!/bin/sh
# Normal enabled build produces a package.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

test -s "${TOP_BUILDDIR}/quicklook-extension/package/SixelQuickLook.pkg" || {
    echo "not ok 1 - Package is missing"
    exit 0
}

echo "ok 1 - Normal enabled build produces a package."
exit 0
