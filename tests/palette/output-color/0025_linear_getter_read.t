#!/bin/sh
# Verify standalone linear getter read.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "26" || {
    printf '\n%s\n' "not ok 1 - standalone linear getter read"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone linear getter read"
exit 0
