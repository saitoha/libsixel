#!/bin/sh
# Fresh default-off build has no package.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 0 || {
    echo "1..0 # SKIP requires quicklook_extension=0"
    exit 0
}
# The shared preparation owns freshness; reconfiguration may retain packages.
test -n "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR:-}" || {
    echo "1..0 # SKIP requires a separately prepared fresh default-off build"
    exit 0
}
echo "1..1"
set -v

test -f "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/config.h" &&
    test -x "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/converters/img2sixel" || {
    echo "not ok 1 - Fresh default-off input is missing or incomplete"
    exit 0
}
test ! -e "${QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR}/quicklook-extension/package/SixelQuickLook.pkg" || {
    echo "not ok 1 - Fresh default-off build has no package"
    exit 0
}

echo "ok 1 - Fresh default-off build has no package."
exit 0
