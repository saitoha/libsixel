#!/bin/sh
# Observe relative destdir rejected.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

output=$("${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" \
    manage uninstall /nonexistent /usr/local/share relative no 2>&1) && {
    echo "not ok 1 - Relative DESTDIR was accepted"
    exit 0
}
case "$output" in
    *"DESTDIR and datadir must be absolute paths without '..'"*) ;;
    *) echo "not ok 1 - Unexpected refusal"; exit 0 ;;
esac

echo "ok 1 - relative destdir rejected."
exit 0
