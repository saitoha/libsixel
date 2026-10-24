#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Decode partial transparent coverage with SIMD disabled and enabled.
set -eux
test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
stream="${ARTIFACT_LOCAL_DIR}/decoder_0108.six"
scalar_png="${ARTIFACT_LOCAL_DIR}/decoder_0108_none.png"
vector_png="${ARTIFACT_LOCAL_DIR}/decoder_0108_auto.png"

# P2=1 keeps unpainted pixels transparent; the runs cover sixteen-column
# blocks partially, fully, and not at all.
printf '\033P0;1q"1;1;40;12#1;2;100;0;0#2;2;0;100;0' >"${stream}"
printf '#1!20~#2@A?BC!8?DE$#2!17?NN!5~-#1A!30?~~$#2!40N\033\134' \
    >>"${stream}"
SIXEL_THREADING_GLOBAL_BUDGET=1 SIXEL_SIMD_LEVEL=none \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D <"${stream}" >"${scalar_png}" || {
    echo "not ok 1 - scalar decode failed"
    exit 0
}
SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D <"${stream}" >"${vector_png}" || {
    echo "not ok 1 - decode at the detected SIMD level failed"
    exit 0
}
cmp "${scalar_png}" "${vector_png}" || {
    echo "not ok 1 - transparent decode differs between SIMD levels"
    exit 0
}
echo "ok 1 - transparent decode is identical at every SIMD level"
exit 0
