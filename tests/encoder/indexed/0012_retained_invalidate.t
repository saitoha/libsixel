#!/bin/sh
# Invalidation clears validity without inventing replacement RGB.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0012_retained_invalidate" || {
    printf '\n%s\n' 'not ok 1 - retained_invalidate'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_invalidate'
exit 0
