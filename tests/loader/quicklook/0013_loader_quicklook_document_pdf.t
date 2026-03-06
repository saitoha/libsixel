#!/bin/sh
# TAP test: quicklook renders PDF document input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/quicklook/sample.pdf" >/dev/null || {
    echo "not ok" 1 - "quicklook PDF render failed"
    exit 0
}

echo "ok" 1 - "quicklook renders PDF"
exit 0
