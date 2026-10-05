#!/bin/sh
# Verify delta map reuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0047_delta_map_reuse \
    "pal:${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal" || {
    echo "not ok 1 - delta map reuse"
    exit 0
}
echo "ok 1 - delta map reuse"
exit 0
