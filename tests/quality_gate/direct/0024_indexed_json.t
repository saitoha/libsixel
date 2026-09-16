#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# JSON reports indexed drawing selection and both input routes.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -Rsrgb -Tsrgb "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    echo "not ok 1 - JSON evaluation failed"
    exit 0
}
test "${value#*'"requested_sixel_drawing": "indexed"'}" != "$value" || {
    echo "not ok 1 - drawing selection missing"
    exit 0
}
reference=${value#*'"drawing": "indexed"'}
test "$reference" != "$value" || {
    echo "not ok 1 - reference drawing missing"
    exit 0
}
test "${reference#*'"drawing": "indexed"'}" != "$reference" || {
    echo "not ok 1 - target drawing missing"
    exit 0
}
echo "ok 1 - indexed json"
exit 0
