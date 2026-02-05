#!/bin/sh
# TAP test verifying sixel2png reports missing required arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"
set -v

stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
if run_sixel2png -i 2>"${stderr_capture}" >"${ARTIFACT_LOCAL_DIR}/stdout.txt"; then
    fail 1 "-i without value should fail"
else
    if grep -qi -- "missing" "${stderr_capture}" \
            && grep -qi -- "--input" "${stderr_capture}"; then
        pass 1 "missing input argument reported"
    else
        fail 1 "error message did not mention missing input"
    fi
fi

exit "${status}"
