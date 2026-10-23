#!/bin/sh
# Reversible bucket means retain the reversible tone grid.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_reversible_mean >/dev/null || {
    echo "not ok 1 - bucket_reversible_mean"
    exit 0
}

echo "ok 1 - bucket_reversible_mean"
exit 0
