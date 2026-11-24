#!/bin/sh
# Abort leaves the previous committed bytes intact.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0011_retained_abort" || {
    printf '\n%s\n' 'not ok 1 - retained_abort'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_abort'
exit 0
