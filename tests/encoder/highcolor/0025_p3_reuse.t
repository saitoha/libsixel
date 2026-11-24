#!/bin/sh
# Output definition and byte or float input switch safely between encoder contexts.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0025_p3_reuse || {
    printf '\n%s\n' "not ok 1 - high-color p3 reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color p3 reuse"
exit 0
