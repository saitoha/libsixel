#!/bin/sh
# The PNG stdout frontend rejects delta before input/staging access and preserves its descriptor and history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0023_delta_stdout_guard || {
    echo "not ok 1 - PNG delta stdout guard"
    exit 0
}
echo "ok 1 - PNG delta stdout guard"
exit 0
