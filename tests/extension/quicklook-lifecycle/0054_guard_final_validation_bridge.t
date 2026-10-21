#!/bin/sh
# Guard bridge invariant at final validation.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:-0}" = 1 || {
    echo "1..0 # SKIP Quick Look extension disabled or unsupported"
    exit 0
}
test -n "${QUICKLOOK_LIFECYCLE_GUARDS:-}" || {
    echo "1..0 # SKIP explicit private lifecycle preparation required"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-lifecycle-audit" \
    "${QUICKLOOK_LIFECYCLE_GUARDS}" 'managed' 'guard-3:bridge' || {
    echo "not ok 1 - Guard bridge invariant at final validation."
    exit 0
}
echo "ok 1 - Guard bridge invariant at final validation."
exit 0
