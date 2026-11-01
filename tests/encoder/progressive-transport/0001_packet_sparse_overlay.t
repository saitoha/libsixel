#!/bin/sh
# Verify packet sparse overlay.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0069_packet_sparse_overlay" || {
    echo "not ok 1 - packet sparse overlay"
    exit 0
}
echo "ok 1 - packet sparse overlay"
exit 0
