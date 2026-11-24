#!/bin/sh
# Paint count excludes kept pixels.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0003_retained_paint_count" || {
    printf '\n%s\n' 'not ok 1 - retained_paint_count'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_paint_count'
exit 0
