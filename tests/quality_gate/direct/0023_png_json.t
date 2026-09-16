#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# JSON reports png drawing selection and both input routes.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -D -L builtin "${TOP_SRCDIR}/images/snake.png" "${TOP_SRCDIR}/images/snake.png") || {
    echo "not ok 1 - JSON evaluation failed"
    exit 0
}
test "${value#*'"requested_sixel_drawing": "direct"'}" != "$value" || {
    echo "not ok 1 - drawing selection missing"
    exit 0
}
reference=${value#*'"drawing": "not-applicable"'}
test "$reference" != "$value" || {
    echo "not ok 1 - reference drawing missing"
    exit 0
}
test "${reference#*'"drawing": "not-applicable"'}" != "$reference" || {
    echo "not ok 1 - target drawing missing"
    exit 0
}
echo "ok 1 - png json"
exit 0
