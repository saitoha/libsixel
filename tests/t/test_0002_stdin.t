#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without producing
# output.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/stdin.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..1"

output_file=$(make_temp_file "${tmp_dir}" "capture.stdin")

if echo -n a | run_img2sixel >"${output_file}" 2>>"${log_file}"; then
    :
fi

if [ -s "${output_file}" ]; then
    fail 1 "img2sixel produced output for invalid stdin"
else
    pass 1 "invalid stdin rejected without output"
fi

rm -f "${output_file}"

exit "${status}"
