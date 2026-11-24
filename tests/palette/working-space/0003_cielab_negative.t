#!/bin/sh
# Verify float clustering cielab negative.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0003_cielab_negative || {
    printf '\n%s\n' "not ok 1 - cielab negative"
    exit 0
}
printf '\n%s\n' "ok 1 - cielab negative"
exit 0
