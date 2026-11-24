#!/bin/sh
# Selecting a PNG filename suffix after delta leaves output state unchanged.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0015_delta_delta_then_suffix || {
    printf '\n%s\n' "not ok 1 - PNG delta delta then suffix"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG delta delta then suffix"
exit 0
