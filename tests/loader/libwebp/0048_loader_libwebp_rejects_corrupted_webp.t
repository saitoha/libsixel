#!/bin/sh
# TAP test confirming forced libwebp loader rejects corrupted WebP inputs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..2"
set -v

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/truncated.webp" \
    >/dev/null && {
    echo "not ok" 1 - "forced libwebp loader accepted truncated WebP"
    exit 0
}
echo "ok" 1 - "forced libwebp loader rejects truncated WebP"

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/bad_anmf_size.webp" \
    >/dev/null && {
    echo "not ok" 2 - "forced libwebp loader accepted invalid ANMF size"
    exit 0
}
echo "ok" 2 - "forced libwebp loader rejects invalid ANMF size"

exit 0
