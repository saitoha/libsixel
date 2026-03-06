#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

help_output="${ARTIFACT_LOCAL_DIR}/help.txt"

run_sixel2png -H 1>"${help_output}" || {
    echo "not ok" 1 - "-H exited with failure"
    exit 0
}

grep -q '^Usage: sixel2png' "${help_output}" || {
    echo "not ok" 1 - "help usage header missing"
    exit 0
}

echo "ok" 1 - "-H prints usage"
exit 0
