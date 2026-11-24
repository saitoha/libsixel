#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/ordered_completed_reuse || {
    printf '\n%s\n' "not ok 1 - ordered completed reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered completed reuse"
exit 0
