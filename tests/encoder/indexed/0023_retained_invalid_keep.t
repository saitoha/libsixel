#!/bin/sh
# Keeping an uninitialized hole leaves it invalid.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0023_retained_invalid_keep" || {
    printf '\n%s\n' 'not ok 1 - retained_invalid_keep'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_invalid_keep'
exit 0
