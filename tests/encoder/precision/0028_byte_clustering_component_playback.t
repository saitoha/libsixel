#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify independent playback emits every frame with a byte request and a
# non-gamma clustering space, whose capture stage has no byte adapter.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0027_byte_clustering_freeze 1 1>&2 || {
    echo "not ok 1 - byte clustering component playback"
    exit 0
}
echo "ok 1 - byte clustering component playback"
exit 0
