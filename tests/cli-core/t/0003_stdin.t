#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without producing
# output.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/stdin.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

output_file=$(make_temp_file "${tmp_dir}" "capture.stdin")

if echo a | run_img2sixel >"${output_file}" 2>>"${log_file}"; then
    :
fi

if [ -s "${output_file}" ]; then
    fail 1 "img2sixel produced output for invalid stdin"
else
    pass 1 "invalid stdin rejected without output"
fi

rm -f "${output_file}"

exit "${status}"
