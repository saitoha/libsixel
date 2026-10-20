#!/bin/sh
# Distribution version equals the configured build number.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

version=$(/usr/bin/plutil -extract packageVersion raw -o - \
    "${TOP_BUILDDIR}/quicklook-extension/package/payload-manifest.json") || {
    echo "not ok 1 - Cannot read distribution version"
    exit 0
}
test "$version" = "${QUICKLOOK_PACKAGE_VERSION:?}" || {
    echo "not ok 1 - Configured distribution version mismatch"
    exit 0
}

echo "ok 1 - Distribution version equals the configured build number."
exit 0
