#!/bin/sh
# TAP test confirming the builtin loader can decode HDR inputs.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.hdr"

run_img2sixel "${input_hdr}" >/dev/null || {
    fail 1 "builtin loader failed to decode HDR"
    exit 0
}

pass 1 "builtin loader decodes HDR"
exit 0
