#!/bin/sh
# HLS encode failure and successful reuse clear the call-scoped comparison hint.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0173_delta_hls_failure_recovery || {
    printf '\n%s\n' "not ok 1 - delta hls failure recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - delta hls failure recovery"
exit 0
