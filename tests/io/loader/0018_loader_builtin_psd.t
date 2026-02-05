#!/bin/sh
# TAP test confirming the builtin loader can decode PSD inputs.

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

input_psd="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.psd"


if run_img2sixel "${input_psd}" \
        >"${output_dir}/stbi_minimal_psd.sixel"; then
    pass ${case_id} "builtin loader decodes PSD"
else
    fail ${case_id} "builtin loader failed to decode PSD"
fi

exit "${status}"
