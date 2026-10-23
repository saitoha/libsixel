#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

SIXEL_THREADING_POLICY=1 SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0048_ray_full_frame_samples || {
    echo "not ok 1 - full frame samples"
    exit 0
}
echo "ok 1 - full frame samples"
exit 0
