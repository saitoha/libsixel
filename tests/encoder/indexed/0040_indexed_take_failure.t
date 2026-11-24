#!/bin/sh
# Failed import leaves both source allocations with the caller.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0040_indexed_take_failure" || {
    printf '\n%s\n' 'not ok 1 - indexed_take_failure'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_take_failure'
exit 0
