#!/bin/sh
# TAP test confirming librsvg gives size attributes priority over viewBox.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-size-overrides-viewbox.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-size-overrides-viewbox.six"
png_path="${ARTIFACT_LOCAL_DIR}/librsvg-size-overrides-viewbox.png"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='19' height='11' viewBox='0 0 95 55'><rect x='0' y='0' width='95' height='55' fill='#ff00ff'/></svg>" >"${svg_path}"

echo "1..1"
set -v

run_img2sixel -L librsvg! "${svg_path}" >"${sixel_path}" || {
    fail 1 "librsvg size+viewBox conversion failed"
    exit 0
}

run_sixel2png -i "${sixel_path}" -o "${png_path}" || {
    fail 1 "sixel2png decode failed"
    exit 0
}

file "${png_path}" | grep -q " 19 x 11" || {
    fail 1 "size attributes did not override viewBox geometry"
    exit 0
}

pass 1 "librsvg size attributes override viewBox geometry"
exit 0
