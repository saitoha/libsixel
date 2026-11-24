#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Policy: docs/encoder/palette-application/clamp-policy.md
# Verify no clamp policy promotes byte diffusion to float arithmetic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 7 1>&2 || {
    printf '\n%s\n' "not ok 1 - clamp keeps byte application"
    exit 0
}
printf '\n%s\n' "ok 1 - clamp keeps byte application"
exit 0
