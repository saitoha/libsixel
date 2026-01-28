#!/bin/sh
# TAP test confirming progressive JPEG decoding works end-to-end.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/progressive-jpeg.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

progressive_jpeg="${images_dir}/snake-progressive.jpg"
require_file "${progressive_jpeg}"

if run_img2sixel "${progressive_jpeg}" \
        >"${output_dir}/progressive.sixel" 2>>"${log_file}"; then
    pass ${case_id} "progressive JPEG converts"
else
    fail ${case_id} "progressive JPEG conversion failed"
fi

exit "${status}"
