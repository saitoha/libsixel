#!/bin/sh
# Ensure PSNR_Y emits a finite numeric value on stable fixtures.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa -m PSNR_Y "${image_ref}" "${image_out}") || {
    fail 1 "lsqa PSNR_Y execution failed"
    exit 0
}

if printf '%s\n' "${value}" | awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}'; then
    pass 1 "PSNR_Y returned a finite value"
else
    fail 1 "PSNR_Y returned non-numeric output: ${value}"
fi

exit 0
