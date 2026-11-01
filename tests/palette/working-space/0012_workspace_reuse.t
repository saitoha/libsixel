#!/bin/sh
# Verify float clustering workspace reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0012_workspace_reuse || {
    echo "not ok 1 - workspace reuse"
    exit 0
}
echo "ok 1 - workspace reuse"
exit 0
