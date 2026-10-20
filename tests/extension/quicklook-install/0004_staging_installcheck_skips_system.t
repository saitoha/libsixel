#!/bin/sh
# Observe staging installcheck skips system.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

output=$("${TOP_SRCDIR}/tools/quicklook-extension.bash" installcheck \
    --package-dir /nonexistent --tool "${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" \
    --destdir ' ' 2>&1) || {
    echo "not ok 1 - Staging installcheck tried to inspect the system"
    exit 0
}
test "$output" = 'Quick Look installcheck: SKIP STAGING (DESTDIR is set)' || {
    echo "not ok 1 - Staging was counted as installed"
    exit 0
}

echo "ok 1 - staging installcheck skips system."
exit 0
