#!/bin/sh
# Typed float input determines arithmetic despite a byte preference.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/medoids_hint" || {
    echo "not ok 1 - Typed float input determines arithmetic despite a byte preference"
    exit 0
}
echo "ok 1 - Typed float input determines arithmetic despite a byte preference"
exit 0
