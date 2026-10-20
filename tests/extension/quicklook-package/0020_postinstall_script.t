#!/bin/sh
# Package contains the declared postinstall script.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

cmp "${TOP_SRCDIR}/quicklook-extension/package-postinstall" \
    "${TOP_BUILDDIR}/quicklook-extension/package/work/expanded/payload.pkg/Scripts/postinstall" >/dev/null || {
    echo "not ok 1 - Packaged postinstall differs"
    exit 0
}

echo "ok 1 - Package contains the declared postinstall script."
exit 0
