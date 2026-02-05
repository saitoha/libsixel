#!/bin/sh
# TAP test checking tab-separated colour introducers are decoded successfully.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

if run_img2sixel "${snake_png}" >"${output_dir}/case01-stage.sixel" && \
        sed 's/C/C:/g' "${output_dir}/case01-stage.sixel" | tr ':' '\t' | \
        run_img2sixel >"${output_dir}/case01.sixel"; then
    pass 1 "tab-separated colour introducers handled"
else
    fail 1 "tab-separated colour introducers rejected"
fi

exit "${status}"
