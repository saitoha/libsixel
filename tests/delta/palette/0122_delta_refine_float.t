#!/bin/sh
# Verify refine float.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/delta_native/0235_delta_refine_float || {
    printf '\n%s\n' "not ok 1 - refine_float"
    exit 0
}
printf '\n%s\n' "ok 1 - refine_float"
exit 0
