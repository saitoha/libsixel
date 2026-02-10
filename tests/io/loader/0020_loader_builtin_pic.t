#!/bin/sh
# TAP test confirming the builtin loader can decode PIC inputs.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_pic="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.pic"

run_img2sixel "${input_pic}" >/dev/null || {
    fail 1 "builtin loader failed to decode PIC"
    exit 0
}

pass 1 "builtin loader decodes PIC"
exit 0
