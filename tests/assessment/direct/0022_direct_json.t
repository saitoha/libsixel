#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# JSON reports direct drawing selection and both input routes.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -D "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    printf '\n%s\n' "not ok 1 - JSON evaluation failed"
    exit 0
}
case "${value}" in *'"requested_sixel_drawing": "direct"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - drawing selection missing"
    exit 0
}
reference=${value#*'"drawing": "direct"'}
case "${value}" in *'"drawing": "direct"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - reference drawing missing"
    exit 0
}
case "${reference}" in *'"drawing": "direct"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target drawing missing"
    exit 0
}
printf '\n%s\n' "ok 1 - direct json"
exit 0
