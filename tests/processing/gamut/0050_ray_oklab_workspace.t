#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

SIXEL_THREADING_POLICY=1 SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0050_ray_oklab_workspace || {
    echo "not ok 1 - oklab workspace"
    exit 0
}
echo "ok 1 - oklab workspace"
exit 0
