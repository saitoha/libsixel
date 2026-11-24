#!/bin/sh
# An out-of-range painted index seals the producer as failed.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0016_indexed_invalid_paint" || {
    printf '\n%s\n' 'not ok 1 - indexed_invalid_paint'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_invalid_paint'
exit 0
