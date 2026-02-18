#!/bin/sh
# TAP test confirming GIF input fails with --loaders wic!.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    skip_all "wic loader is unavailable"
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    skip_all "WIC is unavailable under wine"
}

echo "1..1"
set -v

set +e
run_img2sixel -L wic! "${input_gif}" >/dev/null 2>&1
rc=$?
set -e

test "${rc}" -ne 0 || {
    fail 1 "wic forced GIF decoding should fail"
    exit 0
}

pass 1 "wic forced GIF decoding fails without fallback"
exit 0
