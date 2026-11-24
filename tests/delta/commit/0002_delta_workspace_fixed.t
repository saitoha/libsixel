#!/bin/sh
# Verify delta workspace fixed.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0092_delta_workspace_fixed || {
    printf '\n%s\n' "not ok 1 - delta workspace fixed"
    exit 0
}
printf '\n%s\n' "ok 1 - delta workspace fixed"
exit 0
