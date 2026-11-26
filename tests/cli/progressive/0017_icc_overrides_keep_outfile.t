#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Open a valid final destination once, including across multiple inputs.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled'
    exit 0
}

echo '1..1'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
profile="icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-lut.icc"
input="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
target="${ARTIFACT_LOCAL_DIR}/output.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wlinear -Goff -dnone -p4 '-~none' '-_none' \
    -U "$profile" -Zfull "$input" "$input" >"${ARTIFACT_LOCAL_DIR}/expected" || {
    printf '\n%s\n' 'not ok 1 - reference conversion failed'
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wlinear -Goff -dnone -p4 '-~none' '-_none' \
    -U "$profile" -Zprogressive:N2:M0 -o "$target" -o - -Zfull \
    "$input" "$input" >"${ARTIFACT_LOCAL_DIR}/stdout" || {
    printf '\n%s\n' 'not ok 1 - deferred file conversion failed'
    exit 0
}
test ! -s "${ARTIFACT_LOCAL_DIR}/stdout" && test -s "$target" || {
    printf '\n%s\n' 'not ok 1 - deferred file uses the selected destination'
    exit 0
}
cmp -s "${ARTIFACT_LOCAL_DIR}/expected" "$target" || {
    printf '\n%s\n' 'not ok 1 - deferred file contains both complete images'
    exit 0
}
printf '\n%s\n' 'ok 1 - deferred file contains both complete images'
exit 0
