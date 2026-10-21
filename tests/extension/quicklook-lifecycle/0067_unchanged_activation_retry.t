#!/bin/sh
# Unchanged activation retry preserves the observed journal and makes no new user calls.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:-0}" = 1 || {
    echo "1..0 # SKIP Quick Look extension disabled or unsupported"
    exit 0
}
test -n "${QUICKLOOK_LIFECYCLE_FIXTURE:-}" || {
    echo "1..0 # SKIP explicit private lifecycle preparation required"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-lifecycle-audit" \
    "${QUICKLOOK_LIFECYCLE_FIXTURE}" 'normal' 'unchanged-retry' || {
    echo "not ok 1 - Unchanged activation retry preserves the observed journal and makes no new user calls."
    exit 0
}
echo "ok 1 - Unchanged activation retry preserves the observed journal and makes no new user calls."
exit 0
