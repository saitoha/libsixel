#!/bin/sh
# TAP test verifying sixel2png reports missing required arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
stdout_capture="${ARTIFACT_LOCAL_DIR}/stdout.txt"

run_sixel2png -i 2>"${stderr_capture}" >"${stdout_capture}" && {
    fail 1 "-i without value should fail"
    exit 0
}

grep -qi "missing" "${stderr_capture}" >/dev/null 2>&1 || {
    fail 1 "error message did not mention missing input"
    exit 0
}

grep -qi "--input" "${stderr_capture}" >/dev/null 2>&1 || {
    fail 1 "error message did not mention missing input"
    exit 0
}

pass 1 "missing input argument reported"
exit 0
