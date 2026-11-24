#!/bin/sh
# Verify delta p3 diffuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0107_delta_p3_diffuse || {
    printf '\n%s\n' "not ok 1 - delta p3 diffuse"
    exit 0
}
printf '\n%s\n' "ok 1 - delta p3 diffuse"
exit 0
