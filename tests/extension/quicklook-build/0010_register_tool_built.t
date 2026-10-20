#!/bin/sh
# Registration helper is built without executing it.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

test -x "${TOP_BUILDDIR}/quicklook-extension/register_sixel_preview" || {
    echo "not ok 1 - Registration helper is built without executing it"
    exit 0
}
echo "ok 1 - Registration helper is built without executing it"
exit 0
