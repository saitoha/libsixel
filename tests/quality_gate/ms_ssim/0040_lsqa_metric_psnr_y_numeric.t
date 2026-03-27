#!/bin/sh
# Ensure PSNR_Y emits a finite numeric value on stable fixtures.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m PSNR_Y "${image_ref}" "${image_out}" | tr -d '\r') || {
    echo "not ok" 1 - "lsqa PSNR_Y execution failed"
    exit 0
}

printf '%s\n' "${value}" |
    awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}' || {
    echo "not ok" 1 - "PSNR_Y returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 - "PSNR_Y returned a finite value"

exit 0
