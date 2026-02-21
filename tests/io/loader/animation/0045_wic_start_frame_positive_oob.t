#!/bin/sh
# TAP test: wic rejects out-of-range positive frame indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=999 \
    -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    fail 1 "wic positive out-of-range start frame succeeded"
    exit 0
}

pass 1 "wic positive out-of-range start frame is rejected"
exit 0
