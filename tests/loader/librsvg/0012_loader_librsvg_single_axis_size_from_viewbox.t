#!/bin/sh
# TAP test confirming librsvg derives the missing axis from viewBox ratio.

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

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-single-axis-viewbox.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-single-axis-viewbox.six"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='40' viewBox='0 0 120 30'><rect x='0' y='0' width='120' height='30' fill='#00ff00'/></svg>" >"${svg_path}"

run_img2sixel -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "single-axis viewBox conversion failed"
    exit 0
}

sed 's/^.*"//;s/#.*$//' "${sixel_path}" | grep -q '^1;1;40;10$' || {
    echo "not ok" 1 - "missing-axis geometry was not resolved to 40x10"
    exit 0
}

echo "ok" 1 - "librsvg resolves missing axis from viewBox ratio"
exit 0
