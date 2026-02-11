#!/bin/sh
# Ensure HIGHFREQ_DELTA emits a finite numeric value on stable fixtures.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa -m HIGHFREQ_DELTA "${image_ref}" "${image_out}" | tr -d '\r') || {
    fail 1 "lsqa HIGHFREQ_DELTA execution failed"
    exit 0
}

printf '%s\n' "${value}" |
    awk '/^[+-]?[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?$/{ok=1} END{exit ok?0:1}' || {
    fail 1 "HIGHFREQ_DELTA returned non-numeric output: ${value}"
    exit 0
}

pass 1 "HIGHFREQ_DELTA returned a finite value"

exit 0
