#!/bin/sh
# Verify builtin loader decodes CMYK 8-bit raw.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk8.psd"

run_img2sixel -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode CMYK 8-bit raw"
    exit 0
}

echo "ok" 1 - "builtin loader decodes CMYK 8-bit raw"
exit 0
