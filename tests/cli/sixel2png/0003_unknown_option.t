#!/bin/sh
# TAP test verifying sixel2png rejects unknown options gracefully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
if run_sixel2png --unknown 2>"${stderr_capture}" >"${ARTIFACT_LOCAL_DIR}/stdout.txt"; then
    fail 1 "unknown option should fail"
else
    if grep -qi -- "unrecognized option" "${stderr_capture}"; then
        pass 1 "unknown option reported"
    else
        fail 1 "error message did not mention unknown option"
    fi
fi

exit "${status}"
