#!/bin/sh
# Provider error preserved.
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
    "${QUICKLOOK_LIFECYCLE_FIXTURE}" 'provider-failure' 'error-43' || {
    echo "not ok 1 - Provider error preserved."
    exit 0
}
echo "ok 1 - Provider error preserved."
exit 0
