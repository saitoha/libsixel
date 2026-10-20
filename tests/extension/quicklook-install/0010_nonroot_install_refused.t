#!/bin/sh
# Observe actual nonroot refusal before any Installer execution.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
test "$(id -u)" != 0 || {
    echo "1..0 # SKIP requires a nonroot caller"
    exit 0
}
echo "1..1"
set -v

set +e
output=$("${TOP_SRCDIR}/tools/quicklook-extension.bash" install \
    --package-dir '/nonexistent package' \
    --tool "${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" \
    --destdir '' 2>&1)
status=$?
set -e
test "$status" = 77 || {
    echo "not ok 1 - Missing privilege failure status"
    exit 0
}
case "$output" in
    *'Run as administrator: /usr/sbin/installer -pkg "/nonexistent package/SixelQuickLook.pkg" -target /'*) ;;
    *) echo "not ok 1 - Missing actionable Installer command"; exit 0 ;;
esac

echo "ok 1 - Nonroot installation is refused without elevation."
exit 0
