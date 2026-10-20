#!/bin/sh
# Packaged manifest equals the external distribution manifest.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

cmp "${TOP_BUILDDIR}/quicklook-extension/package/payload-manifest.json" \
    "${TOP_BUILDDIR}/quicklook-extension/package/work/expanded/payload.pkg/Payload/Library/Application Support/libsixel/QuickLook/payload-manifest.json" || {
    echo "not ok 1 - Packaged manifest differs"
    exit 0
}

echo "ok 1 - Packaged manifest equals the external distribution manifest."
exit 0
