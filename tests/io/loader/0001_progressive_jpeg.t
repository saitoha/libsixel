#!/bin/sh
# TAP test confirming progressive JPEG decoding works end-to-end.

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

progressive_jpeg="${images_dir}/snake-progressive.jpg"
require_file "${progressive_jpeg}"

if run_img2sixel "${progressive_jpeg}" \
        >"${output_dir}/progressive.sixel"; then
    pass ${case_id} "progressive JPEG converts"
else
    fail ${case_id} "progressive JPEG conversion failed"
fi

exit "${status}"
