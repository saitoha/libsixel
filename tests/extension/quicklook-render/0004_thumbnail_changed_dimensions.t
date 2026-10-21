#!/bin/sh
# Thumbnail output rejects changed dimensions.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP Quick Look extension disabled or unsupported"
    exit 0
}
test "${CROSS_COMPILING:-no}" != yes || {
    echo "1..0 # SKIP cross-built native observation"
    exit 0
}
echo "1..1"
set -v

set +e
"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-render" check-thumbnail "${TOP_SRCDIR}/tests/extension/quicklook-render/wrong-size.png" >&2
status=$?
set -e
test "$status" = 1 || {
    echo "not ok 1 - Thumbnail output rejects changed dimensions"
    exit 0
}
echo "ok 1 - Thumbnail output rejects changed dimensions"
exit 0
