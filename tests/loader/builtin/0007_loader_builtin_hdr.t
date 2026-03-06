#!/bin/sh
# TAP test confirming the builtin loader can decode HDR inputs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.hdr"

run_img2sixel "${input_hdr}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode HDR"
    exit 0
}

echo "ok" 1 - "builtin loader decodes HDR"
exit 0
