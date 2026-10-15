#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Decode an OR-mode stream with SIMD disabled and enabled.
set -eux
test -x "${IMG2SIXEL_PATH}" || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
stream="${ARTIFACT_LOCAL_DIR}/decoder_0107.six"
scalar_png="${ARTIFACT_LOCAL_DIR}/decoder_0107_none.png"
vector_png="${ARTIFACT_LOCAL_DIR}/decoder_0107_auto.png"

# The odd width leaves a scalar tail after the sixteen-column blocks.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -O -w181 -o "${stream}" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.jpg" || {
    echo "not ok 1 - img2sixel failed"
    exit 0
}
SIXEL_THREADS=1 SIXEL_SIMD_LEVEL=none \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" <"${stream}" >"${scalar_png}" || {
    echo "not ok 1 - scalar decode failed"
    exit 0
}
SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" <"${stream}" >"${vector_png}" || {
    echo "not ok 1 - decode at the detected SIMD level failed"
    exit 0
}
cmp "${scalar_png}" "${vector_png}" || {
    echo "not ok 1 - OR-mode decode differs between SIMD levels"
    exit 0
}
echo "ok 1 - OR-mode decode is identical at every SIMD level"
exit 0
