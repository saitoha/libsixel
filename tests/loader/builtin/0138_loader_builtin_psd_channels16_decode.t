#!/bin/sh
# Verify builtin loader accepts the STB-supported PSD channel upper bound (16).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_channels16.psd"

run_img2sixel -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode 16-channel PSD"
    exit 0
}

echo "ok" 1 - "builtin loader decodes 16-channel PSD"
exit 0
