#!/bin/sh
# Verify native soft cover: explicit soft overrides env.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_PALETTE_COVER_MODE=hard SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 0 0 1 || {
    echo "not ok 1 - explicit soft overrides env"
    exit 0
}

echo "ok 1 - explicit soft overrides env"
exit 0
