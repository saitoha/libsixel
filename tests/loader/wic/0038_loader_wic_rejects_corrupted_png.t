#!/bin/sh
# TAP test confirming forced wic loader rejects corrupted PNG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
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

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"

run_img2sixel -L wic! "${input_png}" >/dev/null && {
    echo "not ok" 1 "forced wic loader accepted corrupted PNG"
    exit 0
}

echo "ok" 1 "forced wic loader rejects corrupted PNG"
exit 0
