#!/bin/sh
# TAP test confirming librsvg respects explicit width and height geometry.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-explicit-size.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-explicit-size.six"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='13' height='7'><rect x='0' y='0' width='13' height='7' fill='#0000ff'/></svg>" >"${svg_path}"

echo "1..1"
set -v

run_img2sixel -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg explicit-size conversion failed"
    exit 0
}

sed 's/^.*"//;s/#.*$//' "${sixel_path}" | grep -q '^1;1;13;7$' || {
    echo "not ok" 1 - "explicit width/height geometry is not 13x7"
    exit 0
}

echo "ok" 1 - "librsvg explicit width/height geometry is respected"
exit 0
