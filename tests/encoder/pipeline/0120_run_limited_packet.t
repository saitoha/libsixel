#!/bin/sh
# A limited repeat command and remainder survive a packet boundary.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0120_run_limited_packet" || {
    echo "not ok 1 - run limited packet"
    exit 0
}
echo "ok 1 - run limited packet"
exit 0
