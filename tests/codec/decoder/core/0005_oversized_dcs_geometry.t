#!/bin/sh
# TAP test checking oversized DCS geometry is tolerated.

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

snake_png="${images_dir}/snake.png"

require_file "${snake_png}"

if run_img2sixel "${snake_png}" >"${output_dir}/case02-stage.sixel" \
 && \
        sed 's/"1;1;600;450/"1;1;700;500/' \
        "${output_dir}/case02-stage.sixel" | \
        run_img2sixel >"${output_dir}/case02.sixel"; then
    pass ${case_id} "oversized DCS geometry tolerated"
else
    fail ${case_id} "oversized DCS geometry rejected"
fi

exit "${status}"
