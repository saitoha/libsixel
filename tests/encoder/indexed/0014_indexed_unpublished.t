#!/bin/sh
# Unpublished rows cannot be consumed.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0014_indexed_unpublished" || {
    printf '\n%s\n' 'not ok 1 - indexed_unpublished'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_unpublished'
exit 0
