#!/bin/sh
# Verify standalone perceptual allocation recovery.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "72" || {
    echo "not ok 1 - standalone perceptual allocation recovery"
    exit 0
}
echo "ok 1 - standalone perceptual allocation recovery"
exit 0
