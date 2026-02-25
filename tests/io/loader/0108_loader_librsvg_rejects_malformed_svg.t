#!/bin/sh
# TAP test confirming forced librsvg loader rejects malformed SVG input.

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

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-malformed.svg"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg'><g><rect x='0' y='0' width='1' height='1'></svg>" >"${svg_path}"

echo "1..1"
set -v

set +e
run_img2sixel -L librsvg! "${svg_path}" >/dev/null
status="$?"
set -e

test "${status}" -ne 0 || {
    fail 1 "forced librsvg unexpectedly accepted malformed SVG"
    exit 0
}

pass 1 "forced librsvg rejects malformed SVG"
exit 0
