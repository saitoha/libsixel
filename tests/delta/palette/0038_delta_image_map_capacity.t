#!/bin/sh
# Verify delta image map capacity.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0051_delta_image_map_capacity \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/gray256.ppm" || {
    echo "not ok 1 - delta image map capacity"
    exit 0
}
echo "ok 1 - delta image map capacity"
exit 0
