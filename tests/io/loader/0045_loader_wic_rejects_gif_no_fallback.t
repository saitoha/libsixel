#!/bin/sh
# TAP test confirming GIF input fails with --loaders wic!.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

test "${HAVE_WIC-}" = 1 || {
    skip_all "wic loader is unavailable"
}

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
