#!/bin/sh
# Ensure BANDING_REL emits a finite numeric value on stable fixtures.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa -m BANDING_REL "${image_ref}" "${image_out}" | tr -d '\r') || {
    echo "not ok" 1 "lsqa BANDING_REL execution failed"
    exit 0
}

printf '%s\n' "${value}" |
    awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}' || {
    echo "not ok" 1 "BANDING_REL returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 "BANDING_REL returned a finite value"

exit 0
