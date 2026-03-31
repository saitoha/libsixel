#!/bin/sh
# TAP test: quicklook reads MP4 as a still image.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${SIXEL_TEST_HOST_ARCH-}" != "x86_64" || {
    printf "1..0 # SKIP quicklook coverage is unstable on x86_64 for this input\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/quicklook/sample.mp4" >/dev/null || {
    echo "not ok" 1 - "quicklook MP4 still-frame decode failed"
    exit 0
}

echo "ok" 1 - "quicklook reads MP4 as still image"
exit 0
