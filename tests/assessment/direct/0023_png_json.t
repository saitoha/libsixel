#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# JSON reports png drawing selection and both input routes.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -D -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' "not ok 1 - JSON evaluation failed"
    exit 0
}
case "${value}" in *'"requested_sixel_drawing": "direct"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - drawing selection missing"
    exit 0
}
reference=${value#*'"drawing": "not-applicable"'}
case "${value}" in *'"drawing": "not-applicable"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - reference drawing missing"
    exit 0
}
case "${reference}" in *'"drawing": "not-applicable"'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target drawing missing"
    exit 0
}
printf '\n%s\n' "ok 1 - png json"
exit 0
