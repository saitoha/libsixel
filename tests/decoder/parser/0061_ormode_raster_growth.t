#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Plane words survive width and band growth beyond attributed dimensions.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0056_ormode_raster_growth" || {
    echo "not ok 1 - ormode_raster_growth"
    exit 0
}

echo "ok 1 - ormode_raster_growth"
exit 0
