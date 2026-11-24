#!/bin/sh
# Four-channel 5bit buckets retain fractional channel means.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_rgba_mean >/dev/null || {
    printf '\n%s\n' "not ok 1 - bucket_rgba_mean"
    exit 0
}

printf '\n%s\n' "ok 1 - bucket_rgba_mean"
exit 0
