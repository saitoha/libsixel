#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Keep unpainted OR index zero opaque through this dequantizer.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0027_ormode_kundither_opaque" || {
    echo "not ok 1 - ormode_kundither_opaque"
    exit 0
}

echo "ok 1 - ormode_kundither_opaque"
exit 0
