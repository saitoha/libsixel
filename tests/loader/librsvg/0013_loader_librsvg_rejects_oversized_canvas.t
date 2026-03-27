#!/bin/sh
# TAP test confirming librsvg rejects oversized canvases before allocation.

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
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-oversized-canvas.svg"
out_path="${ARTIFACT_LOCAL_DIR}/librsvg-oversized-canvas.six"
err_path="${ARTIFACT_LOCAL_DIR}/librsvg-oversized-canvas.err"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='20000' height='20000'><rect x='0' y='0' width='20000' height='20000' fill='#ff0000'/></svg>" >"${svg_path}"

run_img2sixel -L librsvg! "${svg_path}" >"${out_path}" 2>"${err_path}" && {
    echo "not ok" 1 - "oversized SVG unexpectedly succeeded"
    exit 0
}

grep -E 'pixel limit|dimensions exceed limit|integer overflow' "${err_path}" >/dev/null || {
    echo "not ok" 1 - "oversized SVG failure did not report a size-limit reason"
    exit 0
}

echo "ok" 1 - "librsvg rejects oversized canvases"
exit 0
