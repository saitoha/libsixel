#!/bin/sh
# Verify builtin loader decodes Duotone 16-bit ZIP.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_duotone16_zip.psd"

run_img2sixel -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode Duotone 16-bit ZIP"
    exit 0
}

echo "ok" 1 - "builtin loader decodes Duotone 16-bit ZIP"
exit 0
