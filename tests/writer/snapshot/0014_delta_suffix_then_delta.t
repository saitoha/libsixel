#!/bin/sh
# Selecting delta after a PNG filename suffix preserves the configured path and history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0014_delta_suffix_then_delta || {
    printf '\n%s\n' "not ok 1 - PNG delta suffix then delta"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG delta suffix then delta"
exit 0
