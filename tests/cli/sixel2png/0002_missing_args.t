#!/bin/sh
# TAP test verifying sixel2png reports missing required arguments.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
stdout_capture="${ARTIFACT_LOCAL_DIR}/stdout.txt"

run_sixel2png -i 2>"${stderr_capture}" >"${stdout_capture}" && {
    echo "not ok" 1 "-i without value should fail"
    exit 0
}

awk 'index(tolower($0), "missing") { found = 1; exit }
    END { exit found ? 0 : 1 }' "${stderr_capture}" >/dev/null 2>&1 || {
    echo "not ok" 1 "error message did not mention missing input"
    exit 0
}

awk 'index($0, "--input") { found = 1; exit }
    END { exit found ? 0 : 1 }' "${stderr_capture}" >/dev/null 2>&1 || {
    echo "not ok" 1 "error message did not mention missing input"
    exit 0
}

echo "ok" 1 "missing input argument reported"
exit 0
