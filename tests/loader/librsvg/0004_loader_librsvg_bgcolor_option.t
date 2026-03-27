#!/bin/sh
# TAP test confirming librsvg background option affects transparent SVG output.

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

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor-default.six"
white_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor-white.six"
default_png="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor-default.png"
white_png="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor-white.png"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='2' height='1'></svg>" >"${svg_path}"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default background conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! -B '#ffffff' "${svg_path}" >"${white_sixel}" || {
    echo "not ok" 1 - "white background conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${default_sixel}" -o "${default_png}" || {
    echo "not ok" 1 - "default sixel decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${white_sixel}" -o "${white_png}" || {
    echo "not ok" 1 - "white sixel decode failed"
    exit 0
}

cmp -s "${default_png}" "${white_png}" && {
    echo "not ok" 1 - "background option did not change rendered output"
    exit 0
}

echo "ok" 1 - "background option changes transparent SVG output"
exit 0
