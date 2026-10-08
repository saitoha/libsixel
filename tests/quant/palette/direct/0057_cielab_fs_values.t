#!/bin/sh
# Verify standalone cielab fs values.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "57" || {
    echo "not ok 1 - standalone cielab fs values"
    exit 0
}
echo "ok 1 - standalone cielab fs values"
exit 0
