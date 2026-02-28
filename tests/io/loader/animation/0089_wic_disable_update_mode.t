#!/bin/sh
# TAP test: wic handles disable/update flag combination.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n"
    exit 0
}

test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L wic! -ldisable -dnone -u \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" -o/dev/null || {
    fail 1 "wic disable/update flag combination failed"
    exit 0
}

pass 1 "wic disable/update flag combination succeeded"
exit 0
