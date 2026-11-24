#!/bin/sh
# PNG-selected byte frames reject delta before palette work; full reuse paints exact white and black endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0019_delta_byte_recovery || {
    printf '\n%s\n' "not ok 1 - PNG delta byte recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG delta byte recovery"
exit 0
