#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/help.log"
output_dir="${artifact_dir}/out"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

help_output="${output_dir}/help.txt"
if run_sixel2png -H 2>>"${log_file}" 1>"${help_output}"; then
    if grep -Eq '^Usage: sixel2png' "${help_output}"; then
        pass 1 "-H prints usage"
    else
        fail 1 "help usage header missing"
    fi
else
    fail 1 "-H exited with failure"
fi

exit "${status}"
