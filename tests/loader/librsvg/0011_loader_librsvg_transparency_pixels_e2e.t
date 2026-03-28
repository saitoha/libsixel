#!/bin/sh
# TAP test confirming background composition changes decoded SVG pixels.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.six"
bg_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.six"
default_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.png"
bg_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default transparent SVG conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! -B '#ffffff' "${svg_path}" >"${bg_sixel}" || {
    echo "not ok" 1 - "background-composited SVG conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${default_sixel}" -o "${default_png}" || {
    echo "not ok" 1 - "default transparent SIXEL decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${bg_sixel}" -o "${bg_png}" || {
    echo "not ok" 1 - "background-composited SIXEL decode failed"
    exit 0
}

default_cksum="$(cksum "${default_png}")"
bg_cksum="$(cksum "${bg_png}")"
test "${default_cksum}" != "${bg_cksum}" || {
    echo "not ok" 1 - "default/background decoded PNG unexpectedly matched"
    exit 0
}

echo "ok" 1 - "background composition changes decoded SVG pixels"
exit 0
