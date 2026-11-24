#!/bin/sh
# A repeat command survives a packet boundary.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0119_run_repeat_packet" || {
    printf '\n%s\n' "not ok 1 - run repeat packet"
    exit 0
}
printf '\n%s\n' "ok 1 - run repeat packet"
exit 0
