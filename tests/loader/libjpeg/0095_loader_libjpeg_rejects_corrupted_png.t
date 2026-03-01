#!/bin/sh
# TAP test confirming forced libjpeg loader rejects corrupted PNG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"

run_img2sixel -L libjpeg! "${input_png}" >/dev/null && {
    fail 1 "forced libjpeg loader accepted corrupted PNG"
    exit 0
}

pass 1 "forced libjpeg loader rejects corrupted PNG"
exit 0
