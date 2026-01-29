#!/bin/sh
# TAP test checking tab-separated colour introducers are decoded successfully.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/dcs-variations.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

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

exit "${status}"
