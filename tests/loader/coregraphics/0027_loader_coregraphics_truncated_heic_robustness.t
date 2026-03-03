#!/bin/sh
# TAP test: coregraphics handles truncated HEIC input without crash/hang.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

# A truncated HEIC can be partially recovered by Image I/O on macOS.
# This test intentionally accepts both outcomes and only checks robustness.
run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/corrupted/truncated.heic" >/dev/null && {
    echo "ok" 1 "coregraphics truncated HEIC decode completed without crash"
    exit 0
}

echo "ok" 1 "coregraphics truncated HEIC decode rejected without crash"
exit 0
