#!/bin/sh
# Ensure DELTA_E00 emits a finite numeric value on stable fixtures.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa -m DELTA_E00 "${image_ref}" "${image_out}" | tr -d '\r') || {
    fail 1 "lsqa DELTA_E00 execution failed"
    exit 0
}

if printf '%s\n' "${value}" | awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}'; then
    pass 1 "DELTA_E00 returned a finite value"
else
    fail 1 "DELTA_E00 returned non-numeric output: ${value}"
fi

exit 0
