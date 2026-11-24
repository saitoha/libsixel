#!/bin/sh
# Verify delta builtin reuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0046_delta_builtin_reuse || {
    printf '\n%s\n' "not ok 1 - delta builtin reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - delta builtin reuse"
exit 0
