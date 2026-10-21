#!/bin/sh
# Preview output rejects changed pixels.
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
"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-render" check-preview "${TOP_SRCDIR}/tests/extension/quicklook-render/wrong-color" >&2
status=$?
set -e
test "$status" = 1 || {
    echo "not ok 1 - Preview output rejects changed pixels"
    exit 0
}
echo "ok 1 - Preview output rejects changed pixels"
exit 0
