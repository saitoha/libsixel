#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

help_output="${ARTIFACT_LOCAL_DIR}/help.txt"

run_sixel2png -H 1>"${help_output}" || {
    fail 1 "-H exited with failure"
    exit 0
}

grep -q '^Usage: sixel2png' "${help_output}" || {
    fail 1 "help usage header missing"
    exit 0
}

pass 1 "-H prints usage"
exit 0
