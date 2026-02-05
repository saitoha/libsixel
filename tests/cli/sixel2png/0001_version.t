#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"
set -v

version_output="${ARTIFACT_LOCAL_DIR}/version.txt"
if run_sixel2png -V >"${version_output}"; then
    if grep -Eq '^sixel2png ' "${version_output}"; then
        pass 1 "-V prints version"
    else
        fail 1 "version header missing"
    fi
else
    fail 1 "-V exited with failure"
fi

exit "${status}"
