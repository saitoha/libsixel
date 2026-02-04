#!/bin/sh
# TAP test confirming the builtin loader can decode HDR inputs.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/builtin-hdr.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


echo "1..1"
set -v

input_hdr="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.hdr"
require_file "${input_hdr}"

if run_img2sixel "${input_hdr}" \
        >"${output_dir}/stbi_minimal_hdr.sixel" 2>>"${log_file}"; then
    pass ${case_id} "builtin loader decodes HDR"
else
    fail ${case_id} "builtin loader failed to decode HDR"
fi

exit "${status}"
