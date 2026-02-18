#!/bin/sh
# TAP test confirming GIF input fails with --loaders wic!.

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
    printf "1..0 # SKIP WIC is unavailable under wine"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -L wic! "${input_gif}" >/dev/null 2>&1 || rc=$?

test "${rc-0}" -ne 0 || {
    fail 1 "wic forced GIF decoding should fail"
    exit 0
}

pass 1 "wic forced GIF decoding fails without fallback"
exit 0
