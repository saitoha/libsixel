#!/bin/sh
# A repeat command survives a packet boundary.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0119_run_repeat_packet" || {
    echo "not ok 1 - run repeat packet"
    exit 0
}
echo "ok 1 - run repeat packet"
exit 0
