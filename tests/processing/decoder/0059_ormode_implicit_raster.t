#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Without raster attributes, wide planes grow and crop to painted bounds.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0059_ormode_implicit_raster" || {
    echo "not ok 1 - ormode_implicit_raster"
    exit 0
}

echo "ok 1 - ormode_implicit_raster"
exit 0
