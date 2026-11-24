#!/bin/sh
# An exact keep preserves the committed wire color.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0024_retained_exact_keep" || {
    printf '\n%s\n' 'not ok 1 - retained_exact_keep'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_exact_keep'
exit 0
