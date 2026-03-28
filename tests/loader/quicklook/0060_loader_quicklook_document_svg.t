#!/bin/sh
# TAP test: quicklook renders SVG document input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/quicklook/sample.svg" >/dev/null || {
    echo "not ok" 1 - "quicklook SVG render failed"
    exit 0
}

echo "ok" 1 - "quicklook renders SVG"
exit 0
