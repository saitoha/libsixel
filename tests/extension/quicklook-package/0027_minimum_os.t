#!/bin/sh
# Declared minimum OS equals the actual Mach-O maximum with the supported floor.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check minimum-os \
    "${TOP_BUILDDIR}/quicklook-extension/package" || {
    echo "not ok 1 - Declared minimum OS equals the actual Mach-O maximum with the supported floor."
    exit 0
}

echo "ok 1 - Declared minimum OS equals the actual Mach-O maximum with the supported floor."
exit 0
