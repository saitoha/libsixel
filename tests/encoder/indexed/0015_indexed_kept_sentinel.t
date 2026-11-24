#!/bin/sh
# A skipped sentinel is never interpreted as a palette index.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0015_indexed_kept_sentinel" || {
    printf '\n%s\n' 'not ok 1 - indexed_kept_sentinel'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_kept_sentinel'
exit 0
