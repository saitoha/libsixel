#!/bin/sh
# Verify delta map origin.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0039_delta_map_origin \
    "pal:${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal" || {
    printf '\n%s\n' "not ok 1 - delta map origin"
    exit 0
}
printf '\n%s\n' "ok 1 - delta map origin"
exit 0
