#!/bin/sh
# Presentation marks identify accepted paint only.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0004_retained_paint_marks" || {
    echo 'not ok 1 - retained_paint_marks'
    exit 0
}
echo 'ok 1 - retained_paint_marks'
exit 0
