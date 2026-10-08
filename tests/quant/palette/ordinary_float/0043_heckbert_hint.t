#!/bin/sh
# Typed Heckbert input overrides the byte preference.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_hint" || {
    echo "not ok 1 - Typed Heckbert input overrides the byte preference"
    exit 0
}
echo "ok 1 - Typed Heckbert input overrides the byte preference"
exit 0
