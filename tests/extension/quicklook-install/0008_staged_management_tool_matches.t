#!/bin/sh
# Observe staged management tool matches.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
test -n "${QUICKLOOK_STAGE_DIR:-}" && test -n "${QUICKLOOK_STAGE_SOURCE:-}" || {
    echo "1..0 # SKIP requires an explicitly prepared distribution stage"
    exit 0
}
echo "1..1"
set -v

cmp "${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" \
    "${QUICKLOOK_STAGE_DIR}/sixel-quicklook-package" >/dev/null || {
    echo "not ok 1 - Staged management executable differs"
    exit 0
}

echo "ok 1 - staged management tool matches."
exit 0
