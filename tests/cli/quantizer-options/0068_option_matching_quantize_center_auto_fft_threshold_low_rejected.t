#!/bin/sh
# TAP test verifying center auto_fft_threshold rejects values below range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:auto_fft_threshold=255 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - out-of-range center auto_fft_threshold unexpectedly succeeded"
    exit 0
}

case "${msg}" in *auto_fft_threshold**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing center auto_fft_threshold range diagnostic"
    exit 0
}

printf '\n%s\n' "ok 1 - center auto_fft_threshold rejects values below range"
exit 0
