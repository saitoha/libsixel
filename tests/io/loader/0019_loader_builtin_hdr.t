#!/bin/sh
# TAP test confirming the builtin loader can decode HDR inputs.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


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
        >"${output_dir}/stbi_minimal_hdr.sixel"; then
    pass ${case_id} "builtin loader decodes HDR"
else
    fail ${case_id} "builtin loader failed to decode HDR"
fi

exit "${status}"
