#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve a named SIXEL destination on a final output ICC conflict.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled'
    exit 0
}

echo '1..1'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
target="${ARTIFACT_LOCAL_DIR}/output.six"
profile="icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-lut.icc"
printf '%s' 'existing capture' >"$target"
status=0
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wlinear -Goff -dnone -p4 '-~none' '-_none' \
    -Zdelta:P1 -U "$profile" -o "$target" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    2>&1 >"${ARTIFACT_LOCAL_DIR}/stdout") || status=$?
test "$status" -eq 2 && test ! -s "${ARTIFACT_LOCAL_DIR}/stdout" &&
    test "$(cat "$target"; printf .)" = 'existing capture.' || {
    printf '\n%s\n' 'not ok 1 - final ICC rejection preserves the output file'
    exit 0
}
case "$trace" in
    *"Progressive repair does not support ICC output (-U icc)."*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - final ICC rejection diagnostic'
    exit 0
}
printf '\n%s\n' 'ok 1 - final ICC rejection preserves the output file'
exit 0
