#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf '\n%s\n' "ok 1 # SKIP sixel2png disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -T display-p3 -d lso_undither:Vlight \
    <"${TOP_SRCDIR}/tests/writer/png/uniform.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0022_p3_lso_light_coordinates" || {
    printf '\n%s\n' "not ok 1 - p3_lso_light_coordinates"
    exit 0
}
printf '\n%s\n' "ok 1 - p3_lso_light_coordinates"
exit 0
