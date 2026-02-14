#!/bin/sh
# TAP test verifying sixel2png rejects unknown options gracefully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
stdout_capture="${ARTIFACT_LOCAL_DIR}/stdout.txt"

run_sixel2png --unknown 2>"${stderr_capture}" >"${stdout_capture}" && {
    fail 1 "unknown option should fail"
    exit 0
}

grep -qi "unrecognized option" "${stderr_capture}" >/dev/null 2>&1 || {
    fail 1 "error message did not mention unknown option"
    exit 0
}

pass 1 "unknown option reported"
exit 0
