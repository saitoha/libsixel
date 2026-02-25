#!/bin/sh
# TAP test confirming forced coregraphics decoding rejects corrupted HEIF input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/corrupted/truncated.heif" >/dev/null && {
    fail 1 "coregraphics corrupted HEIF should fail"
    exit 0
}

pass 1 "coregraphics corrupted HEIF is rejected"
exit 0
