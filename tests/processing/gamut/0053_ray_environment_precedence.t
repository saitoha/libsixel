#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

SIXEL_OUTPUT_COLORSPACE_GAMUT=ray-trace ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0053_ray_environment_precedence || {
    echo "not ok 1 - environment precedence"
    exit 0
}
echo "ok 1 - environment precedence"
exit 0
