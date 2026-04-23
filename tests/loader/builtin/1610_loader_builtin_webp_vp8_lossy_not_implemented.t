#!/bin/sh
# TAP test confirming builtin loader rejects lossy VP8 WebP.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" >/dev/null && {
    echo "not ok" 1 - "forced builtin loader accepted lossy VP8 WebP"
    exit 0
}

echo "ok" 1 - "forced builtin loader rejects lossy VP8 WebP"
exit 0
