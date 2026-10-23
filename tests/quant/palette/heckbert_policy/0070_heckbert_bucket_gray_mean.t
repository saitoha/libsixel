#!/bin/sh
# The generic one-channel path retains fractional means.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_gray_mean >/dev/null || {
    echo "not ok 1 - bucket_gray_mean"
    exit 0
}

echo "ok 1 - bucket_gray_mean"
exit 0
