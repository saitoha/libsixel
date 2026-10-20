#!/bin/sh
# Host has the required numeric bundle versions matching the distribution.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check versions \
    "${TOP_BUILDDIR}/quicklook-extension/package" host || {
    echo "not ok 1 - Host has the required numeric bundle versions matching the distribution."
    exit 0
}

echo "ok 1 - Host has the required numeric bundle versions matching the distribution."
exit 0
