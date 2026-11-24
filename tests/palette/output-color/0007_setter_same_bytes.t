#!/bin/sh
# Verify standalone float palette setter same bytes.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "7" || {
    printf '\n%s\n' "not ok 1 - standalone palette setter same bytes"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone palette setter same bytes"
exit 0
