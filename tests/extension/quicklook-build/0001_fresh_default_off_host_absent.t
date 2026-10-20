#!/bin/sh
# A separately prepared fresh default-off build has no host app.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 0 || {
    echo "1..0 # SKIP requires quicklook_extension=0"
    exit 0
}
# Reconfiguration can retain apps; never infer freshness from the feature flag.
test -n "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR:-}" || {
    echo "1..0 # SKIP requires a separately prepared fresh default-off build"
    exit 0
}
echo "1..1"
set -v

# Reject a missing or unbuilt fixture instead of accepting vacuous absence.
test -f "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/config.h" &&
    test -x "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/converters/img2sixel" || {
    echo "not ok 1 - Fresh default-off build input is missing or incomplete"
    exit 0
}
test ! -e "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/quicklook-extension/products/SixelQuickLookHost.app" || {
    echo "not ok 1 - Fresh default-off build has no host app"
    exit 0
}
echo "ok 1 - Fresh default-off build has no host app"
exit 0
