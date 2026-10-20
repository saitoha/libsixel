#!/bin/sh
# Preview has the required actual Mach-O architecture set matching the manifest.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check architectures \
    "${TOP_BUILDDIR}/quicklook-extension/package" preview || {
    echo "not ok 1 - Preview has the required actual Mach-O architecture set matching the manifest."
    exit 0
}

echo "ok 1 - Preview has the required actual Mach-O architecture set matching the manifest."
exit 0
