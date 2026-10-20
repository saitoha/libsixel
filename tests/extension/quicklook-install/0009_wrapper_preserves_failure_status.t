#!/bin/sh
# Observe propagation of a failing native-tool boundary through the wrapper.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

set +e
"${TOP_SRCDIR}/tools/quicklook-extension.bash" install --package-dir '/unused path' \
    --tool /usr/bin/false >/dev/null 2>&1
status=$?
set -e
test "$status" = 1 || {
    echo "not ok 1 - Wrapper lost the failing command status"
    exit 0
}

echo "ok 1 - Wrapper preserves the native-tool failure status."
exit 0
