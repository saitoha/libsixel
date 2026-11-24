#!/bin/sh
# Selecting delta after a PNG stdout target preserves destination and history on rejection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0010_delta_stdout_then_delta || {
    printf '\n%s\n' "not ok 1 - PNG delta stdout then delta"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG delta stdout then delta"
exit 0
