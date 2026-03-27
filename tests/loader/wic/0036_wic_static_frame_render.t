#!/bin/sh
# TAP test: wic static frame rendering with animation flags succeeds.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L wic! -ldisable -dnone -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    echo "not ok" 1 - "wic static frame rendering failed"
    exit 0
}

echo "ok" 1 - "wic static frame rendering succeeded"
exit 0
