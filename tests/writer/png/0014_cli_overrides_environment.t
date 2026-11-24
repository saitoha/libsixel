#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf '\n%s\n' "ok 1 # SKIP sixel2png disabled"
    exit 0
}
SIXEL_DECODER_INPUT_COLORSPACE=display-p3 ${SIXEL_RUNTIME-} \
    "${SIXEL2PNG_PATH}" -T srgb \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0011_cli_overrides_environment" || {
    printf '\n%s\n' "not ok 1 - cli_overrides_environment"
    exit 0
}
printf '\n%s\n' "ok 1 - cli_overrides_environment"
exit 0
