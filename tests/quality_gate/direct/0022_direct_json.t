#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# JSON reports direct drawing selection and both input routes.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -D "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    echo "not ok 1 - JSON evaluation failed"
    exit 0
}
test "${value#*'"requested_sixel_drawing": "direct"'}" != "$value" || {
    echo "not ok 1 - drawing selection missing"
    exit 0
}
reference=${value#*'"drawing": "direct"'}
test "$reference" != "$value" || {
    echo "not ok 1 - reference drawing missing"
    exit 0
}
test "${reference#*'"drawing": "direct"'}" != "$reference" || {
    echo "not ok 1 - target drawing missing"
    exit 0
}
echo "ok 1 - direct json"
exit 0
