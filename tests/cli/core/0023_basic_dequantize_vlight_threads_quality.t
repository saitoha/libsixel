#!/bin/sh
# Verify Vlight thread-count output remains visually stable.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}
test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}
test -x "${LSQA_PATH}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"

echo "1..1"
set -v

thread1_png="${artifact_dir}/vlight-thread1-$$.png"
thread4_png="${artifact_dir}/vlight-thread4-$$.png"

SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        -D -dl:Vl \
        -i "${TOP_SRCDIR}/images/snake.six" \
        -o "${thread1_png}" || {
    echo "not ok" 1 - "single-thread fast4 decode failed"
    exit 0
}

SIXEL_THREADS=4 ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        -D -dl:Vl \
        -i "${TOP_SRCDIR}/images/snake.six" \
        -o "${thread4_png}" || {
    echo "not ok" 1 - "four-thread fast4 decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:0.999" \
        "${thread1_png}" "${thread4_png}" >/dev/null || {
    echo "not ok" 1 - "Vlight thread-count output fell below MS-SSIM 0.999"
    exit 0
}

echo "ok" 1 - "Vlight thread-count output stays above MS-SSIM 0.999"
exit 0
