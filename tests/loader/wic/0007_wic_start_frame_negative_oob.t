#!/bin/sh
# TAP test: wic rejects out-of-range negative frame indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n";
    exit 0
}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=-999 \
    -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    echo "not ok" 1 - "wic negative out-of-range start frame succeeded"
    exit 0
}

echo "ok" 1 - "wic negative out-of-range start frame is rejected"
exit 0
