#!/bin/sh
# Verify builtin PSD RGB+alpha with embedded ICC and --bgcolor avoids false failure trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_rgb8_alpha_embedded_esrgb.psd"

command_status=0
command_output=$(set +xv; run_img2sixel -Lbuiltin:cms_engine=auto! \
    -B "#112233" "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin decode failed: ${command_output}"
    exit 0
}

printf '%s\n' "${command_output}" | grep -F "embedded ICC conversion failed" >/dev/null && {
    echo "not ok" 1 - "unexpected embedded ICC failure trace was emitted"
    exit 0
}

echo "ok" 1 - "builtin PSD RGB+alpha ICC path avoids false failure trace"
exit 0
