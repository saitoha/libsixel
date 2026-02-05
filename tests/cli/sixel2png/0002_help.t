#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

help_output="${output_dir}/help.txt"
if run_sixel2png -H 1>"${help_output}"; then
    if grep -Eq '^Usage: sixel2png' "${help_output}"; then
        pass 1 "-H prints usage"
    else
        fail 1 "help usage header missing"
    fi
else
    fail 1 "-H exited with failure"
fi

exit "${status}"
