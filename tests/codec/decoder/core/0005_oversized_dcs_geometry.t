#!/bin/sh
# TAP test checking oversized DCS geometry is tolerated.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

oversized="${TOP_SRCDIR}/tests/data/inputs/snake_64-oversized.six"

if run_img2sixel "${oversized}" >"${ARTIFACT_LOCAL_DIR}/output.sixel"; then
    pass ${case_id} "oversized DCS geometry tolerated"
else
    fail ${case_id} "oversized DCS geometry rejected"
fi

exit "${status}"
