#!/bin/sh
# TAP test confirming progressive JPEG decoding works end-to-end.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

progressive_jpeg="${images_dir}/snake-progressive-16x16.jpg"


if run_img2sixel "${progressive_jpeg}" \
        >"${ARTIFACT_LOCAL_DIR}/progressive.sixel"; then
    pass ${case_id} "progressive JPEG converts"
else
    fail ${case_id} "progressive JPEG conversion failed"
fi

exit "${status}"
