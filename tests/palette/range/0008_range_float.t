#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# A narrow float interval is refined without absorbing a highlight.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0008_range_float" || {
    printf '\n%s\n' "not ok 1 - 0008_range_float"
    exit 0
}
printf '\n%s\n' "ok 1 - 0008_range_float"
exit 0
