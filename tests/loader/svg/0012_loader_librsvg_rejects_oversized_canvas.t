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

echo "1..1"
set -v

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-oversized-canvas.svg"
status=0
msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" -o/dev/null 2>&1
) || status="$?"

test "${status}" -ne 0 || {
    echo "not ok" 1 - "oversized SVG unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"pixel limit"* | *"dimensions exceed limit"* | *"integer overflow"*)
        ;;
    *)
        echo "not ok" 1 - "oversized SVG failure did not report a size-limit reason"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg rejects oversized canvases"
exit 0
