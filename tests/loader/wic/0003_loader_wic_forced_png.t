#!/bin/sh
# TAP test confirming --loaders wic! forces PNG decoding path.

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

run_img2sixel -L wic! "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png" \
    >/dev/null || {
    echo "not ok" 1 - "wic forced PNG decoding failed"
    exit 0
}

echo "ok" 1 - "wic forced PNG decoding succeeds"
exit 0
