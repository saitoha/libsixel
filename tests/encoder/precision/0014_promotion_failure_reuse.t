#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify promotion failure reuse without image-quality metrics.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 5 1>&2 || {
    echo "not ok 1 - promotion failure reuse"
    exit 0
}
echo "ok 1 - promotion failure reuse"
exit 0
