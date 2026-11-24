#!/bin/sh
# Packaged preview code signature is valid.
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
    "${TOP_BUILDDIR}/quicklook-extension/package/work/expanded/payload.pkg/Payload/Applications/SixelQuickLookHost.app/Contents/PlugIns/SixelPreview.appex" || {
    printf '\n%s\n' "not ok 1 - Invalid preview signature"
    exit 0
}

printf '\n%s\n' "ok 1 - Packaged preview code signature is valid."
exit 0
