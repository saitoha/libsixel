#!/bin/sh
# Package preflight rejects non-system target volumes.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

output=$("${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" preflight /non-system-volume \
    "${TOP_BUILDDIR}/quicklook-extension/package/payload-manifest.json" 2>&1) && {
    echo "not ok 1 - Non-system target was accepted"
    exit 0
}
test "$output" = "Quick Look package: Only the system volume / is supported" || {
    echo "not ok 1 - Expected target-volume refusal"
    exit 0
}

echo "ok 1 - Package preflight rejects non-system target volumes."
exit 0
