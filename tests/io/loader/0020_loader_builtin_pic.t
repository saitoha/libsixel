#!/bin/sh
# TAP test confirming the builtin loader can decode PIC inputs.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


echo "1..1"
set -v

input_pic="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.pic"
require_file "${input_pic}"

if run_img2sixel "${input_pic}" \
        >"${output_dir}/stbi_minimal_pic.sixel"; then
    pass ${case_id} "builtin loader decodes PIC"
else
    fail ${case_id} "builtin loader failed to decode PIC"
fi

exit "${status}"
