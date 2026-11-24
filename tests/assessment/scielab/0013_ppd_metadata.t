#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none --ppd=224 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" -v28.5) || {
    printf '\n%s\n' "not ok 1 - scielab viewing metadata"
    exit 0
}
case "${value}" in
    *'"ppd": 28.5'*'"scielab_model": "zhang-wandell-1998-unit-dc"'*'"scielab_difference": "CIE76"'*) ;;
    *) printf '\n%s\n' "not ok 1 - scielab viewing metadata"; exit 0 ;;
esac

printf '\n%s\n' "ok 1 - scielab viewing metadata and precedence"
exit 0
