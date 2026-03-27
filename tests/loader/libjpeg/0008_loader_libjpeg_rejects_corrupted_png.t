#!/bin/sh
# TAP test confirming forced libjpeg loader rejects corrupted PNG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg! "${input_png}" >/dev/null && {
    echo "not ok" 1 - "forced libjpeg loader accepted corrupted PNG"
    exit 0
}

echo "ok" 1 - "forced libjpeg loader rejects corrupted PNG"
exit 0
