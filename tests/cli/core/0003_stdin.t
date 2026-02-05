#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without producing
# output.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

tmp_dir="${ARTIFACT_LOCAL_DIR}"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

output_file=$(make_temp_file "${tmp_dir}" "capture.stdin")

if echo a | run_img2sixel >"${output_file}"; then
    :
fi

if [ -s "${output_file}" ]; then
    fail 1 "img2sixel produced output for invalid stdin"
else
    pass 1 "invalid stdin rejected without output"
fi

rm -f "${output_file}"

exit "${status}"
