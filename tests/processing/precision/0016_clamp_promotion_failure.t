#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify clamp promotion failure without image-quality metrics.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 7 1>&2 || {
    echo "not ok 1 - clamp promotion failure"
    exit 0
}
echo "ok 1 - clamp promotion failure"
exit 0
