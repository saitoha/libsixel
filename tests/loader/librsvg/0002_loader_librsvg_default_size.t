#!/bin/sh
# TAP test confirming librsvg fallback geometry is 300x150 without size hints.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-default-size.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-default-size.six"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg'><rect x='0' y='0' width='10' height='10' fill='#ff0000'/></svg>" >"${svg_path}"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg default-size conversion failed"
    exit 0
}

sed 's/^.*"//;s/#.*$//' "${sixel_path}" | grep -q '^1;1;300;150$' || {
    echo "not ok" 1 - "fallback geometry is not 300x150"
    exit 0
}

echo "ok" 1 - "librsvg fallback geometry is 300x150"
exit 0
