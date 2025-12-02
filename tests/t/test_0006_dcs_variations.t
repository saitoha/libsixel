#!/bin/sh
# TAP test checking img2sixel DCS parsing variations.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/dcs-variations.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..2"

snake_png="${images_dir}/snake.png"

require_file "${snake_png}"

if run_img2sixel "${snake_png}" >"${output_dir}/case01-stage.sixel" \
        2>>"${log_file}" && \
        sed 's/C/C:/g' "${output_dir}/case01-stage.sixel" | tr ':' '\t' | \
        run_img2sixel >"${output_dir}/case01.sixel" 2>>"${log_file}"; then
    pass ${case_id} "tab-separated colour introducers handled"
else
    fail ${case_id} "tab-separated colour introducers rejected"
fi
case_id=$((case_id + 1))

if run_img2sixel "${snake_png}" >"${output_dir}/case02-stage.sixel" \
        2>>"${log_file}" && \
        sed 's/"1;1;600;450/"1;1;700;500/' \
        "${output_dir}/case02-stage.sixel" | \
        run_img2sixel >"${output_dir}/case02.sixel" 2>>"${log_file}"; then
    pass ${case_id} "oversized DCS geometry tolerated"
else
    fail ${case_id} "oversized DCS geometry rejected"
fi

exit "${status}"
