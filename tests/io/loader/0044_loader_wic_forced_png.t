#!/bin/sh
# TAP test confirming --loaders wic! forces PNG decoding path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n"
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

loader_output=$(run_img2sixel -L wic! "${input_png}" 2>&1 >/dev/null) || rc=$?

printf '%s' "${loader_output}" >&2

echo "1..1"
set -v

test "${rc-0}" -eq 0 || {
    fail 1 "wic forced PNG decoding failed"
    exit 0
}

pass 1 "wic forced PNG decoding succeeds"
exit 0
