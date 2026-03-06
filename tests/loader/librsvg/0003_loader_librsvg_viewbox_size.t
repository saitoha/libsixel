#!/bin/sh
# TAP test confirming librsvg uses viewBox geometry when size is omitted.

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

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-viewbox-size.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-viewbox-size.six"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 40 25'><rect x='0' y='0' width='40' height='25' fill='#00ff00'/></svg>" >"${svg_path}"

echo "1..1"
set -v

run_img2sixel -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg viewBox conversion failed"
    exit 0
}

sed 's/^.*"//;s/#.*$//' "${sixel_path}" | grep -q '^1;1;40;25$' || {
    echo "not ok" 1 - "viewBox geometry is not 40x25"
    exit 0
}

echo "ok" 1 - "librsvg viewBox geometry is respected"
exit 0
