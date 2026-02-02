#!/bin/sh
# TAP test confirming the builtin loader can decode PSD inputs.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/builtin-psd.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


echo "1..1"
set -v

input_psd="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.psd"
require_file "${input_psd}"

if run_img2sixel "${input_psd}" \
        >"${output_dir}/stbi_minimal_psd.sixel" 2>>"${log_file}"; then
    pass ${case_id} "builtin loader decodes PSD"
else
    fail ${case_id} "builtin loader failed to decode PSD"
fi

exit "${status}"
