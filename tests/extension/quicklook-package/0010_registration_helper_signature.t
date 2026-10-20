#!/bin/sh
# Packaged registration helper signature is valid.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" signature \
    "${TOP_BUILDDIR}/quicklook-extension/package/work/expanded/payload.pkg/Payload/Library/Application Support/libsixel/QuickLook/register_sixel_preview" || {
    echo "not ok 1 - Invalid helper signature"
    exit 0
}

echo "ok 1 - Packaged registration helper signature is valid."
exit 0
