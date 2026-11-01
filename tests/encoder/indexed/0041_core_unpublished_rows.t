#!/bin/sh
# The body encoder refuses rows the producer has not published.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0041_core_unpublished_rows" || {
    echo 'not ok 1 - core_unpublished_rows'
    exit 0
}
echo 'ok 1 - core_unpublished_rows'
exit 0
