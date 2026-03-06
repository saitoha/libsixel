#!/bin/sh
# Ensure DELTA_E00 emits a finite numeric value on stable fixtures.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa -m DELTA_E00 "${image_ref}" "${image_out}" | tr -d '\r') || {
    echo "not ok" 1 - "lsqa DELTA_E00 execution failed"
    exit 0
}

printf '%s\n' "${value}" |
    awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}' || {
    echo "not ok" 1 - "DELTA_E00 returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 - "DELTA_E00 returned a finite value"

exit 0
