#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify palette promotion none without image-quality metrics.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 0 1>&2 || {
    echo "not ok 1 - palette promotion none"
    exit 0
}
echo "ok 1 - palette promotion none"
exit 0
