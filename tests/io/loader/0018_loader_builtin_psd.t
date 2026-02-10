#!/bin/sh
# TAP test confirming the builtin loader can decode PSD inputs.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_psd="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.psd"

run_img2sixel "${input_psd}" >/dev/null || {
    fail 1 "builtin loader failed to decode PSD"
    exit 0
}

pass 1 "builtin loader decodes PSD"
exit 0
