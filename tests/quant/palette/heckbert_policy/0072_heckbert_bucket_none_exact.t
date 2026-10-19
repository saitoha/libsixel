#!/bin/sh
# Lookup none retains exact source colors and populations.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_none_exact >/dev/null || {
    echo "not ok 1 - bucket_none_exact"
    exit 0
}

echo "ok 1 - bucket_none_exact"
exit 0
