#!/bin/sh
# TAP test confirming the builtin loader can decode PIC inputs.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.pic"

run_img2sixel "${input_pic}" >/dev/null || {
    fail 1 "builtin loader failed to decode PIC"
    exit 0
}

pass 1 "builtin loader decodes PIC"
exit 0
