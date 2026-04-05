#!/bin/sh
# Verify -B rejects invalid color strings.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
status=0
stderr_text=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -B "not_a_color" \
    "${image_ref}" "${image_out}" 2>&1 1>/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "invalid -B should exit with status 2"
    exit 0
}

test "${stderr_text#*invalid argument for*}" != "${stderr_text}" || {
    echo "not ok" 1 - "invalid -B error message was missing"
    exit 0
}

echo "ok" 1 - "invalid -B value is rejected"
exit 0
