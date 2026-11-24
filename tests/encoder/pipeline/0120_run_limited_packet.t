#!/bin/sh
# A limited repeat command and remainder survive a packet boundary.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0120_run_limited_packet" || {
    printf '\n%s\n' "not ok 1 - run limited packet"
    exit 0
}
printf '\n%s\n' "ok 1 - run limited packet"
exit 0
