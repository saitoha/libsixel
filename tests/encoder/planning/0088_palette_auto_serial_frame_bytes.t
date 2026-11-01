#!/bin/sh
# Verify serial auto/frame bytes on generated RGB loader sessions.
# Policy: docs/cli/threading-policy.md
set -eux

test "${HAVE_IMG2SIXEL-0}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# SIXEL has no NUL bytes. A sentinel preserves trailing
# newlines, allowing exact comparison without artifacts.
reference=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -g -Lbuiltin! \
    "-=lazy:B1:palette=frame:load=serial:prepare=ahead" \
    -Z "delta" --precision-policy="float32" \
    -d fs --lookup-policy=none -4 full-frame \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    && printf '.') || {
    echo "not ok 1 - frame conversion failed: lazy/delta/float32/serial/ahead"
    exit 0
}
actual=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -g -Lbuiltin! \
    "-=lazy:B1:palette=auto:load=serial:prepare=ahead" \
    -Z "delta" --precision-policy="float32" \
    -d fs --lookup-policy=none -4 full-frame \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    && printf '.') || {
    echo "not ok 1 - auto conversion failed: lazy/delta/float32/serial/ahead"
    exit 0
}
test "${reference}" != '.' || {
    echo "not ok 1 - serial reference is empty"
    exit 0
}
test "${reference}" = "${actual}" || {
    echo "not ok 1 - serial auto differs: lazy/delta/float32/serial/ahead"
    exit 0
}
echo "ok 1 - serial auto matches frame bytes for lazy float delta animation"
exit 0
