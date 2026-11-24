#!/bin/sh
# Completion requires every source row to be published.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0017_indexed_early_complete" || {
    printf '\n%s\n' 'not ok 1 - indexed_early_complete'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_early_complete'
exit 0
