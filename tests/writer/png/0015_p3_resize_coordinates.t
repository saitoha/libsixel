#!/bin/sh
# Policy: docs/writers/png.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP sixel2png disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -T display-p3 -s 3 \
    <"${TOP_SRCDIR}/tests/writer/png/uniform.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0015_p3_resize_coordinates" || {
    echo "not ok 1 - p3_resize_coordinates"
    exit 0
}
echo "ok 1 - p3_resize_coordinates"
exit 0
