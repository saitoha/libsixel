#!/bin/sh
# Observe dry run has no tool execution.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

output=$("${TOP_SRCDIR}/tools/quicklook-extension.bash" install --dry-run \
    --package-dir '/missing package' --tool /must-not-run --destdir '' 2>&1) || {
    echo "not ok 1 - Dry-run executed the package tool"
    exit 0
}
case "$output" in
    'Quick Look dry-run: install package=/missing package DESTDIR=') ;;
    *) echo "not ok 1 - Missing dry-run observation"; exit 0 ;;
esac

echo "ok 1 - dry run has no tool execution."
exit 0
