#!/bin/sh
# Preserve extended Heckbert sources in five spaces.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_sources" || {
    echo "not ok 1 - Preserve extended Heckbert sources in five spaces"
    exit 0
}
echo "ok 1 - Preserve extended Heckbert sources in five spaces"
exit 0
