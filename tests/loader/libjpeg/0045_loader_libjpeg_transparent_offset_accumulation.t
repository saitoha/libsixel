#!/bin/sh
# Verify transparent-offset accumulation accepts libjpeg float frame views.

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

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libjpeg! -p 16 -+ 3,3 "${input_jpeg}" >/dev/null || {
    echo "not ok 1 - libjpeg transparent-offset encode failed"
    exit 0
}

echo "ok 1 - libjpeg transparent-offset accumulation accepts frame pixels"
exit 0
