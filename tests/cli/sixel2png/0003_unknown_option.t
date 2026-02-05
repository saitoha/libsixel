#!/bin/sh
# TAP test verifying sixel2png rejects unknown options gracefully.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

stderr_capture="${output_dir}/stderr.txt"
if run_sixel2png --unknown 2>"${stderr_capture}" >"${output_dir}/stdout.txt"; then
    fail 1 "unknown option should fail"
else
    if grep -qi -- "unrecognized option" "${stderr_capture}"; then
        pass 1 "unknown option reported"
    else
        fail 1 "error message did not mention unknown option"
    fi
fi

exit "${status}"
