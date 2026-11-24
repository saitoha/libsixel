#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify requested promotion hard without image-quality metrics.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 3 1>&2 || {
    printf '\n%s\n' "not ok 1 - requested promotion hard"
    exit 0
}
printf '\n%s\n' "ok 1 - requested promotion hard"
exit 0
