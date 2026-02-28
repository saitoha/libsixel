#!/bin/sh
# TAP test confirming the builtin loader can decode PSD inputs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.psd"

run_img2sixel "${input_psd}" >/dev/null || {
    echo "not ok" 1 "builtin loader failed to decode PSD"
    exit 0
}

echo "ok" 1 "builtin loader decodes PSD"
exit 0
