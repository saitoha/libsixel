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

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${ARTIFACT_LOCAL_DIR}/librsvg-size-overrides-viewbox.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-size-overrides-viewbox.six"

printf '%s' "<svg xmlns='http://www.w3.org/2000/svg' width='19' height='11' viewBox='0 0 95 55'><rect x='0' y='0' width='95' height='55' fill='#ff00ff'/></svg>" >"${svg_path}"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg size+viewBox conversion failed"
    exit 0
}

sed 's/^.*"//;s/#.*$//' "${sixel_path}" | grep -q '^1;1;19;11$' || {
    echo "not ok" 1 - "size attributes did not override viewBox geometry"
    exit 0
}

echo "ok" 1 - "librsvg size attributes override viewBox geometry"
exit 0
