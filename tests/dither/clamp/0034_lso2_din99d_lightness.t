#!/bin/sh
# Verify LSO2 din99d lightness with unchanged extended chroma using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0034_lso2_din99d_lightness" || {
    printf '\n%s\n' "not ok 1 - lso2_din99d_lightness"
    exit 0
}
printf '\n%s\n' "ok 1 - lso2_din99d_lightness"
exit 0
