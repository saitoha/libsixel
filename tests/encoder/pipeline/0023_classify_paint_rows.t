#!/bin/sh
# Verify classify paint rows.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0051_classify_paint_rows" || {
    printf '\n%s\n' "not ok 1 - classify paint rows"
    exit 0
}

printf '\n%s\n' "ok 1 - classify paint rows"
exit 0
