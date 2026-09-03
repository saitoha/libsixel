#!/bin/sh
# Verify the diagnostics base has equivalent CLI and environment behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

cli_status=0
env_status=0
cli_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=human \
    -xcode -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || cli_status=$?
env_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || env_status=$?

test "${cli_status}" -eq 2 || {
    echo "not ok" 1 - "diagnostics CLI exit status mismatch"
    exit 0
}

test "${env_status}" -eq 2 || {
    echo "not ok" 1 - "diagnostics environment exit status mismatch"
    exit 0
}

test "${cli_message}" = "${env_message}" || {
    echo "not ok" 1 - "diagnostics CLI and environment output differ"
    exit 0
}

test "${cli_message#*\(matches:*}" = "${cli_message}" || {
    echo "not ok" 1 - "diagnostics code mode retained candidates"
    exit 0
}

echo "ok" 1 - "diagnostics CLI and environment output are equivalent"
exit 0
