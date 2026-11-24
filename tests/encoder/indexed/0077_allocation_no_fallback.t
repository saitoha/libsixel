#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/allocation_no_fallback || {
    printf '\n%s\n' "not ok 1 - allocation no fallback"
    exit 0
}
printf '\n%s\n' "ok 1 - allocation no fallback"
exit 0
