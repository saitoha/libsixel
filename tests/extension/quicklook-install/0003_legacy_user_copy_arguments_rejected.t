#!/bin/sh
# Observe legacy user copy arguments rejected.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_SRCDIR}/tools/quicklook-extension.bash" uninstall \
    --products-dir /unused --user-home /must-not-touch >/dev/null 2>&1 && {
    echo "not ok 1 - Legacy deletion arguments were accepted"
    exit 0
}

echo "ok 1 - legacy user copy arguments rejected."
exit 0
