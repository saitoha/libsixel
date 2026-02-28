#!/bin/sh
# TAP test verifying -Q accepts kmeans suboptions with short prefixes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

label="quantize_suboption_success"

run_img2sixel -Qk:i=p:t=0.120 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null || {
    echo "not ok" 1 "-Q kmeans suboptions were rejected"
    exit 0
}

echo "ok" 1 "-Q accepts kmeans suboptions"
exit 0
