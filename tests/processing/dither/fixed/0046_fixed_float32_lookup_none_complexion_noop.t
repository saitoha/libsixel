#!/bin/sh
# TAP test covering fixed float32 dither with lookup none and deprecated -C.
#
# Flow:
# - Convert the snake reference with float32 precision and no LUT lookup.
# - Pass a non-default complexion score to verify the deprecated no-op path.
# - Compare exact output bytes against the default invocation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# The deprecated complexion option must leave the encoded bytes unchanged.
# A small fixture is sufficient because this is an option no-op contract.
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/complexion.six"
default_sixel="${ARTIFACT_LOCAL_DIR}/default.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs --precision=float32 \
        --lookup-policy=none -C 8 -p 16 "${input_image}" >"${output_sixel}" || {
    echo "not ok 1 - deprecated complexion conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs --precision=float32 \
        --lookup-policy=none -p 16 "${input_image}" >"${default_sixel}" || {
    echo "not ok 1 - default complexion conversion failed"
    exit 0
}
test -s "${output_sixel}" || {
    echo "not ok 1 - encoded output is empty"
    exit 0
}
cmp -s "${output_sixel}" "${default_sixel}" || {
    echo "not ok 1 - deprecated complexion option changed encoded bytes"
    exit 0
}
echo "ok 1 - deprecated complexion option preserves encoded bytes"
exit 0
