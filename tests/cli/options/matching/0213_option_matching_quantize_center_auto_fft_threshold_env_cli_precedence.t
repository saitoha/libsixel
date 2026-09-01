#!/bin/sh
# TAP test verifying center auto_fft_threshold keeps CLI priority over env.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD=1024" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center auto_fft_threshold=1024 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:auto_fft_threshold=4096 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center auto_fft_threshold=4096 was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD=1024" \
    -Qcenter:auto_fft_threshold=70000 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI auto_fft_threshold unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*auto_fft_threshold*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center auto_fft_threshold diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD=70000" \
    -Qcenter:auto_fft_threshold=2048 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center auto_fft_threshold did not override invalid env"
    exit 0
}

echo "ok" 1 - "center auto_fft_threshold follows env/CLI acceptance and CLI priority"
exit 0
